// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A program's message functions (maul-locale/message.h): the table a
// formatter is made with and what it refuses; a call's options and
// locale through the accessors; operands of each kind; a function's
// faults, falling back; :string's operands; selection through match and
// better entries, and a selector that cannot select.

#include "test_harness.h"
#include "test_print.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static mlocLocale s_locale;
static mlocDataSet s_data;
static uint8_t s_compiled[1 << 18];
static uint8_t s_bound[1 << 18];
static char s_text[1 << 12];

// What echo resolves to: a short text it writes back.
typedef struct Echo
{
    char text[62];
    uint8_t length;
    bool failFormat;
} Echo;

static void Add(Echo* e, const char* text, size_t length)
{
    for (size_t i = 0; i < length && e->length < sizeof e->text; i++)
    {
        e->text[e->length++] = text[i];
    }
}

// An operand as echo shows it: "_" none, its text, "#" an argument,
// "N" and its digit count a number value, "@" and the function's index a
// resolved value, "?" a fallback.
static void AddOperand(Echo* e, const mlocMessageOperand* operand)
{
    char shown[32];
    switch (operand->kind)
    {
    case mloc_messageOperandNone:
        Add(e, "_", 1);
        break;
    case mloc_messageOperandText:
        Add(e, operand->text, operand->length);
        break;
    case mloc_messageOperandArgument:
        Add(e, "#", 1);
        break;
    case mloc_messageOperandNumber:
        snprintf(shown, sizeof shown, "N%d", operand->number->count);
        Add(e, shown, strlen(shown));
        break;
    case mloc_messageOperandCurrency:
        snprintf(shown, sizeof shown, "C%d%.*s", operand->number->count, (int)operand->length,
                 operand->text);
        Add(e, shown, strlen(shown));
        break;
    case mloc_messageOperandUnit:
        snprintf(shown, sizeof shown, "U%d%.*s", operand->number->count, (int)operand->length,
                 operand->text);
        Add(e, shown, strlen(shown));
        break;
    case mloc_messageOperandDate:
    case mloc_messageOperandTime:
    case mloc_messageOperandDateTime:
        snprintf(shown, sizeof shown, "%c%d:%u", "DTB"[operand->kind - mloc_messageOperandDate],
                 (int)operand->dateTime->date.day, (unsigned)operand->dateTime->time.hour);
        Add(e, shown, strlen(shown));
        if (operand->instant != NULL)
        {
            // A zoned value's instant and zone.
            char zone[64];
            mlocOutput out = {zone, sizeof zone, 0, NULL, NULL};
            snprintf(shown, sizeof shown, "z%lld", (long long)operand->instant->seconds);
            Add(e, shown, strlen(shown));
            if (mlocFormatTimeZoneId(operand->zone, &out) == mloc_success)
            {
                Add(e, zone, out.length);
            }
        }
        break;
    case mloc_messageOperandDuration:
        snprintf(shown, sizeof shown, "P%dh%dm", (int)operand->duration->hours,
                 (int)operand->duration->minutes);
        Add(e, shown, strlen(shown));
        break;
    case mloc_messageOperandList:
        snprintf(shown, sizeof shown, "L%zu", operand->itemCount);
        Add(e, shown, strlen(shown));
        if (operand->itemCount > 0)
        {
            Add(e, operand->items[0].text, operand->items[0].length);
        }
        break;
    case mloc_messageOperandResolved:
        snprintf(shown, sizeof shown, "@%u", (unsigned)operand->function);
        Add(e, shown, strlen(shown));
        break;
    default:
        Add(e, "?", 1);
        break;
    }
}

// x:echo: its operand, then each option as ";name=value"; "fail" as
// its operand is refused with a function fault, the option fail=format
// makes its format fail.
static size_t s_echoes;

static mlocMessageFaultKind EchoResolve(void* context, const mlocMessageCall* call,
                                        const mlocMessageOperand* operand,
                                        mlocMessageResolved* valueOut)
{
    (void)context;
    s_echoes++;
    if (operand->kind == mloc_messageOperandText && operand->length == 4 &&
        memcmp(operand->text, "fail", 4) == 0)
    {
        return mloc_messageFaultFunction;
    }
    Echo e;
    memset(&e, 0, sizeof e);
    AddOperand(&e, operand);
    size_t count = mlocMessageCall_GetOptionCount(call);
    for (size_t i = 0; i < count; i++)
    {
        const char* name;
        size_t length;
        mlocMessageOperand value;
        mlocMessageResolved storage;
        if (mlocMessageCall_GetOption(call, i, &name, &length, &value, &storage) != mloc_success)
        {
            return mloc_messageFaultFunction;
        }
        Add(&e, ";", 1);
        Add(&e, name, length);
        Add(&e, "=", 1);
        AddOperand(&e, &value);
    }
    mlocMessageOperand fail;
    mlocMessageResolved storage;
    e.failFormat = mlocMessageCall_FindOption(call, "fail", 4, &fail, &storage) == mloc_success;
    memset(valueOut, 0, sizeof *valueOut);
    memcpy(valueOut, &e, sizeof e);
    return mloc_messageFaultNone;
}

