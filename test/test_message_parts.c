// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A formatted message's parts (maul-locale/message.h): text, placeholders,
// fallbacks, markup and isolation marks tiling the text in order, each
// placeholder's own parts inside it as its function reports them, a
// program function's parts passed on, and none when the call does not
// succeed.

#include "test_harness.h"

#include "maul-locale/datetime.h"
#include "maul-locale/message.h"
#include "maul-locale/number.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocMessageFormatter s_plain;
static mlocMessageFormatter s_isolating;
static mlocMessageFormatter s_strict;
static uint8_t s_compiled[1 << 16];
static uint8_t s_bound[1 << 16];
static char s_text[1 << 10];

// The parts a call reported, in order.
typedef struct Part
{
    uint32_t kind;
    size_t start;
    size_t length;
} Part;

typedef struct Parts
{
    Part list[64];
    size_t count;
} Parts;

static void Collect(void* context, uint32_t part, size_t start, size_t length)
{
    Parts* parts = context;
    if (parts->count < sizeof parts->list / sizeof parts->list[0])
    {
        parts->list[parts->count] = (Part){part, start, length};
    }
    parts->count++;
}

// A message part's kind.
static uint32_t Message(uint32_t kind, uint32_t index)
{
    return index << 8 | kind;
}

// Formats a message, collecting its parts; the call's result.
static mlocResult Format(const mlocMessageFormatter* formatter, const char* message,
                         const mlocMessageArgument* arguments, size_t count, size_t capacity,
                         Parts* parts)
{
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    memset(parts, 0, sizeof *parts);
    if (mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                .status != mloc_success ||
        mlocMessageFormatter_Bind(formatter, s_compiled, size, s_bound, sizeof s_bound, &bound) !=
            mloc_success)
    {
        printf("  [%s] does not compile and bind\n", message);
        return mloc_errorInvalid;
    }
    mlocOutput output = {s_text, capacity, 0, Collect, parts};
    mlocResult result = mlocFormatMessage(s_bound, arguments, count, &output, NULL);
    s_text[output.length < sizeof s_text ? output.length : sizeof s_text - 1] = '\0';
    return result;
}

// Whether a message's text and parts are those wanted.
static bool PartsAre(const mlocMessageFormatter* formatter, const char* message,
                     const mlocMessageArgument* arguments, size_t count, const char* text,
                     const Part* wanted, size_t wantedCount)
{
    Parts parts;
    mlocResult result = Format(formatter, message, arguments, count, sizeof s_text, &parts);
    bool good = (result == mloc_success || result == mloc_withFallbacks) &&
                strcmp(s_text, text) == 0 && parts.count == wantedCount;
    for (size_t i = 0; good && i < wantedCount; i++)
    {
        good = parts.list[i].kind == wanted[i].kind && parts.list[i].start == wanted[i].start &&
               parts.list[i].length == wanted[i].length;
    }
    if (!good)
    {
        printf("  [%s] -> %d [%s], %zu parts:", message, (int)result, s_text, parts.count);
        for (size_t i = 0; i < parts.count && i < 64; i++)
        {
            printf(" %#x@%zu+%zu", (unsigned)parts.list[i].kind, parts.list[i].start,
                   parts.list[i].length);
        }
        printf("\n");
    }
    return good;
}

// Whether the message's own parts tile the text in order, and every
// other part lies inside the placeholder before it.
static bool Tiles(const Parts* parts, size_t length)
{
    size_t end = 0;
    size_t outerStart = 0;
    size_t outerEnd = 0;
    for (size_t i = 0; i < parts->count; i++)
    {
        const Part* p = &parts->list[i];
        if ((p->kind & 0xFF) >= mloc_messagePartText)
        {
            if (p->start != end)
            {
                return false;
            }
            end = p->start + p->length;
            outerStart = p->start;
            outerEnd = end;
        }
        else if (p->start < outerStart || p->start + p->length > outerEnd)
        {
            return false;
        }
    }
    return end == length;
}

