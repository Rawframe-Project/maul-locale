// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Answers questions about Temporal's strings through the public parsers,
// for tools/check_ixdtf.py to compare with its own reading of ECMA-262's
// grammar. It reads lines from standard input, each a kind and the
// string in hexadecimal (a string may hold spaces), and writes one line
// for each, '-' for refused:
//   dt FORMATS HEX   mlocParseTemporalString: the goal, the year or '_',
//                    month, day, the time or '_', Z, the offset or '_'
//                    with whether it gave seconds, and the time zone and
//                    calendar spans as text or '_'
//   offset HEX       mlocParseUTCOffset: nanoseconds, whether seconds
//   zone HEX         mlocParseTimeZoneString: the name or '_', utc, the
//                    offset in minutes or '_'
//   calendar HEX     mlocParseCalendarString: the id or '_'
//   duration HEX     mlocParseDuration: the ten fields
//
// usage: conform_ixdtf < questions

#include "maul-locale/ixdtf.h"

#include <stdio.h>
#include <string.h>

enum
{
    maxText = 4096,
};

static size_t Unhex(const char* hex, char* out)
{
    size_t n = 0;
    while (hex[0] != '\0' && hex[1] != '\0' && n < maxText)
    {
        int v = 0;
        for (int i = 0; i < 2; i += 1)
        {
            char ch = hex[i];
            v = v * 16 + (ch >= 'a' ? ch - 'a' + 10 : ch >= 'A' ? ch - 'A' + 10 : ch - '0');
        }
        out[n] = (char)v;
        n += 1;
        hex += 2;
    }
    return n;
}

static void Span(const char* text, mlocTextSpan span, const char* before)
{
    if (span.length == 0)
    {
        printf("%s_", before);
        return;
    }
    printf("%s%.*s", before, (int)span.length, text + span.start);
}

static void DateTime(unsigned formats, const char* text, size_t length)
{
    mlocTemporalParse p;
    if (mlocParseTemporalString(text, length, formats, &p) != mloc_success)
    {
        printf("-\n");
        return;
    }
    printf("%u", (unsigned)p.format);
    if (p.hasYear)
    {
        printf(" %ld", (long)p.year);
    }
    else
    {
        printf(" _");
    }
    printf(" %u %u", (unsigned)p.month, (unsigned)p.day);
    if (p.hasTime)
    {
        printf(" %u:%u:%u.%lu", (unsigned)p.time.hour, (unsigned)p.time.minute,
               (unsigned)p.time.second, (unsigned long)p.time.nanosecond);
    }
    else
    {
        printf(" _");
    }
    printf(" %d", p.utc ? 1 : 0);
    if (p.hasOffset)
    {
        printf(" %lld/%d", (long long)p.offsetNanoseconds, p.offsetHasSeconds ? 1 : 0);
    }
    else
    {
        printf(" _");
    }
    Span(text, p.timeZone, " ");
    Span(text, p.calendar, " ");
    printf("\n");
}

static void Offset(const char* text, size_t length)
{
    int64_t ns = 0;
    bool seconds = false;
    if (mlocParseUTCOffset(text, length, &ns, &seconds) != mloc_success)
    {
        printf("-\n");
        return;
    }
    printf("%lld %d\n", (long long)ns, seconds ? 1 : 0);
}

static void Zone(const char* text, size_t length)
{
    mlocTimeZoneParse p;
    if (mlocParseTimeZoneString(text, length, &p) != mloc_success)
    {
        printf("-\n");
        return;
    }
    Span(text, p.name, "");
    printf(" %d", p.utc ? 1 : 0);
    if (p.hasOffset)
    {
        printf(" %d\n", (int)p.offsetMinutes);
    }
    else
    {
        printf(" _\n");
    }
}

static void Calendar(const char* text, size_t length)
{
    mlocTextSpan span;
    if (mlocParseCalendarString(text, length, &span) != mloc_success)
    {
        printf("-\n");
        return;
    }
    Span(text, span, "");
    printf("\n");
}

static void Duration(const char* text, size_t length)
{
    mlocDurationValues v;
    if (mlocParseDuration(text, length, &v) != mloc_success)
    {
        printf("-\n");
        return;
    }
    double f[10] = {v.years,   v.months,  v.weeks,        v.days,         v.hours,
                    v.minutes, v.seconds, v.milliseconds, v.microseconds, v.nanoseconds};
    for (int i = 0; i < 10; i += 1)
    {
        printf(i == 0 ? "%.17g" : " %.17g", f[i]);
    }
    printf("\n");
}

int main(void)
{
    static char line[2 * maxText + 64];
    static char text[maxText];
    while (fgets(line, sizeof line, stdin) != nullptr)
    {
        // The kind, for dt the goals, then x and the string's bytes in
        // hexadecimal, so that an empty string has a token too.
        char* tokens[3] = {nullptr, nullptr, nullptr};
        size_t count = 0;
        for (char* t = strtok(line, " \n"); t != nullptr && count < 3; t = strtok(nullptr, " \n"))
        {
            tokens[count] = t;
            count += 1;
        }
        bool dt = count == 3 && strcmp(tokens[0], "dt") == 0;
        const char* hex = dt ? tokens[2] : count == 2 ? tokens[1] : nullptr;
        if (hex == nullptr || hex[0] != 'x')
        {
            printf("error\n");
            continue;
        }
        size_t length = Unhex(hex + 1, text);
        if (dt)
        {
            unsigned formats = 0;
            (void)sscanf(tokens[1], "%u", &formats);
            DateTime(formats, text, length);
        }
        else if (strcmp(tokens[0], "offset") == 0)
        {
            Offset(text, length);
        }
        else if (strcmp(tokens[0], "zone") == 0)
        {
            Zone(text, length);
        }
        else if (strcmp(tokens[0], "calendar") == 0)
        {
            Calendar(text, length);
        }
        else if (strcmp(tokens[0], "duration") == 0)
        {
            Duration(text, length);
        }
        else
        {
            printf("error\n");
        }
    }
    return 0;
}