static mlocMessageFaultKind EchoFormat(void* context, const mlocMessageResolved* value,
                                       mlocOutput* output)
{
    (void)context;
    Echo e;
    memcpy(&e, value, sizeof e);
    if (e.failFormat)
    {
        return mloc_messageFaultFunction;
    }
    output->length = e.length;
    if (output->buffer != NULL)
    {
        memcpy(output->buffer, e.text, e.length < output->capacity ? e.length : output->capacity);
    }
    return mloc_messageFaultNone;
}

// x:any matches every key, or none when its operand is "none"; with
// the operand "broken" its match fails.
static mlocMessageFaultKind AnyMatch(void* context, const mlocMessageResolved* value,
                                     const char* key, size_t length, bool* matchOut)
{
    (void)context;
    (void)key;
    (void)length;
    Echo e;
    memcpy(&e, value, sizeof e);
    if (e.length == 6 && memcmp(e.text, "broken", 6) == 0)
    {
        return mloc_messageFaultUnsupportedOperation;
    }
    *matchOut = !(e.length == 4 && memcmp(e.text, "none", 4) == 0);
    return mloc_messageFaultNone;
}

// x:short prefers the shorter key.
static bool ShortBetter(void* context, const mlocMessageResolved* value, const char* key1,
                        size_t length1, const char* key2, size_t length2)
{
    (void)context;
    (void)value;
    (void)key1;
    (void)key2;
    return length1 < length2;
}

// x:locale writes the call's locale.
static mlocMessageFaultKind LocaleResolve(void* context, const mlocMessageCall* call,
                                          const mlocMessageOperand* operand,
                                          mlocMessageResolved* valueOut)
{
    (void)context;
    (void)operand;
    mlocLocale locale;
    Echo e;
    memset(&e, 0, sizeof e);
    char tag[32];
    mlocOutput out = {tag, sizeof tag, 0, NULL, NULL};
    if (mlocMessageCall_GetLocale(call, &locale) != mloc_success ||
        mlocFormatLocale(&locale, &out) != mloc_success)
    {
        return mloc_messageFaultFunction;
    }
    Add(&e, tag, out.length);
    memset(valueOut, 0, sizeof *valueOut);
    memcpy(valueOut, &e, sizeof e);
    return mloc_messageFaultNone;
}

// x:greedy reads its option k 40000 times: "ok", or "short" when a read
// fell back.
static mlocMessageFaultKind GreedyResolve(void* context, const mlocMessageCall* call,
                                          const mlocMessageOperand* operand,
                                          mlocMessageResolved* valueOut)
{
    (void)context;
    (void)operand;
    bool short_ = false;
    for (int i = 0; i < 40000; i++)
    {
        mlocMessageOperand value;
        mlocMessageResolved storage;
        short_ = short_ ||
                 mlocMessageCall_FindOption(call, "k", 1, &value, &storage) != mloc_success ||
                 value.kind == mloc_messageOperandFallback;
    }
    Echo e;
    memset(&e, 0, sizeof e);
    Add(&e, short_ ? "short" : "ok", short_ ? 5 : 2);
    memset(valueOut, 0, sizeof *valueOut);
    memcpy(valueOut, &e, sizeof e);
    return mloc_messageFaultNone;
}

// x:ltr and x:rtl give their values a direction; x:odd one out of range.
static mlocDirection LeftToRight(void* context, const mlocMessageResolved* value)
{
    (void)context;
    (void)value;
    return mloc_directionLeftToRight;
}

static mlocDirection RightToLeft(void* context, const mlocMessageResolved* value)
{
    (void)context;
    (void)value;
    return mloc_directionRightToLeft;
}

static mlocDirection Odd(void* context, const mlocMessageResolved* value)
{
    (void)context;
    (void)value;
    return (mlocDirection)7;
}

static const mlocMessageFunction s_functions[] = {
    {"x:echo", 6, NULL, EchoResolve, EchoFormat, NULL, NULL, NULL},
    {"x:any", 5, NULL, EchoResolve, NULL, AnyMatch, NULL, NULL},
    {"x:short", 7, NULL, EchoResolve, EchoFormat, AnyMatch, ShortBetter, NULL},
    {"x:locale", 8, NULL, LocaleResolve, EchoFormat, NULL, NULL, NULL},
    {"x:greedy", 8, NULL, GreedyResolve, EchoFormat, NULL, NULL, NULL},
    {"x:ltr", 5, NULL, EchoResolve, EchoFormat, NULL, NULL, LeftToRight},
    {"x:rtl", 5, NULL, EchoResolve, EchoFormat, NULL, NULL, RightToLeft},
    {"x:odd", 5, NULL, EchoResolve, EchoFormat, NULL, NULL, Odd},
};

enum
{
    functionCount = sizeof s_functions / sizeof s_functions[0],
};

static mlocMessageFormatter s_formatter;
// The formatter Run and BindOnly use.
static const mlocMessageFormatter* s_using = &s_formatter;

static void MakeFormatter(void)
{
    static mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, s_functions, functionCount,
                                  NULL};
    CHECK(mlocParseLocale("de-CH", 5, &s_locale).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &s_locale, &options, &s_formatter) == mloc_success,
          "a formatter with functions is made");
}