static void TestPieces(void)
{
    mlocMessageArgument name = {"name", 4, {mloc_messageValueString, 0, {.string = {"World", 5}}}};
    Part hello[] = {{mloc_messagePartText, 0, 6},
                    {Message(mloc_messagePartPlaceholder, 0), 6, 5},
                    {mloc_messagePartText, 11, 1}};
    CHECK(PartsAre(&s_plain, "Hello {$name}!", &name, 1, "Hello World!", hello, 3),
          "text and a placeholder");
    Part isolated[] = {{mloc_messagePartText, 0, 6},
                       {mloc_messagePartIsolation, 6, 3},
                       {Message(mloc_messagePartPlaceholder, 0), 9, 5},
                       {mloc_messagePartIsolation, 14, 3},
                       {mloc_messagePartText, 17, 1}};
    CHECK(PartsAre(&s_isolating, "Hello {$name}!", &name, 1, "Hello \xE2\x81\xA8World\xE2\x81\xA9!",
                   isolated, 5),
          "isolation marks are parts of their own");
    Part fallbacks[] = {{Message(mloc_messagePartFallback, 0), 0, 4},
                        {mloc_messagePartText, 4, 1},
                        {Message(mloc_messagePartFallback, 1), 5, 5}};
    CHECK(PartsAre(&s_plain, "{$x} {|a| :x:none}", NULL, 0, "{$x} {|a|}", fallbacks, 3),
          "fallbacks are parts with their expression's index");
    Part markup[] = {{Message(mloc_messagePartMarkupOpen, 0), 0, 0},
                     {mloc_messagePartText, 0, 1},
                     {Message(mloc_messagePartMarkupClose, 1), 1, 0},
                     {Message(mloc_messagePartMarkupStandalone, 2), 1, 0}};
    CHECK(PartsAre(&s_plain, "{#b}x{/b}{#br/}", NULL, 0, "x", markup, 4),
          "markup is a part of no length at its place, with its index");
    mlocMessageArgument y = {"y", 1, {mloc_messageValueString, 0, {.string = {"b", 1}}}};
    Part local[] = {{Message(mloc_messagePartPlaceholder, 1), 0, 1},
                    {mloc_messagePartText, 1, 1},
                    {Message(mloc_messagePartPlaceholder, 2), 2, 1}};
    CHECK(PartsAre(&s_plain, ".local $x = {|a|} {{{$x} {$y}}}", &y, 1, "a b", local, 3),
          "a placeholder's index is its expression's, after the declarations'");
    Part selected[] = {{mloc_messagePartText, 0, 3},
                       {Message(mloc_messagePartPlaceholder, 1), 3, 1}};
    CHECK(PartsAre(&s_plain, ".input {$y :string} .match $y a {{one}} b {{two{$y}}} * {{other}}",
                   &y, 1, "twob", selected, 2),
          "the selected variant's parts");
}

static void TestInner(void)
{
    Part number[] = {{Message(mloc_messagePartPlaceholder, 0), 0, 8},
                     {mloc_numberPartInteger, 0, 1},
                     {mloc_numberPartGroup, 1, 1},
                     {mloc_numberPartInteger, 2, 3},
                     {mloc_numberPartDecimal, 5, 1},
                     {mloc_numberPartFraction, 6, 2},
                     {mloc_messagePartText, 8, 2}};
    CHECK(PartsAre(&s_plain, "{1234.5 :number minimumFractionDigits=2} m", NULL, 0, "1,234.50 m",
                   number, 7),
          "a number's parts inside its placeholder");
    Part date[] = {{mloc_messagePartText, 0, 3}, {Message(mloc_messagePartPlaceholder, 0), 3, 11},
                   {mloc_datePartMonth, 3, 3},   {mloc_datePartLiteral, 6, 1},
                   {mloc_datePartDay, 7, 1},     {mloc_datePartLiteral, 8, 2},
                   {mloc_datePartYear, 10, 4}};
    CHECK(PartsAre(&s_plain, "On {|2006-01-02| :date}", NULL, 0, "On Jan 2, 2006", date, 7),
          "a date's parts inside its placeholder");
    Part isolated[] = {{mloc_messagePartIsolation, 0, 3},
                       {Message(mloc_messagePartPlaceholder, 0), 3, 2},
                       {mloc_numberPartInteger, 3, 2},
                       {mloc_messagePartIsolation, 5, 3}};
    CHECK(PartsAre(&s_isolating, "{42 :number u:dir=rtl}", NULL, 0,
                   "\xE2\x81\xA7"
                   "42\xE2\x81\xA9",
                   isolated, 4),
          "a function's parts are moved past the isolation mark");
}

