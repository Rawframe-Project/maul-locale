// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message part's benchmark cases (bench_messages.h), all in en with
// the default isolation, as ICU4C's MF2 isolates by default: the plural
// message compiled and bound; "Hello, {$name}!" with "Ayşe"; "You have
// {$n :number} new messages." with 3; a plural .match on $n :number; and
// two selectors, :string on a gender and :number on a count. Then a
// message for each function family, on every data file: :currency on
// 42.5 euros, :percent on 0.25, :unit on 3.5 kilometers, :date short and
// :datetime medium on 15:04:05 on 17 October 2026, mloc:spellout on
// 1234, mloc:duration digital on 3723 seconds, mloc:relative on 3 days,
// mloc:list on three names; the plural message with its parts reported;
// a .match naming six functions whose chosen variant names none, against
// the same match and keys naming none; the hello message bound to a formatter
// with 64 program functions; and binding the six-function message. The
// stack is measured by painting a region below the caller's frame,
// calling, and finding the deepest byte changed.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "bench_messages.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"
#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char s_hello[] = "Hello, {$name}!";
static const char s_number[] = "You have {$n :number} new messages.";
static const char s_plural[] = ".input {$n :number} .match $n 0 {{No messages.}} one {{One "
                               "message.}} * {{{$n} messages.}}";
static const char s_two[] =
    ".input {$g :string} .input {$n :number} .match $g $n female one {{She has {$n} cat.}} "
    "female * {{She has {$n} cats.}} * one {{They have {$n} cat.}} * * {{They have {$n} cats.}}";

static const char s_wide[] =
    ".input {$n :integer} .match $n 0 {{{$c :currency}}} 1 {{{$x :percent}}} 2 {{{$x :unit "
    "unit=kilometer}}} 4 {{{$d :date length=short}}} 5 {{{$x :mloc:spellout}}} 6 {{{$l "
    ":mloc:list}}} * {{Hello, {$name}!}}";
static const char s_narrow[] = ".input {$n :integer} .match $n 0 {{a}} 1 {{b}} 2 {{c}} 4 {{d}} 5 "
                               "{{e}} 6 {{f}} * {{Hello, {$name}!}}";

enum
{
    messageCount = 15,
    messageRoom = 16384,
    // The messages bound to the formatter on every data file, from here.
    messageFull = 4,
    // The one bound to the formatter with program functions.
    messageTable = 14,
    functionCount = 64,
};

static const char* const s_sources[messageCount] = {
    s_hello,
    s_number,
    s_plural,
    s_two,
    "Total: {$c :currency}",
    "{$x :percent} done",
    "{$x :unit unit=kilometer}",
    "{$d :date length=short}",
    "{$d :datetime}",
    "{$y :mloc:spellout}",
    "{$s :mloc:duration style=digital}",
    "{$n :mloc:relative unit=day}",
    "{$l :mloc:list}",
    s_wide,
    s_hello,
};
static mlocMessageFormatter s_formatter;
static mlocMessageFormatter s_full;
static mlocMessageFormatter s_table;
static uint8_t s_narrowBound[messageRoom];
static size_t s_wideSize;
alignas(8) static uint8_t s_narrowCompiled[messageRoom];
alignas(8) static uint8_t s_compiled[messageCount][messageRoom];
static size_t s_compiledSize[messageCount];
alignas(8) static uint8_t s_bound[messageCount][messageRoom];
alignas(8) static uint8_t s_scratch[messageRoom];
static char s_text[256];
static size_t s_length;
static volatile size_t s_sink;

static mlocDecimal s_amount;
static mlocDecimal s_quarter;
static mlocDateTime s_dateTime;
static const mlocListItem s_names[] = {{"Ann", 3}, {"Bob", 3}, {"Cem", 3}};

