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
// and for the writers, which write their string or '-' for refused, each
// with the precision as 0 to 9 digits, -1 for auto or -2 for minutes:
//   wdate DAY CAL PREC SHOW           mlocFormatTemporalDate (wym, wmd:
//                                     mlocFormatYearMonth, MonthDay)
//   wtime H M S NS PREC               mlocFormatTemporalTime
//   wdt DAY H M S NS CAL PREC SHOW    mlocFormatTemporalDateTime
//   winst SEC NS ZONE PREC            mlocFormatInstant, the zone an offset
//                                     in minutes or '-' for none
//   wzdt ZONE SEC NS CAL PREC SHOW TZ HIDE  mlocFormatZonedDateTime
//   wdur F0 ... F9 PREC               mlocFormatTemporalDuration
//   woffset NS                        mlocFormatUTCOffset
//
// usage: conform_ixdtf < questions

#include "maul-locale/ixdtf.h"

#include <stdio.h>
#include <stdlib.h>
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

static mlocTemporalStringOptions Options(const char* calendar, long precision, long show, long zone,
                                         long hide)
{
    return (mlocTemporalStringOptions){
        .calendar = calendar,
        .calendarLength = strlen(calendar),
        .secondsPrecision = precision == -1   ? mloc_secondsAuto
                            : precision == -2 ? mloc_secondsMinute
                                              : mloc_secondsDigits,
        .fractionDigits = precision >= 0 ? (uint8_t)precision : 0,
        .showCalendar = (uint8_t)show,
        .showTimeZone = (uint8_t)zone,
        .hideOffset = hide != 0,
    };
}

static bool DateOf(long day, mlocDate* out)
{
    return mlocMakeDateFromDay(mloc_calendarGregorian, (int32_t)day, out) == mloc_success;
}

// Runs a writer's question; false for a malformed one.
static bool Write(char** t, int count, mlocOutput* out, mlocResult* r)
{
    long v[12] = {0};
    for (int i = 1; i < count && i < 12; i += 1)
    {
        v[i] = strtol(t[i], nullptr, 10);
    }
    mlocTemporalStringOptions o;
    mlocDate date;
    mlocDateTime dt;
    mlocTimeZone zone = {0};
    *r = mloc_errorInvalid;
    if ((strcmp(t[0], "wdate") == 0 || strcmp(t[0], "wym") == 0 || strcmp(t[0], "wmd") == 0) &&
        count == 5)
    {
        o = Options(t[2], v[3], v[4], 0, 0);
        if (DateOf(v[1], &date))
        {
            *r = t[0][1] == 'd'   ? mlocFormatTemporalDate(&date, &o, out)
                 : t[0][1] == 'y' ? mlocFormatYearMonth(&date, &o, out)
                                  : mlocFormatMonthDay(&date, &o, out);
        }
        return true;
    }
    if (strcmp(t[0], "wtime") == 0 && count == 6)
    {
        mlocTime time = {(uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3], (uint32_t)v[4]};
        o = Options("iso8601", v[5], 0, 0, 0);
        *r = mlocFormatTemporalTime(&time, &o, out);
        return true;
    }
    if (strcmp(t[0], "wdt") == 0 && count == 9)
    {
        dt.time = (mlocTime){(uint8_t)v[2], (uint8_t)v[3], (uint8_t)v[4], (uint32_t)v[5]};
        o = Options(t[6], v[7], v[8], 0, 0);
        if (DateOf(v[1], &dt.date))
        {
            *r = mlocFormatTemporalDateTime(&dt, &o, out);
        }
        return true;
    }
    if (strcmp(t[0], "winst") == 0 && count == 5)
    {
        mlocInstant instant = {strtoll(t[1], nullptr, 10), (int32_t)v[2]};
        zone.offsetMinutes = (int16_t)v[3];
        o = Options("iso8601", v[4], 0, 0, 0);
        *r = mlocFormatInstant(&instant, strcmp(t[3], "-") == 0 ? nullptr : &zone, &o, out);
        return true;
    }
    if (strcmp(t[0], "wzdt") == 0 && count == 9)
    {
        mlocInstant instant = {strtoll(t[2], nullptr, 10), (int32_t)v[3]};
        zone.offsetMinutes = (int16_t)v[1];
        o = Options(t[4], v[5], v[6], v[7], v[8]);
        *r = mlocFormatZonedDateTime(&zone, &instant, &o, out);
        return true;
    }
    if (strcmp(t[0], "wdur") == 0 && count == 12)
    {
        double f[10];
        for (int i = 0; i < 10; i += 1)
        {
            f[i] = strtod(t[1 + i], nullptr);
        }
        mlocDurationValues values = {f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]};
        o = Options("iso8601", v[11], 0, 0, 0);
        *r = mlocFormatTemporalDuration(&values, &o, out);
        return true;
    }
    if (strcmp(t[0], "woffset") == 0 && count == 2)
    {
        *r = mlocFormatUTCOffset(strtoll(t[1], nullptr, 10), out);
        return true;
    }
    return false;
}

static void Writer(char* line)
{
    char* t[16];
    int count = 0;
    for (char* p = strtok(line, " \n"); p != nullptr && count < 16; p = strtok(nullptr, " \n"))
    {
        t[count] = p;
        count += 1;
    }
    char text[512];
    mlocOutput out = {.buffer = text, .capacity = sizeof text};
    mlocResult r = mloc_errorInvalid;
    if (count == 0 || !Write(t, count, &out, &r))
    {
        printf("error\n");
        return;
    }
    if (r != mloc_success)
    {
        printf("-\n");
        return;
    }
    printf("%.*s\n", (int)out.length, text);
}

int main(void)
{
    static char line[2 * maxText + 64];
    static char text[maxText];
    while (fgets(line, sizeof line, stdin) != nullptr)
    {
        if (line[0] == 'w')
        {
            Writer(line);
            continue;
        }
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