// x:parts writes "ab" as two parts of its own, one of them with a kind
// whose low byte is a message part's.
static mlocMessageFaultKind PartsResolve(void* context, const mlocMessageCall* call,
                                         const mlocMessageOperand* operand,
                                         mlocMessageResolved* valueOut)
{
    (void)context;
    (void)call;
    (void)operand;
    memset(valueOut, 0, sizeof *valueOut);
    return mloc_messageFaultNone;
}

static size_t s_formats;

static mlocMessageFaultKind PartsFormat(void* context, const mlocMessageResolved* value,
                                        mlocOutput* output)
{
    (void)context;
    (void)value;
    s_formats++;
    output->length = 2;
    if (output->capacity < 2)
    {
        return mloc_messageFaultNone;
    }
    memcpy(output->buffer, "ab", 2);
    if (output->part != NULL)
    {
        output->part(output->context, 0x105, 0, 1);
        output->part(output->context, 0x7F, 0, 1);
        output->part(output->context, 0x80, 1, 1);
        output->part(output->context, 0x81, 1, 1);
    }
    return mloc_messageFaultNone;
}

static void TestProgramParts(void)
{
    static const mlocMessageFunction function = {"x:parts",   7,    NULL, PartsResolve,
                                                 PartsFormat, NULL, NULL, NULL};
    mlocLocale en;
    mlocMessageFormatter formatter;
    mlocMessageOptions options = {false, mloc_messageIsolationNone, &function, 1, NULL};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &options, &formatter) == mloc_success,
          "a formatter with x:parts is made");
    Part wanted[] = {{mloc_messagePartText, 0, 1},
                     {Message(mloc_messagePartPlaceholder, 0), 1, 2},
                     {0x105, 1, 1},
                     {0x7F, 1, 1}};
    s_formats = 0;
    CHECK(PartsAre(&formatter, "<{|z| :x:parts}", NULL, 0, "<ab", wanted, 4) && s_formats == 3,
          "a program function's parts are passed on but those of a message part's kind, its "
          "format entry called again to measure and to report");
    Parts parts;
    s_formats = 0;
    mlocOutput output = {s_text, sizeof s_text, 0, NULL, NULL};
    CHECK(Format(&formatter, "<{|z| :x:parts}", NULL, 0, sizeof s_text, &parts) == mloc_success &&
              mlocFormatMessage(s_bound, NULL, 0, &output, NULL) == mloc_success && s_formats == 4,
          "without a part function the text is written once");
}

static void TestNoParts(void)
{
    Parts parts;
    Part hello[] = {{mloc_messagePartText, 0, 5}};
    CHECK(PartsAre(&s_strict, "Hello", NULL, 0, "Hello", hello, 1),
          "a strict formatter reports parts when nothing fails");
    CHECK(Format(&s_plain, "Hello {|World|}", NULL, 0, 5, &parts) == mloc_errorCapacity &&
              parts.count == 0,
          "no parts when the text does not fit");
    CHECK(Format(&s_strict, "Hello {$x}", NULL, 0, sizeof s_text, &parts) == mloc_errorFallback &&
              parts.count == 0,
          "no parts when strict formatting stops at a fault");
    mlocMessageFault list[4];
    mlocMessageFaults faults = {list, 4, 0};
    size_t size = 0;
    size_t bound = 0;
    mlocMessageError error;
    const char* message = "{$x} {$y}";
    mlocOutput output = {s_text, sizeof s_text, 0, Collect, &parts};
    memset(&parts, 0, sizeof parts);
    CHECK(mlocCompileMessage(message, strlen(message), s_compiled, sizeof s_compiled, &size, &error)
                      .status == mloc_success &&
              mlocMessageFormatter_Bind(&s_plain, s_compiled, size, s_bound, sizeof s_bound,
                                        &bound) == mloc_success &&
              mlocFormatMessage(s_bound, NULL, 0, &output, &faults) == mloc_withFallbacks &&
              faults.count == 2 && parts.count == 3,
          "faults are counted once though the text is written twice");
}