static mlocMessageArgument s_arguments[] = {
    {"name",
     4,
     {mloc_messageValueString,
      0,
      {.string = {"Ay\xC5\x9F"
                  "e",
                  5}}}},
    {"n", 1, {mloc_messageValueInteger, 0, {.integer = 3}}},
    {"g", 1, {mloc_messageValueString, 0, {.string = {"female", 6}}}},
    {"c", 1, {mloc_messageValueCurrency, 0, {.currency = {&s_amount, {'E', 'U', 'R'}}}}},
    {"x", 1, {mloc_messageValueDecimal, 0, {.decimal = &s_quarter}}},
    {"d", 1, {mloc_messageValueDateTime, 0, {.integer = 0}}},
    {"y", 1, {mloc_messageValueInteger, 0, {.integer = 1234}}},
    {"s", 1, {mloc_messageValueInteger, 0, {.integer = 3723}}},
    {"l", 1, {mloc_messageValueList, 0, {.list = {s_names, 3}}}},
};
enum
{
    argumentCount = sizeof(s_arguments) / sizeof(s_arguments[0]),
};

static void Write(int message)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult result =
        mlocFormatMessage(s_bound[message], s_arguments, argumentCount, &output, nullptr);
    s_length = output.length;
    s_sink += (size_t)result + output.length;
}

// Writes a bound message, its parts reported to a function that counts
// them.
static size_t s_parts;

static void CountPart(void* context, uint32_t part, size_t start, size_t length)
{
    (void)context;
    (void)part;
    (void)start;
    (void)length;
    s_parts++;
}

static void WriteParts(const uint8_t* bound)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, CountPart, nullptr};
    mlocResult result = mlocFormatMessage(bound, s_arguments, argumentCount, &output, nullptr);
    s_length = output.length;
    s_sink += (size_t)result + output.length + s_parts;
}

static void WriteBound(const uint8_t* bound)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult result = mlocFormatMessage(bound, s_arguments, argumentCount, &output, nullptr);
    s_length = output.length;
    s_sink += (size_t)result + output.length;
}

static void Compile(void)
{
    size_t size = 0;
    mlocMessageError error;
    mlocTextResult result = mlocCompileMessage(s_plural, sizeof(s_plural) - 1, s_scratch,
                                               sizeof(s_scratch), &size, &error);
    s_sink += size + (size_t)result.status;
}

static void Bind(void)
{
    size_t size = 0;
    mlocResult result = mlocBindMessage(&s_formatter, s_compiled[2], s_compiledSize[2], s_scratch,
                                        sizeof(s_scratch), &size);
    s_sink += size + (size_t)result;
}

static void Hello(void)
{
    Write(0);
}

static void Number(void)
{
    Write(1);
}

static void Plural(void)
{
    Write(2);
}

static void Two(void)
{
    Write(3);
}

static void Currency(void)
{
    Write(4);
}

static void Percent(void)
{
    Write(5);
}

static void Unit(void)
{
    Write(6);
}

static void Date(void)
{
    Write(7);
}

static void DateTime(void)
{
    Write(8);
}

static void Spellout(void)
{
    Write(9);
}

static void Duration(void)
{
    Write(10);
}

static void Relative(void)
{
    Write(11);
}

static void List(void)
{
    Write(12);
}

static void Parts(void)
{
    WriteParts(s_bound[2]);
}

static void Wide(void)
{
    Write(13);
}

static void Narrow(void)
{
    WriteBound(s_narrowBound);
}

static void Table(void)
{
    Write(messageTable);
}

static void BindWide(void)
{
    size_t size = 0;
    mlocResult result =
        mlocBindMessage(&s_full, s_compiled[13], s_wideSize, s_scratch, sizeof(s_scratch), &size);
    s_sink += size + (size_t)result;
}

const BenchCase g_messageCases[] = {
    {"message-compile", Compile, 500000},    {"message-bind", Bind, 500000},
    {"message-hello", Hello, 2000000},       {"message-number", Number, 1000000},
    {"message-plural", Plural, 500000},      {"message-two", Two, 500000},
    {"message-currency", Currency, 1000000}, {"message-percent", Percent, 1000000},
    {"message-unit", Unit, 1000000},         {"message-date", Date, 1000000},
    {"message-datetime", DateTime, 1000000}, {"message-spellout", Spellout, 500000},
    {"message-duration", Duration, 1000000}, {"message-relative", Relative, 1000000},
    {"message-list", List, 1000000},         {"message-parts", Parts, 500000},
    {"message-wide", Wide, 1000000},         {"message-narrow", Narrow, 1000000},
    {"message-table", Table, 2000000},       {"message-bind-wide", BindWide, 50000},
};
const size_t g_messageCaseCount = sizeof(g_messageCases) / sizeof(g_messageCases[0]);

