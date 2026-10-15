// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Answers questions about the arithmetic of zoned date-times through the
// public zone functions, for tools/check_zoned.py to compare with its own
// reading of ECMA-262 over Python's zoneinfo. It reads lines from
// standard input and writes one line for each but "zone", '-' for
// refused:
//   zone NAME                         the zone the next lines ask about
//   until S N S N LARGEST SMALLEST INCREMENT MODE
//                                     mlocZonedDateTimeUntil in the ISO
//                                     calendar: years, months, weeks,
//                                     days, seconds and nanoseconds
//   add S N Y M W D S N OVERFLOW      mlocAddToZonedDateTime (overflow 0
//                                     constrain, 1 reject): the instant
//   round S N UNIT INCREMENT MODE     mlocRoundZonedDateTime: the instant
//   total S N S N UNIT                mlocZonedDateTimeTotal in the ISO
//                                     calendar: the float64's bits in
//                                     hexadecimal
//   offset LOCAL NANOSECOND OFFSET OPTION DISAMBIGUATION
//                                     mlocGetInstantForOffset of a local
//                                     date-time (seconds since 1970 read
//                                     as universal time): the instant
// Instants are seconds since 1970 and the nanoseconds past them; units,
// modes and options are the enums' values.
//
// usage: conform_zoned < questions

#include "maul-locale/zone.h"

#include <stdio.h>
#include <string.h>

enum
{
    secondsPerDay = 86400,
    epochDay = 719163,
};

static long long FloorDiv(long long a, long long b)
{
    return a / b - (a % b < 0 ? 1 : 0);
}

static void PrintInstant(mlocResult r, const mlocInstant* instant)
{
    if (r == mloc_success)
    {
        printf("%lld %ld\n", (long long)instant->seconds, (long)instant->nanoseconds);
    }
    else
    {
        printf("-\n");
    }
}

static void Until(const mlocTimeZone* zone, const char* line)
{
    long long s1 = 0;
    long long s2 = 0;
    long n1 = 0;
    long n2 = 0;
    int largest = 0;
    int smallest = 0;
    unsigned increment = 0;
    int mode = 0;
    mlocDateTimeDuration d = {0};
    if (sscanf(line, "until %lld %ld %lld %ld %d %d %u %d", &s1, &n1, &s2, &n2, &largest, &smallest,
               &increment, &mode) != 8)
    {
        printf("error\n");
        return;
    }
    mlocInstant one = {s1, (int32_t)n1};
    mlocInstant two = {s2, (int32_t)n2};
    mlocDateTimeRounding r = {(mlocUnit)largest, (mlocUnit)smallest, increment,
                              (mlocRoundingMode)mode};
    if (mlocZonedDateTimeUntil(zone, mloc_calendarGregorian, &one, &two, &r, &d) != mloc_success)
    {
        printf("-\n");
        return;
    }
    printf("%lld %lld %lld %lld %lld %ld\n", (long long)d.date.years, (long long)d.date.months,
           (long long)d.date.weeks, (long long)d.date.days, (long long)d.time.seconds,
           (long)d.time.nanoseconds);
}

static void Total(const mlocTimeZone* zone, const char* line)
{
    long long s1 = 0;
    long long s2 = 0;
    long n1 = 0;
    long n2 = 0;
    int unit = 0;
    double total = 0;
    if (sscanf(line, "total %lld %ld %lld %ld %d", &s1, &n1, &s2, &n2, &unit) != 5)
    {
        printf("error\n");
        return;
    }
    mlocInstant one = {s1, (int32_t)n1};
    mlocInstant two = {s2, (int32_t)n2};
    if (mlocZonedDateTimeTotal(zone, mloc_calendarGregorian, &one, &two, (mlocUnit)unit, &total) !=
        mloc_success)
    {
        printf("-\n");
        return;
    }
    uint64_t bits;
    memcpy(&bits, &total, sizeof bits);
    printf("%016llx\n", (unsigned long long)bits);
}

static void Add(const mlocTimeZone* zone, const char* line)
{
    long long s = 0;
    long n = 0;
    long long v[6] = {0};
    int overflow = 0;
    if (sscanf(line, "add %lld %ld %lld %lld %lld %lld %lld %lld %d", &s, &n, &v[0], &v[1], &v[2],
               &v[3], &v[4], &v[5], &overflow) != 9)
    {
        printf("error\n");
        return;
    }
    mlocInstant start = {s, (int32_t)n};
    mlocDateTimeDuration d = {{v[0], v[1], v[2], v[3]}, {v[4], (int32_t)v[5]}};
    mlocInstant out = {0};
    PrintInstant(mlocAddToZonedDateTime(zone, mloc_calendarGregorian, &start, &d,
                                        (mlocOverflow)overflow, &out),
                 &out);
}

static void Round(const mlocTimeZone* zone, const char* line)
{
    long long s = 0;
    long n = 0;
    int unit = 0;
    unsigned increment = 0;
    int mode = 0;
    if (sscanf(line, "round %lld %ld %d %u %d", &s, &n, &unit, &increment, &mode) != 5)
    {
        printf("error\n");
        return;
    }
    mlocInstant start = {s, (int32_t)n};
    mlocInstant out = {0};
    PrintInstant(mlocRoundZonedDateTime(zone, &start, (mlocTimeUnit)unit, increment,
                                        (mlocRoundingMode)mode, &out),
                 &out);
}

static void Offset(const mlocTimeZone* zone, const char* line)
{
    long long local = 0;
    long nanosecond = 0;
    long long offset = 0;
    int option = 0;
    int disambiguation = 0;
    if (sscanf(line, "offset %lld %ld %lld %d %d", &local, &nanosecond, &offset, &option,
               &disambiguation) != 5)
    {
        printf("error\n");
        return;
    }
    long long days = FloorDiv(local, secondsPerDay);
    long long second = local - days * secondsPerDay;
    mlocDate date;
    mlocTime time;
    mlocDateTime dateTime;
    mlocTimeInput input = {.hour = second / 3600,
                           .minute = second / 60 % 60,
                           .second = second % 60,
                           .nanosecond = nanosecond};
    mlocInstant out = {0};
    if (mlocMakeDateFromDay(mloc_calendarGregorian, (int32_t)(days + epochDay), &date) !=
            mloc_success ||
        mlocMakeTime(&input, mloc_overflowReject, &time) != mloc_success ||
        mlocMakeDateTime(&date, &time, &dateTime) != mloc_success)
    {
        printf("error\n");
        return;
    }
    PrintInstant(mlocGetInstantForOffset(zone, &dateTime, offset, (mlocOffsetOption)option,
                                         (mlocDisambiguation)disambiguation, false, &out),
                 &out);
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    mlocTimeZone zone = {0};
    char line[256];
    while (fgets(line, sizeof line, stdin) != nullptr)
    {
        char name[128];
        if (sscanf(line, "zone %127s", name) == 1)
        {
            if (mlocFindTimeZone(&data, name, strlen(name), &zone) != mloc_success)
            {
                fprintf(stderr, "no zone %s\n", name);
                return 1;
            }
        }
        else if (strncmp(line, "until ", 6) == 0)
        {
            Until(&zone, line);
        }
        else if (strncmp(line, "total ", 6) == 0)
        {
            Total(&zone, line);
        }
        else if (strncmp(line, "add ", 4) == 0)
        {
            Add(&zone, line);
        }
        else if (strncmp(line, "round ", 6) == 0)
        {
            Round(&zone, line);
        }
        else if (strncmp(line, "offset ", 7) == 0)
        {
            Offset(&zone, line);
        }
        else
        {
            printf("error\n");
        }
    }
    return 0;
}