// Whether a parameter is as wanted (a null value for none).
static bool ParameterIs(const mlocMessageParameter* p, bool attribute, bool variable,
                        const char* name, const char* value)
{
    return p->attribute == attribute && p->variable == variable && p->nameLength == strlen(name) &&
           memcmp(p->name, name, p->nameLength) == 0 &&
           (value == NULL
                ? p->value == NULL && p->valueLength == 0
                : p->valueLength == strlen(value) && memcmp(p->value, value, p->valueLength) == 0);
}

static void TestAccess(void)
{
    mlocMessageArgument t = {"t", 1, {mloc_messageValueString, 0, {.string = {"T", 1}}}};
    Parts parts;
    CHECK(Format(&s_plain,
                 "{|1| :number u:id=total minimumFractionDigits=2 @translate=no}{#a href=|u| "
                 "title=$t @k}{$t}{/a}{$x}",
                 &t, 1, sizeof s_text, &parts) == mloc_withFallbacks &&
              parts.count == 8,
          "a message with options, attributes and markup formats");
    uint32_t number = parts.list[0].kind;
    uint32_t open = parts.list[4].kind;
    uint32_t bare = parts.list[5].kind;
    uint32_t fallback = parts.list[7].kind;
    mlocMessagePartInfo info;
    mlocMessageParameter p;
    CHECK(mlocGetMessagePartInfo(s_bound, number, &info) == mloc_success && info.nameLength == 6 &&
              memcmp(info.name, "number", 6) == 0 && info.parameterCount == 3 &&
              mlocFindMessagePartOption(s_bound, number, "u:id", 4, &p) == mloc_success &&
              ParameterIs(&p, false, false, "u:id", "total") &&
              mlocGetMessagePartParameter(s_bound, number, 1, &p) == mloc_success &&
              ParameterIs(&p, false, false, "minimumFractionDigits", "2") &&
              mlocGetMessagePartParameter(s_bound, number, 2, &p) == mloc_success &&
              ParameterIs(&p, true, false, "translate", "no"),
          "a placeholder's function, options and attributes, u:id found by name");
    CHECK(mlocGetMessagePartInfo(s_bound, open, &info) == mloc_success && info.nameLength == 1 &&
              memcmp(info.name, "a", 1) == 0 && info.parameterCount == 3 &&
              mlocGetMessagePartParameter(s_bound, open, 0, &p) == mloc_success &&
              ParameterIs(&p, false, false, "href", "u") &&
              mlocGetMessagePartParameter(s_bound, open, 1, &p) == mloc_success &&
              ParameterIs(&p, false, true, "title", "t") &&
              mlocGetMessagePartParameter(s_bound, open, 2, &p) == mloc_success &&
              ParameterIs(&p, true, false, "k", NULL),
          "a markup's name, a literal and a variable option, a bare attribute");
    CHECK(mlocGetMessagePartInfo(s_bound, bare, &info) == mloc_success && info.name == NULL &&
              info.nameLength == 0 && info.parameterCount == 0 &&
              mlocGetMessagePartInfo(s_bound, fallback, &info) == mloc_success && info.name == NULL,
          "an expression without a function, and a fallback's");
    CHECK(mlocFindMessagePartOption(s_bound, number, "u:dir", 5, &p) == mloc_errorNotFound &&
              mlocFindMessagePartOption(s_bound, number, "translate", 9, &p) ==
                  mloc_errorNotFound &&
              mlocFindMessagePartOption(s_bound, number, NULL, 0, &p) == mloc_errorNotFound &&
              mlocFindMessagePartOption(s_bound, number, "u:ix", 4, &p) == mloc_errorNotFound &&
              mlocFindMessagePartOption(s_bound, number, "href", 4, &p) == mloc_errorNotFound &&
              mlocGetMessagePartInfo(s_bound, parts.list[6].kind, &info) == mloc_success &&
              info.nameLength == 1 && memcmp(info.name, "a", 1) == 0,
          "an option the expression lacks or the next one has, an attribute's name, no name; a "
          "closing markup's name");
    CHECK(mlocGetMessagePartInfo(s_bound, mloc_messagePartText, &info) == mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, mloc_messagePartIsolation, &info) ==
                  mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, mloc_numberPartInteger, &info) == mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, Message(mloc_messagePartPlaceholder, 3), &info) ==
                  mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, Message(mloc_messagePartPlaceholder, 2), &info) ==
                  mloc_success &&
              mlocGetMessagePartInfo(s_bound, Message(mloc_messagePartMarkupClose, 2), &info) ==
                  mloc_errorInvalid &&
              mlocGetMessagePartInfo(NULL, number, &info) == mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_bound, number, NULL) == mloc_errorInvalid &&
              mlocGetMessagePartInfo(s_compiled, number, &info) == mloc_errorInvalid,
          "parts that name nothing, indices past the records, null pointers, no bound message");
    CHECK(mlocGetMessagePartParameter(s_bound, number, 3, &p) == mloc_errorInvalid &&
              mlocGetMessagePartParameter(s_bound, bare, 0, &p) == mloc_errorInvalid &&
              mlocGetMessagePartParameter(s_bound, number, 0, NULL) == mloc_errorInvalid &&
              mlocGetMessagePartParameter(s_bound, mloc_messagePartText, 0, &p) ==
                  mloc_errorInvalid &&
              mlocFindMessagePartOption(s_bound, number, NULL, 1, &p) == mloc_errorInvalid &&
              mlocFindMessagePartOption(s_bound, number, "u:id", 4, NULL) == mloc_errorInvalid &&
              mlocFindMessagePartOption(s_bound, mloc_messagePartText, "u:id", 4, &p) ==
                  mloc_errorInvalid,
          "parameter indices past the count, and the other refusals");
}