// Compiles a message and binds it to a formatter into its slot; false
// when it fails.
static bool MakeFor(const mlocMessageFormatter* formatter, const char* source, uint8_t* compiled,
                    size_t compiledCapacity, size_t* compiledSize, uint8_t* bound,
                    size_t boundCapacity)
{
    size_t size = 0;
    mlocMessageError error;
    return mlocCompileMessage(source, strlen(source), compiled, compiledCapacity, compiledSize,
                              &error)
                   .status == mloc_success &&
           mlocBindMessage(formatter, compiled, *compiledSize, bound, boundCapacity, &size) ==
               mloc_success;
}

static bool Make(const char* source, uint8_t* compiled, size_t compiledCapacity,
                 size_t* compiledSize, uint8_t* bound, size_t boundCapacity)
{
    return MakeFor(&s_formatter, source, compiled, compiledCapacity, compiledSize, bound,
                   boundCapacity);
}

// A program function that is never called: the table's entries.
static mlocMessageFaultKind Unused(void* context, const mlocMessageCall* call,
                                   const mlocMessageOperand* operand, mlocMessageResolved* valueOut)
{
    (void)context;
    (void)call;
    (void)operand;
    (void)valueOut;
    return mloc_messageFaultFunction;
}

// The formatter with program functions x:f00 to x:f63.
static bool MakeTable(const mlocDataSet* data, const mlocLocale* en)
{
    static char names[functionCount][8];
    static mlocMessageFunction functions[functionCount];
    for (int i = 0; i < functionCount; i++)
    {
        snprintf(names[i], sizeof(names[i]), "x:f%02d", i);
        functions[i] =
            (mlocMessageFunction){names[i], 5, nullptr, Unused, nullptr, nullptr, nullptr, nullptr};
    }
    mlocMessageOptions options = {false, mloc_messageIsolationDefault, functions, functionCount,
                                  nullptr};
    return mlocInitMessageFormatter(&s_table, data, en, &options) == mloc_success;
}

// The values the function families' messages read.
static bool MakeValues(void)
{
    mlocDateInput day = {.year = 2026, .month = 10, .day = 17};
    s_dateTime.time = (mlocTime){.hour = 15, .minute = 4, .second = 5};
    if (mlocParseDecimal("42.5", 4, &s_amount).status != mloc_success ||
        mlocParseDecimal("0.25", 4, &s_quarter).status != mloc_success ||
        mlocMakeDate(mloc_calendarIso8601, &day, mloc_overflowReject, &s_dateTime.date) !=
            mloc_success)
    {
        return false;
    }
    for (size_t i = 0; i < argumentCount; i++)
    {
        if (s_arguments[i].value.kind == mloc_messageValueDateTime)
        {
            s_arguments[i].value.as.dateTime = s_dateTime;
        }
    }
    return true;
}

bool PrepareMessageBench(void)
{
    static mlocData builtin;
    static mlocDataSet data;
    mlocOpenBuiltinData(&builtin);
    data = (mlocDataSet){&builtin, 1};
    mlocLocale en;
    if (mlocParseLocale("en", 2, &en).status != mloc_success ||
        mlocInitMessageFormatter(&s_formatter, &data, &en, nullptr) != mloc_success)
    {
        return false;
    }
    const mlocDataSet* all = BenchDataSet();
    if (!MakeValues() || mlocInitMessageFormatter(&s_full, all, &en, nullptr) != mloc_success ||
        !MakeTable(all, &en))
    {
        return false;
    }
    for (int i = 0; i < messageCount; i++)
    {
        const mlocMessageFormatter* formatter = i == messageTable  ? &s_table
                                                : i >= messageFull ? &s_full
                                                                   : &s_formatter;
        if (!MakeFor(formatter, s_sources[i], s_compiled[i], messageRoom, &s_compiledSize[i],
                     s_bound[i], messageRoom))
        {
            fprintf(stderr, "bench: cannot make [%s]\n", s_sources[i]);
            return false;
        }
    }
    size_t size = 0;
    s_wideSize = s_compiledSize[13];
    return MakeFor(&s_full, s_narrow, s_narrowCompiled, messageRoom, &size, s_narrowBound,
                   messageRoom);
}