static bool Refused(const char* name)
{
    mlocMessageFunction function = {name, strlen(name), NULL, EchoResolve, NULL, NULL, NULL, NULL};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, &function, 1, NULL};
    mlocMessageFormatter formatter;
    return mlocInitMessageFormatter(&s_data, &s_locale, &options, &formatter) == mloc_errorInvalid;
}

static void TestTable(void)
{
    static const char* const bad[] = {
        "echo", "u:echo", "mloc:echo", ":echo", "x:", "x:1a", "1x:a", "x:a:b", "x:\xff", ""};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        CHECK(Refused(bad[i]), "a name that is not namespaced or is the library's is refused");
    }
    CHECK(!Refused("x:echo") && !Refused("ns-1:a.b_c") && !Refused("u2:a"),
          "a namespaced name is taken");
    mlocMessageFormatter formatter;
    mlocMessageFunction twice[2] = {s_functions[0], s_functions[0]};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, twice, 2, NULL};
    CHECK(mlocInitMessageFormatter(&s_data, &s_locale, &options, &formatter) == mloc_errorInvalid,
          "two functions of one name are refused");
    twice[1].resolve = NULL;
    twice[1].name = "x:other";
    twice[1].nameLength = 7;
    CHECK(mlocInitMessageFormatter(&s_data, &s_locale, &options, &formatter) == mloc_errorInvalid,
          "a function without a resolve entry is refused");
    options.functions = NULL;
    CHECK(mlocInitMessageFormatter(&s_data, &s_locale, &options, &formatter) == mloc_errorInvalid,
          "a null table with a count is refused");
    options.functionCount = 0;
    CHECK(mlocInitMessageFormatter(&s_data, &s_locale, &options, &formatter) == mloc_success,
          "a null table without a count is taken");
}

// Compiles and binds a message, then formats it; the result.
static mlocResult Run(const char* message, const mlocMessageArgument* arguments, size_t count,
                      mlocMessageFaults* faults, size_t* lengthOut)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    if (mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocMessageFormatter_Bind(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success)
    {
        return mloc_errorSyntax;
    }
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    mlocResult result = mlocFormatMessage(s_bound, arguments, count, &output, faults);
    *lengthOut = output.length;
    return result;
}

// Whether a message formats to a text with the faults given in order
// (0 ending the list).
static bool Formats(const char* message, const mlocMessageArgument* arguments, size_t count,
                    const char* text, const mlocMessageFaultKind* expected)
{
    mlocMessageFault list[8];
    mlocMessageFaults faults = {list, 8, 0};
    size_t length = 0;
    mlocResult result = Run(message, arguments, count, &faults, &length);
    size_t expectedCount = 0;
    while (expected != NULL && expected[expectedCount] != mloc_messageFaultNone)
    {
        expectedCount++;
    }
    bool good = result == (expectedCount > 0 ? mloc_withFallbacks : mloc_success) &&
                length == strlen(text) && memcmp(s_text, text, length) == 0 &&
                faults.count == expectedCount;
    for (size_t i = 0; good && i < expectedCount; i++)
    {
        good = list[i].kind == expected[i];
    }
    if (!good)
    {
        printf("  [%s] -> %d [%.*s], %zu faults\n", message, (int)result, (int)length, s_text,
               faults.count);
    }
    return good;
}

static const mlocMessageArgument s_arguments[] = {
    {"s", 1, {mloc_messageValueString, 0, {.string = {"S", 1}}}},
    {"n", 1, {mloc_messageValueInteger, 0, {.integer = 5}}},
    {"o", 1, {mloc_messageValueObject, 3, {.object = &s_locale}}},
    {"e", 1, {mloc_messageValueString, 0, {.string = {"e\xcc\x81", 3}}}},
};

