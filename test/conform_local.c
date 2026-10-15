// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Answers local time questions through the public zone functions, for
// tools/check_local.py to compare with Python's zoneinfo reading zic's
// output for the same tzdb source. It reads lines from standard input
// and writes one line for each but "zone":
//   zone NAME     the zone the next lines ask about
//   at LOCAL      a local date-time as seconds since 1970 read as
//                 universal time: the possible instants' count, earliest
//                 and latest, then the instant chosen by compatible,
//                 earlier, later and reject ('-' for refused)
//   day LOCAL     the start of the day holding LOCAL
//   local T       the local date-time at instant T, as seconds again
// Instants are seconds since 1970; nanoseconds stay 0.
//
// usage: conform_local < questions

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

static bool DateTimeOf(long long local, mlocDateTime* out)
{
    long long days = FloorDiv(local, secondsPerDay);
    long long second = local - days * secondsPerDay;
    mlocDate date;
    mlocTime time;
    mlocTimeInput input = {
        .hour = second / 3600, .minute = second / 60 % 60, .second = second % 60};
    return mlocMakeDateFromDay(mloc_calendarGregorian, (int32_t)(days + epochDay), &date) ==
               mloc_success &&
           mlocMakeTime(&input, mloc_overflowReject, &time) == mloc_success &&
           mlocMakeDateTime(&date, &time, out) == mloc_success;
}

static void PrintInstant(mlocResult r, const mlocInstant* instant)
{
    if (r == mloc_success)
    {
        printf(" %lld", (long long)instant->seconds);
    }
    else
    {
        printf(" -");
    }
}

static void At(const mlocTimeZone* zone, long long local)
{
    mlocDateTime dateTime;
    mlocPossibleInstants p;
    if (!DateTimeOf(local, &dateTime) ||
        mlocGetPossibleInstants(zone, &dateTime, &p) != mloc_success)
    {
        printf("error\n");
        return;
    }
    printf("%u %lld %lld", (unsigned)p.count, (long long)p.earliest.seconds,
           (long long)p.latest.seconds);
    for (int d = mloc_disambiguationCompatible; d <= mloc_disambiguationReject; d++)
    {
        mlocInstant instant = {0};
        PrintInstant(mlocGetInstantFor(zone, &dateTime, (mlocDisambiguation)d, &instant), &instant);
    }
    printf("\n");
}

static void Day(const mlocTimeZone* zone, long long local)
{
    mlocDateTime dateTime;
    mlocInstant instant = {0};
    mlocResult r = DateTimeOf(local, &dateTime) ? mlocGetStartOfDay(zone, &dateTime.date, &instant)
                                                : mloc_errorInvalid;
    PrintInstant(r, &instant);
    printf("\n");
}

static void Local(const mlocTimeZone* zone, long long seconds)
{
    mlocInstant instant = {.seconds = seconds};
    mlocDateTime dateTime;
    if (mlocGetDateTimeFor(zone, &instant, mloc_calendarGregorian, &dateTime) != mloc_success)
    {
        printf("error\n");
        return;
    }
    const mlocTime* t = &dateTime.time;
    long long local = ((long long)dateTime.date.day - epochDay) * secondsPerDay + t->hour * 3600 +
                      t->minute * 60 + t->second;
    printf("%lld\n", local);
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
        char word[16];
        char name[128];
        long long value = 0;
        if (sscanf(line, "zone %127s", name) == 1)
        {
            if (mlocFindTimeZone(&data, name, strlen(name), &zone) != mloc_success)
            {
                fprintf(stderr, "no zone %s\n", name);
                return 1;
            }
        }
        else if (sscanf(line, "%15s %lld", word, &value) == 2)
        {
            if (strcmp(word, "at") == 0)
            {
                At(&zone, value);
            }
            else if (strcmp(word, "day") == 0)
            {
                Day(&zone, value);
            }
            else if (strcmp(word, "local") == 0)
            {
                Local(&zone, value);
            }
            else
            {
                printf("error\n");
            }
        }
    }
    return 0;
}