void ShowMessageBench(void)
{
    for (int i = 0; i < messageCount; i++)
    {
        Write(i);
        printf("[%.*s]\n", (int)s_length, s_text);
    }
    void (*const more[])(void) = {Parts, Wide, Narrow};
    for (size_t i = 0; i < sizeof(more) / sizeof(more[0]); i++)
    {
        s_parts = 0;
        more[i]();
        printf("[%.*s] %zu parts\n", (int)s_length, s_text, s_parts);
    }
}

enum
{
    paintSize = 1 << 17,
    paint = 0xA5,
};

// The painted region's address, kept as a number: it is read after the
// frame that held it returns, as the call measured runs over it.
static volatile uintptr_t s_painted;

// Fills a region of the stack below the caller's frame and keeps its
// address; the call measured then runs over it.
static void Paint(void)
{
    volatile uint8_t area[paintSize];
    for (size_t i = 0; i < paintSize; i++)
    {
        area[i] = paint;
    }
    s_painted = (uintptr_t)area;
}

// The bytes of the painted region the call changed, from its deepest
// change up: the stack grows down, so the lowest changed byte.
static size_t Used(void)
{
    const volatile uint8_t* area = (const volatile uint8_t*)s_painted;
    size_t first = 0;
    while (first < paintSize && area[first] == paint)
    {
        first++;
    }
    return paintSize - first;
}

// Called through a pointer, so its frame is not merged into the caller's.
static void (*volatile s_paint)(void) = Paint;

// The stack a format call of a bound message takes.
static size_t StackOf(const uint8_t* bound)
{
    s_paint();
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult result = mlocFormatMessage(bound, s_arguments, argumentCount, &output, nullptr);
    s_sink += (size_t)result;
    return Used();
}

// Appends printf's output to a text of a capacity at *length; false, with
// the text before it unchanged, when it does not fit.
[[gnu::format(printf, 4, 5)]] static bool AppendText(char* text, size_t capacity, size_t* length,
                                                     const char* format, ...)
{
    if (*length >= capacity)
    {
        return false;
    }
    va_list arguments;
    va_start(arguments, format);
    int n = vsnprintf(text + *length, capacity - *length, format, arguments);
    va_end(arguments);
    if (n < 0 || (size_t)n >= capacity - *length)
    {
        text[*length] = '\0';
        return false;
    }
    *length += (size_t)n;
    return true;
}

bool PrintMessageStack(void)
{
    // .local $a0 = {1 :number} .local $a1 = {$a0 :number} ... and the
    // last named in the pattern: 64 expressions, the longest chain.
    static char source[4096];
    size_t length = 0;
    bool fits = AppendText(source, sizeof(source), &length, "%s", ".local $a0 = {1 :number}");
    for (int i = 1; fits && i < 63; i++)
    {
        fits =
            AppendText(source, sizeof(source), &length, " .local $a%d = {$a%d :number}", i, i - 1);
    }
    fits = fits && AppendText(source, sizeof(source), &length, "%s", " {{{$a62 :number}}}");
    if (!fits)
    {
        return false;
    }
    static uint8_t compiled[16384];
    // Each :number's value is prepared in the bound message.
    static uint8_t bound[65536];
    size_t compiledSize = 0;
    if (!Make(source, compiled, sizeof(compiled), &compiledSize, bound, sizeof(bound)))
    {
        return false;
    }
    printf("%-18s %10zu bytes\n", "stack-plural", StackOf(s_bound[2]));
    printf("%-18s %10zu bytes\n", "stack-chain-64", StackOf(bound));
    // Through options: .local $a1 = {1 :number maximumFractionDigits=$a0}
    // and so on, as long a chain as binding takes (an option is resolved
    // twice, so the resolutions double at each level).
    length = 0;
    fits = AppendText(source, sizeof(source), &length, "%s", ".local $a0 = {1 :number}");
    for (int i = 1; fits && i < 14; i++)
    {
        fits = AppendText(source, sizeof(source), &length,
                          " .local $a%d = {1 :number maximumFractionDigits=$a%d}", i, i - 1);
    }
    fits = fits && AppendText(source, sizeof(source), &length, "%s", " {{{$a13 :number}}}");
    if (!fits || !Make(source, compiled, sizeof(compiled), &compiledSize, bound, sizeof(bound)))
    {
        return false;
    }
    printf("%-18s %10zu bytes\n", "stack-options-15", StackOf(bound));
    return true;
}