static void TestCalls(void)
{
    const mlocMessageArgument* a = s_arguments;
    CHECK(Formats("{|a| :x:echo k=v n=$n s=$s}", a, 4, "a;k=v;n=#;s=S", NULL),
          "options are read in order, a literal and arguments");
    CHECK(Formats("{:x:echo}", a, 4, "_", NULL) && Formats("{$n :x:echo}", a, 4, "#", NULL) &&
              Formats("{$s :x:echo}", a, 4, "S", NULL),
          "no operand, an argument, a string argument");
    CHECK(Formats(".local $v = {|a| :x:echo} {{{$v :x:echo} {$v :x:short}}}", a, 4, "@0 @0", NULL),
          "a resolved value is passed with its function's index");
    CHECK(Formats(".local $v = {|t| :string} {{{$v :x:echo k=$v}}}", a, 4, "t;k=t", NULL),
          "a :string value is passed as text");
    CHECK(Formats(".local $n = {7.25 :number maximumFractionDigits=0} {{{$n :x:echo k=$n}}}", a, 4,
                  "N3;k=N3", NULL),
          "a number value is passed as its decimal, unrounded");
    CHECK(
        Formats(".local $c = {7.25 :currency currency=eur} {{{$c :x:echo k=$c}}}", a, 4,
                "C3EUR;k=C3EUR", NULL) &&
            Formats(".local $c = {7 :currency currency=czk} {{{$c :x:echo}}}", a, 4, "C1CZK", NULL),
        "a currency value is passed as its amount and code");
    CHECK(Formats(".local $u = {7.5 :unit unit=meter} {{{$u :x:echo}}}", a, 4, "U2meter", NULL),
          "a unit value is passed as its amount and unit");
    CHECK(Formats(".local $d = {|2006-01-02| :date} .local $t = {|2006-01-02T15:04| :time} {{{$d "
                  ":x:echo}{$t :x:echo}}}",
                  a, 4, "D732313:0B732313:15", NULL),
          "a date value is passed as its date and time, and which it had");
    CHECK(Formats(".local $z = {|2006-01-02T15:04:06Z| :datetime timeZone=|America/New_York|} "
                  ".local $o = {|2006-01-02T15:04:06-05:00| :time} {{{$z :x:echo}|{$o :x:echo}}}",
                  a, 4, "B732313:10z1136214246America/New_York|B732313:15z1136232246-05:00", NULL),
          "a zoned date value is passed with its instant and zone");
    CHECK(Formats(".local $d = {|PT2H30M| :mloc:duration} {{{$d :x:echo}}}", a, 4, "P2h30m", NULL),
          "a duration value is passed as its fields");
    static const mlocListItem names[] = {{"Ann", 3}, {"Bob", 3}};
    mlocMessageArgument list = {"l", 1, {mloc_messageValueList, 0, {.list = {names, 2}}}};
    CHECK(Formats(".local $o = {$l :mloc:list type=disjunction} {{{$o :x:echo}}}", &list, 1,
                  "L2Ann", NULL),
          "a list value is passed as its items");
    mlocMessageArgument named[] = {
        {"\xC3\xA9", 2, {mloc_messageValueString, 0, {.string = {"wrong", 5}}}},
        {"n", 1, {mloc_messageValueInteger, 0, {.integer = 5}}},
    };
    CHECK(Formats("{$n}", named, 2, "5", NULL),
          "an argument's name already in NFC is another variable's when its bytes differ");
    static const mlocMessageFaultKind badOperand[] = {mloc_messageFaultBadOperand, 0};
    s_echoes = 0;
    CHECK(Formats(".local $x = {|7| :x:echo} .local $y = {$x :number} {{{$y}}}", a, 4, "{$y}",
                  badOperand) &&
              s_echoes == 1,
          "binding never calls a program function, while preparing a number");
    static const mlocMessageFaultKind unresolved[] = {mloc_messageFaultUnresolvedVariable,
                                                      mloc_messageFaultBadOption, 0};
    CHECK(Formats("{|a| :x:echo k=$missing}", a, 4, "a;k=?", unresolved),
          "an option that fails is a bad option, reported once, read as a fallback");
    static const mlocMessageFaultKind function[] = {mloc_messageFaultFunction, 0};
    CHECK(Formats("{|fail| :x:echo}", a, 4, "{|fail|}", function) &&
              Formats("{|a| :x:echo fail=1}", a, 4, "{|a|}", function),
          "a function's faults fall back, in resolve and in format");
    static const mlocMessageFaultKind unsupported[] = {mloc_messageFaultUnsupportedOperation, 0};
    CHECK(Formats("{$s :x:any}", a, 4, "{$s}", unsupported),
          "a function without a format entry falls back");
    CHECK(Formats("{:x:locale}", a, 4, "de-CH", NULL), "a call's locale is the formatter's");

    size_t length = 0;
    char small[4];
    mlocOutput output = {small, sizeof small, 0, NULL, NULL};
    CHECK(Run("{|abcdef| :x:echo}!", a, 4, NULL, &length) == mloc_success &&
              mlocFormatMessage(s_bound, a, 4, &output, NULL) == mloc_errorCapacity &&
              output.length == 7 && memcmp(small, "abcd", 4) == 0,
          "a function's text past the buffer counts its length");
    output.capacity = 0;
    output.buffer = NULL;
    CHECK(mlocFormatMessage(s_bound, a, 4, &output, NULL) == mloc_errorCapacity &&
              output.length == 7,
          "a function's text is measured with no buffer");
}

static void TestAccessors(void)
{
    mlocMessageCall call;
    memset(&call, 0, sizeof call);
    const char* name;
    size_t length;
    mlocMessageOperand value;
    mlocMessageResolved storage;
    mlocLocale locale;
    CHECK(mlocMessageCall_GetOptionCount(NULL) == 0 && mlocMessageCall_GetOptionCount(&call) == 0 &&
              mlocMessageCall_GetOption(&call, 0, &name, &length, &value, &storage) ==
                  mloc_errorInvalid &&
              mlocMessageCall_FindOption(&call, "k", 1, &value, &storage) == mloc_errorInvalid &&
              mlocMessageCall_GetLocale(&call, &locale) == mloc_errorInvalid &&
              mlocMessageCall_GetLocale(NULL, &locale) == mloc_errorInvalid,
          "a call the library did not make is refused");
}