static void TestTiling(void)
{
    static const char* const messages[] = {
        "a {1 :number} b {|x|} {$x} {#m}c{/m}",
        ".local $n = {-1234567.891 :number} {{{$n} {$n :integer} {0.5 :percent}}}",
        "{|2006-01-02T15:04:06| :datetime dateLength=long} {12 :currency currency=EUR}",
        ".input {$n :number} .match $n 1 {{one {$n}}} * {{{$n} more}}",
    };
    mlocMessageArgument n = {"n", 1, {mloc_messageValueInteger, 0, {.integer = 3}}};
    for (size_t m = 0; m < sizeof messages / sizeof messages[0]; m++)
    {
        for (int f = 0; f < 2; f++)
        {
            Parts parts;
            mlocResult result =
                Format(f == 0 ? &s_plain : &s_isolating, messages[m], &n, 1, sizeof s_text, &parts);
            bool good = (result == mloc_success || result == mloc_withFallbacks) &&
                        parts.count <= 64 && Tiles(&parts, strlen(s_text));
            if (!good)
            {
                printf("  [%s] (%d) does not tile: %d, %zu parts\n", messages[m], f, (int)result,
                       parts.count);
            }
            CHECK(good, "the message's parts tile its text, the others inside them");
        }
    }
}

int main(void)
{
    static mlocData sources[2];
    alignas(8) static uint8_t dates[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    FILE* file = fopen(MLOC_DATES_FILE, "rb");
    size_t size = file == NULL ? 0 : fread(dates, 1, sizeof dates, file);
    if (file != NULL)
    {
        fclose(file);
    }
    if (size == 0 || size >= sizeof dates || mlocOpenData(dates, size, &sources[1]) != mloc_success)
    {
        fputs("test_message_parts: cannot open " MLOC_DATES_FILE "\n", stderr);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    mlocLocale en;
    mlocMessageOptions plain = {false, mloc_messageIsolationNone, NULL, 0, NULL};
    mlocMessageOptions isolating = {false, mloc_messageIsolationDefault, NULL, 0, NULL};
    mlocMessageOptions strict = {true, mloc_messageIsolationNone, NULL, 0, NULL};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &plain, &s_plain) == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &isolating, &s_isolating) == mloc_success &&
              mlocInitMessageFormatter(&s_data, &en, &strict, &s_strict) == mloc_success,
          "the formatters are made");
    TestPieces();
    TestInner();
    TestProgramParts();
    TestNoParts();
    TestAccess();
    TestTiling();
    return s_failures == 0 ? 0 : 1;
}
