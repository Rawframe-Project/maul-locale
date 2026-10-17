// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message part's benchmark cases (bench_messages.h), all in en with
// the default isolation, as ICU4C's MF2 isolates by default: the plural
// message compiled and bound; "Hello, {$name}!" with "Ayşe"; "You have
// {$n :number} new messages." with 3; a plural .match on $n :number; and
// two selectors, :string on a gender and :number on a count. The stack
// is measured by painting a region below the caller's frame, calling,
// and finding the deepest byte changed.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "bench_messages.h"

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/message.h"

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

enum
{
    messageCount = 4,
    messageRoom = 4096,
};

static const char* const s_sources[messageCount] = {s_hello, s_number, s_plural, s_two};
static mlocMessageFormatter s_formatter;
static uint8_t s_compiled[messageCount][messageRoom];
static size_t s_compiledSize[messageCount];
static uint8_t s_bound[messageCount][messageRoom];
static uint8_t s_scratch[messageRoom];
static char s_text[256];
static size_t s_length;
static volatile size_t s_sink;

static const mlocMessageArgument s_arguments[] = {
    {"name",
     4,
     {mloc_messageValueString,
      0,
      {.string = {"Ay\xC5\x9F"
                  "e",
                  5}}}},
    {"n", 1, {mloc_messageValueInteger, 0, {.integer = 3}}},
    {"g", 1, {mloc_messageValueString, 0, {.string = {"female", 6}}}},
};

static void Write(int message)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult result = mlocFormatMessage(s_bound[message], s_arguments, 3, &output, nullptr);
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

const BenchCase g_messageCases[] = {
    {"message-compile", Compile, 500000}, {"message-bind", Bind, 500000},
    {"message-hello", Hello, 2000000},    {"message-number", Number, 1000000},
    {"message-plural", Plural, 500000},   {"message-two", Two, 500000},
};
const size_t g_messageCaseCount = sizeof(g_messageCases) / sizeof(g_messageCases[0]);

// Compiles and binds a message into its slot; false when it fails.
static bool Make(const char* source, uint8_t* compiled, size_t compiledCapacity,
                 size_t* compiledSize, uint8_t* bound, size_t boundCapacity)
{
    size_t size = 0;
    mlocMessageError error;
    return mlocCompileMessage(source, strlen(source), compiled, compiledCapacity, compiledSize,
                              &error)
                   .status == mloc_success &&
           mlocBindMessage(&s_formatter, compiled, *compiledSize, bound, boundCapacity, &size) ==
               mloc_success;
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
    for (int i = 0; i < messageCount; i++)
    {
        if (!Make(s_sources[i], s_compiled[i], messageRoom, &s_compiledSize[i], s_bound[i],
                  messageRoom))
        {
            fprintf(stderr, "bench: cannot make [%s]\n", s_sources[i]);
            return false;
        }
    }
    return true;
}

void ShowMessageBench(void)
{
    for (int i = 0; i < messageCount; i++)
    {
        Write(i);
        printf("[%.*s]\n", (int)s_length, s_text);
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
    mlocResult result = mlocFormatMessage(bound, s_arguments, 3, &output, nullptr);
    s_sink += (size_t)result;
    return Used();
}

bool PrintMessageStack(void)
{
    // .local $a0 = {1 :number} .local $a1 = {$a0 :number} ... and the
    // last named in the pattern: 64 expressions, the longest chain.
    static char source[4096];
    size_t length = (size_t)snprintf(source, sizeof(source), ".local $a0 = {1 :number}");
    for (int i = 1; i < 63; i++)
    {
        length += (size_t)snprintf(source + length, sizeof(source) - length,
                                   " .local $a%d = {$a%d :number}", i, i - 1);
    }
    snprintf(source + length, sizeof(source) - length, " {{{$a62 :number}}}");
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
    return true;
}