static void TestString(void)
{
    const mlocMessageArgument* a = s_arguments;
    static const mlocMessageFaultKind operand[] = {mloc_messageFaultBadOperand, 0};
    CHECK(Formats("{$s :string}{$n :string}", a, 4, "S5", NULL),
          ":string writes a string, and a number as its plain decimal");
    CHECK(Formats("{$o :string}", a, 4, "{$o}", operand) &&
              Formats("{:string}", a, 4, "{:string}", operand) &&
              Formats(".local $v = {|a| :x:echo} {{{$v :string}}}", a, 4, "{$v}", operand),
          ":string refuses an object, no operand, a function's value");
    static const mlocMessageFaultKind unresolved[] = {mloc_messageFaultUnresolvedVariable, 0};
    CHECK(Formats("{$missing :string}", a, 4, "{$missing}", unresolved),
          ":string over a fallback adds no fault");
    CHECK(Formats(".input {$n :string} .match $n 5 {{five}} * {{other}}", a, 4, "five", NULL) &&
              Formats(".input {$e :string} .match $e \xc3\xa9 {{nfc}} * {{other}}", a, 4, "nfc",
                      NULL) &&
              Formats(".input {$s :string} .match $s s {{s}} * {{other}}", a, 4, "other", NULL),
          ":string matches a number's decimal and a key in NFC, case and all");
    // A string not in NFC and longer than the stack's buffer cannot be
    // compared: the selector is bad, matching only "*".
    static char decomposed[600];
    for (size_t i = 0; i < 200; i++)
    {
        memcpy(decomposed + 3 * i, "e\xcc\x81", 3);
    }
    mlocMessageArgument longer = {
        "e", 1, {mloc_messageValueString, 0, {.string = {decomposed, 600}}}};
    static const mlocMessageFaultKind bad[] = {mloc_messageFaultBadSelector, 0};
    CHECK(Formats(".input {$e :string} .match $e x {{x}} * {{other}}", &longer, 1, "other", bad),
          "a long string not in NFC is a bad selector");
    mlocMessageArgument unnamed = {NULL, 0, {mloc_messageValueInteger, 0, {.integer = 1}}};
    static const mlocMessageFaultKind unresolved2[] = {mloc_messageFaultUnresolvedVariable, 0};
    CHECK(Formats("{$x}", &unnamed, 1, "{$x}", unresolved2),
          "an argument without a name matches no variable");
}

static void TestSelection(void)
{
    const mlocMessageArgument* a = s_arguments;
    static const mlocMessageFaultKind bad[] = {mloc_messageFaultBadSelector, 0};
    CHECK(Formats(".local $v = {|a| :x:echo} .match $v a {{A}} * {{other}}", a, 4, "other", bad),
          "a function without a match entry is a bad selector");
    CHECK(Formats(".local $v = {|broken| :x:any} .match $v a {{A}} b {{B}} * {{other}}", a, 4,
                  "other", bad),
          "a match fault breaks the selector, reported once");
    CHECK(
        Formats(".local $v = {|a| :x:any} .match $v bb {{B}} a {{A}} * {{other}}", a, 4, "B", NULL),
        "without a better entry, the first matching variant");
    CHECK(Formats(".local $v = {|a| :x:short} .match $v bb {{B}} a {{A}} * {{other}}", a, 4, "A",
                  NULL),
          "the better key wins");
    CHECK(Formats(".local $v = {|a| :x:short} .match $v ab {{AB}} a {{A}} * {{other}}", a, 4, "A",
                  NULL),
          "the better key wins over one it begins");
    CHECK(Formats(".local $v = {|a| :x:any} .match $v a {{A}} * {{other}} b {{B}} c {{C}}", a, 4,
                  "A", NULL) &&
              Formats(".local $v = {|a| :x:any} .match $v * {{other}}", a, 4, "other", NULL),
          "a tie at the last selector keeps the first variant; one variant alone");
    CHECK(Formats(".local $v = {|a| :x:any} .local $w = {|none| :x:any}"
                  " .match $w $v a a {{AA}} * * {{other}}",
                  a, 4, "other", NULL),
          "only the message's variants are chosen from");
    CHECK(Formats(".local $v = {|a| :x:short} .local $w = {|none| :x:short}"
                  " .match $v $w bb * {{B}} a x {{AX}} a * {{A}} * * {{other}}",
                  a, 4, "A", NULL),
          "the better key at the first selector whose keys differ");

    // 32 selectors bind; 33 are refused.
    for (int count = 32; count <= 33; count++)
    {
        char text[512];
        size_t at = 0;
        AppendPrint(text, sizeof text, &at, "%s", ".local $v = {|a| :x:any} .match");
        for (int i = 0; i < count; i++)
        {
            AppendPrint(text, sizeof text, &at, "%s", " $v");
        }
        for (int i = 0; i < count; i++)
        {
            AppendPrint(text, sizeof text, &at, "%s", " *");
        }
        AppendPrint(text, sizeof text, &at, "%s", " {{all}}");
        size_t size = 0;
        size_t bound = 0;
        mlocMessageError error;
        mlocTextResult compiled =
            mlocCompileMessage(text, strlen(text), s_compiled, sizeof s_compiled, &size, &error);
        CHECK(count == 32 ? compiled.status == mloc_success &&
                                mlocMessageFormatter_Bind(&s_formatter, s_compiled, size, s_bound,
                                                          sizeof s_bound, &bound) == mloc_success
                          : compiled.status == mloc_errorLimit && compiled.offset == 0 &&
                                error == mloc_messageErrorNone,
              "32 selectors compile and bind, 33 are refused");
    }
}

// Binds a message; the result.
static mlocResult BindOnly(const char* message)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    mlocTextResult compiled =
        mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error);
    if (compiled.status != mloc_success)
    {
        return compiled.status;
    }
    return mlocMessageFormatter_Bind(s_using, s_compiled, size, s_bound, sizeof s_bound, &bound);
}

static void TestResolutionLimit(void)
{
    // Each local named by the next as operand and option: 1 + 3c
    // resolutions, 29524 for $a10, so two references bind (59048) and
    // three (88572) pass 65536.
    char text[2048];
    size_t at = 0;
    AppendPrint(text, sizeof text, &at, "%s", ".local $a1 = {|x| :string}");
    for (int i = 2; i <= 10; i++)
    {
        AppendPrint(text, sizeof text, &at, " .local $a%d = {$a%d :string k=$a%d}", i, i - 1,
                    i - 1);
    }
    // Each message below ends the declarations differently, from at.
    snprintf(text + at, sizeof text - at, " {{{$a10}{$a10}}}");
    CHECK(BindOnly(text) == mloc_success, "a message within the resolution limit binds");
    snprintf(text + at, sizeof text - at, " {{{$a10}{$a10}{$a10}}}");
    CHECK(BindOnly(text) == mloc_errorLimit, "one past it is refused");
    snprintf(text + at, sizeof text - at, " .match $a10 x {{{$a10}}} * {{{$a10}{$a10}}}");
    CHECK(BindOnly(text) == mloc_errorLimit, "a selector counts with the costliest variant");
    snprintf(text + at, sizeof text - at, " .match $a10 x {{{$a10}}} * {{x}}");
    CHECK(BindOnly(text) == mloc_success, "and a cheaper one binds");
    // Exactly the limit: 1 + 9841 + 2 * 27847 resolutions in one
    // expression binds.
    snprintf(text + at, sizeof text - at,
             " {{{$a9 :string k1=$a9 k2=$a9 k3=$a8 k4=$a8 k5=$a7 k6=$a6 k7=$a5 k8=$a3 k9=$a3"
             " k10=$a1}}}");
    CHECK(BindOnly(text) == mloc_success, "a message at the limit binds");
    snprintf(text + at, sizeof text - at,
             " {{{$a9 :string k1=$a9 k2=$a9 k3=$a8 k4=$a8 k5=$a7 k6=$a6 k7=$a5 k8=$a3 k9=$a3"
             " k10=$a2}}}");
    CHECK(BindOnly(text) == mloc_errorLimit, "one more resolution is refused");
    snprintf(text + at, sizeof text - at, " .match $a10 x {{{$a10}{$a10}}} * {{{$a10}}}");
    CHECK(BindOnly(text) == mloc_errorLimit, "each variant's pieces are its own");
    char input[4096];
    snprintf(input, sizeof input, ".input {$s :string} %s", text);
    size_t end = strlen(input) - strlen(" .match $a10 x {{{$a10}{$a10}}} * {{{$a10}}}");
    snprintf(input + end, sizeof input - end, " .match $s x {{{$a10}{$a10}}} * {{{$a10}}}");
    CHECK(BindOnly(input) == mloc_success, "the costliest variant counts, not their sum");
    snprintf(text + at, sizeof text - at, " {{{#b k1=$a10 k2=$a10}{/b k=$a1}}}");
    CHECK(BindOnly(text) == mloc_success, "markup's options count");
    snprintf(text + at, sizeof text - at, " {{{#b k1=$a10 k2=$a10}{/b k=$a10}}}");
    CHECK(BindOnly(text) == mloc_errorLimit, "and pass the limit");
    snprintf(text + at, sizeof text - at, " {{{#b k=$a1}{|x| :string k=$a10}}}");
    CHECK(BindOnly(text) == mloc_success, "a markup counts only its own options");

    // 1024 declarations bind, 1025 are refused.
    static char many[32768];
    for (int count = 1024; count <= 1025; count++)
    {
        size_t used = 0;
        for (int i = 0; i < count; i++)
        {
            AppendPrint(many, sizeof many, &used, ".local $d%d={1}", i);
        }
        AppendPrint(many, sizeof many, &used, "%s", "{{}}");
        CHECK(BindOnly(many) == (count == 1024 ? mloc_success : mloc_errorLimit),
              "1024 declarations bind, 1025 are refused");
    }

    // A bind writes exactly the size it measured, nothing past it.
    size_t size = 0;
    size_t needed = 0;
    mlocMessageError error;
    static uint8_t guarded[256];
    CHECK(mlocCompileMessage("{#b k=$x}", 9, s_compiled, sizeof s_compiled, &size, &error).status ==
                  mloc_success &&
              mlocMessageFormatter_Bind(s_using, s_compiled, size, NULL, 0, &needed) ==
                  mloc_errorCapacity &&
              needed + 8 <= sizeof guarded,
          "a message of markup alone measures");
    memset(guarded, 0xA5, sizeof guarded);
    size_t bound = 0;
    bool untouched = true;
    CHECK(mlocMessageFormatter_Bind(s_using, s_compiled, size, guarded, needed, &bound) ==
              mloc_success,
          "it binds into that size");
    for (size_t i = needed; i < sizeof guarded; i++)
    {
        untouched = untouched && guarded[i] == 0xA5;
    }
    CHECK(untouched, "no byte past the bound message is written");

    const mlocMessageArgument* a = s_arguments;
    static const mlocMessageFaultKind out[] = {mloc_messageFaultUnsupportedOperation, 0};
    CHECK(Formats(".local $a = {|v| :x:echo} {{{|g| :x:greedy k=$a}{|h| :x:greedy k=$a}}}", a, 4,
                  "okshort", out),
          "a call stops resolving at the limit, reported once");
}

// A formatter isolating by the Default Bidi Strategy, for a locale.
static void MakeIsolating(mlocMessageFormatter* formatter, const char* tag)
{
    mlocLocale locale;
    mlocMessageOptions options = {false, mloc_messageIsolationDefault, s_functions, functionCount,
                                  NULL};
    CHECK(mlocParseLocale(tag, strlen(tag), &locale).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &locale, &options, formatter) == mloc_success,
          "an isolating formatter is made");
}

#define LRI "\xE2\x81\xA6"
#define RLI "\xE2\x81\xA7"
#define FSI "\xE2\x81\xA8"
#define PDI "\xE2\x81\xA9"

static void TestIsolation(void)
{
    static mlocMessageFormatter en;
    static mlocMessageFormatter ar;
    static mlocMessageFormatter unknown;
    MakeIsolating(&en, "en");
    MakeIsolating(&ar, "ar");
    MakeIsolating(&unknown, "qaa");
    const mlocMessageArgument* a = s_arguments;
    s_using = &en;
    CHECK(Formats("{$s} and {|a| :x:echo}", a, 4, FSI "S" PDI " and " FSI "a" PDI, NULL),
          "an argument and a function without a direction are isolated as unknown");
    CHECK(Formats("{|a| :x:ltr}{|b| :x:rtl}{|c| :x:odd}", a, 4, "a" RLI "b" PDI FSI "c" PDI, NULL),
          "a left-to-right value is bare in a left-to-right message");
    CHECK(Formats("{|a| :x:ltr u:dir=ltr}{|b| :x:echo u:dir=rtl}{|c| :x:ltr u:dir=auto}"
                  "{|d| :x:ltr u:dir=inherit}{|e| :x:rtl u:dir=inherit}",
                  a, 4, LRI "a" PDI RLI "b" PDI FSI "c" PDI "d" RLI "e" PDI, NULL),
          "u:dir replaces the direction and isolates; inherit keeps it");
    static const mlocMessageFaultKind bad[] = {mloc_messageFaultBadOption, 0};
    static const mlocMessageFaultKind bad2[] = {mloc_messageFaultBadOption,
                                                mloc_messageFaultBadOption, 0};
    CHECK(Formats("{|a| :x:ltr u:dir=up}", a, 4, "a", bad) &&
              Formats("{|a| :x:ltr u:dir=$o u:id=$o}", a, 4, "a", bad2),
          "a bad u:dir or u:id is a bad option, ignored");
    mlocMessageArgument rtl = {"d", 1, {mloc_messageValueString, 0, {.string = {"rtl", 3}}}};
    CHECK(Formats("{|a| :x:ltr u:dir=$d u:id=$d u:locale=fr}", &rtl, 1, RLI "a" PDI, NULL),
          "u:dir from a variable; u:id and other u: options take no part");
    CHECK(Formats("{|a| :x:echo k=v u:dir=ltr u:id=z u:other=1}", a, 4, LRI "a;k=v" PDI, NULL),
          "a function does not see the u: options");
    CHECK(Formats(".local $v = {|a| :x:echo u:dir=rtl} {{{$v} {$v :x:echo}}}", a, 4,
                  RLI "a" PDI " " FSI "@0" PDI, NULL),
          "a local keeps its u:dir; a new call does not");
    CHECK(Formats(".local $v = {|a| :string u:dir=ltr} {{{$v :string}}}", a, 4, FSI "a" PDI, NULL),
          ":string's value is its own");
    static const mlocMessageFaultKind unresolved[] = {mloc_messageFaultUnresolvedVariable, 0};
    static const mlocMessageFaultKind function[] = {mloc_messageFaultFunction, 0};
    CHECK(Formats("{$missing}", a, 4, FSI "{$missing}" PDI, unresolved) &&
              Formats("{|a| :x:ltr fail=1}", a, 4, FSI "{|a|}" PDI, function),
          "a fallback is isolated as unknown, also where a bare value failed to format");
    static const mlocMessageFaultKind markup[] = {mloc_messageFaultBadOption, 0};
    static const mlocMessageFaultKind markupVariable[] = {mloc_messageFaultUnresolvedVariable,
                                                          mloc_messageFaultBadOption, 0};
    CHECK(Formats("{#b u:dir=rtl}x{/b}", a, 4, "x", markup) &&
              Formats("{#b k=$missing}x{/b u:id=$s}", a, 4, "x", markupVariable) &&
              Formats("{#b u:id=x k=$s}x{/b}{#c/}", a, 4, "x", NULL),
          "markup's options resolve, and u:dir on it is a bad option");
    CHECK(Formats("{#b}{|a| :x:ltr u:dir=rtl}", a, 4, RLI "a" PDI, NULL),
          "markup reads only its own options");
    static const mlocMessageFaultKind failed[] = {mloc_messageFaultFunction, 0};
    CHECK(Formats("{|fail| :x:echo u:dir=rtl}", a, 4, FSI "{|fail|}" PDI, failed),
          "u:dir does not direct a fallback");

    s_using = &ar;
    CHECK(Formats("{|a| :x:ltr}{|b| :x:rtl}{$s}", a, 4, LRI "a" PDI RLI "b" PDI FSI "S" PDI, NULL),
          "a right-to-left message isolates a left-to-right value");
    CHECK(Formats("{|a| :x:ltr fail=1}", a, 4, FSI "{|a|}" PDI, function),
          "a fallback's isolate replaces the value's");
    s_using = &unknown;
    CHECK(Formats("{|a| :x:ltr}", a, 4, LRI "a" PDI, NULL),
          "a message of unknown direction isolates a left-to-right value");
    s_using = &s_formatter;

    mlocLocale locale;
    mlocMessageFormatter formatter;
    mlocMessageOptions options = {false, (mlocMessageIsolation)2, NULL, 0, NULL};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &locale, &options, &formatter) == mloc_errorInvalid,
          "an unknown isolation is refused");
}

// x:nest formats the message it stands in again, from inside the call,
// two levels down, then writes "x": "a{$n :x:nest}b" is "aaaxbbb".
typedef struct Nest
{
    const void* bound;
    int depth;
} Nest;

static mlocMessageFaultKind NestFormat(void* context, const mlocMessageResolved* value,
                                       mlocOutput* output)
{
    (void)value;
    Nest* nest = context;
    if (nest->depth == 2)
    {
        output->length = 1;
        if (output->buffer != NULL && output->capacity > 0)
        {
            output->buffer[0] = 'x';
        }
        return mloc_messageFaultNone;
    }
    nest->depth++;
    mlocMessageArgument argument = {"n", 1, {mloc_messageValueInteger, 0, {.integer = 5}}};
    mlocResult result = mlocFormatMessage(nest->bound, &argument, 1, output, NULL);
    nest->depth--;
    return result == mloc_success ? mloc_messageFaultNone : mloc_messageFaultFunction;
}

// Formats the message it reports parts of again, from inside the part
// function, and checks the text is the same.
typedef struct Again
{
    const void* bound;
    const char* text;
    size_t length;
    int calls;
    bool same;
} Again;

static void AgainPart(void* context, mlocPart part, size_t start, size_t length)
{
    (void)part;
    (void)start;
    (void)length;
    Again* again = context;
    char text[64];
    mlocOutput output = {text, sizeof text, 0, NULL, NULL};
    mlocMessageArgument argument = {"n", 1, {mloc_messageValueInteger, 0, {.integer = 1234}}};
    again->same = again->same &&
                  mlocFormatMessage(again->bound, &argument, 1, &output, NULL) == mloc_success &&
                  output.length == again->length && memcmp(text, again->text, again->length) == 0;
    again->calls++;
}

// The library's callbacks may call it back: every object is read only
// after it is made, and a call keeps its state on its stack.
static void TestCallingBack(void)
{
    static uint8_t compiled[1024];
    static uint8_t bound[1 << 16];
    Nest nest = {bound, 0};
    mlocMessageFunction function = {"x:nest", 6, &nest, EchoResolve, NestFormat, NULL, NULL, NULL};
    mlocMessageOptions options = {false, mloc_messageIsolationNone, &function, 1, NULL};
    mlocMessageFormatter formatter;
    const char* message = "a{$n :x:nest}b";
    size_t size = 0;
    size_t boundSize = 0;
    mlocMessageError error;
    char text[64];
    mlocOutput output = {text, sizeof text, 0, NULL, NULL};
    mlocMessageArgument argument = {"n", 1, {mloc_messageValueInteger, 0, {.integer = 5}}};
    CHECK(mlocInitMessageFormatter(&s_data, &s_locale, &options, &formatter) == mloc_success &&
              mlocCompileMessage(message, strlen(message), compiled, sizeof compiled, &size, &error)
                      .status == mloc_success &&
              mlocMessageFormatter_Bind(&formatter, compiled, size, bound, sizeof bound,
                                        &boundSize) == mloc_success &&
              mlocFormatMessage(bound, &argument, 1, &output, NULL) == mloc_success &&
              output.length == 7 && memcmp(text, "aaaxbbb", 7) == 0 && nest.depth == 0,
          "a program function formats the message it stands in, from inside its call");
    const char* number = "{$n :number}";
    Again again = {bound, "1'234", 5, 0, true};
    mlocOutput parts = {text, sizeof text, 0, AgainPart, &again};
    argument.value.as.integer = 1234;
    CHECK(mlocCompileMessage(number, strlen(number), compiled, sizeof compiled, &size, &error)
                      .status == mloc_success &&
              mlocMessageFormatter_Bind(&formatter, compiled, size, bound, sizeof bound,
                                        &boundSize) == mloc_success &&
              mlocFormatMessage(bound, &argument, 1, &parts, NULL) == mloc_success &&
              parts.length == again.length && memcmp(text, again.text, again.length) == 0 &&
              again.calls > 0 && again.same,
          "a part function formats the message it is given parts of, from inside its call");
}

int main(void)
{
    MakeFormatter();
    TestTable();
    TestCalls();
    TestAccessors();
    TestString();
    TestSelection();
    TestResolutionLimit();
    TestIsolation();
    TestCallingBack();
    return s_failures == 0 ? 0 : 1;
}
