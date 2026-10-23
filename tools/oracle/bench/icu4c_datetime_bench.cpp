// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: the datetime part's work,
// the same inputs as maul-locale's bench/bench_dates.c, all in en. Best of
// five runs after a warm-up of a tenth, nanoseconds per call. Formatters
// are made from the same skeletons a Maul option set compiles to (yMMMd,
// jm, yMMMdjm, yMMMdjmz); UnicodeString output into one reused buffer.
// ICU4C has no duration formatter (the duration rows are ICU4X's alone).
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>
#include <unicode/calendar.h>
#include <unicode/datefmt.h>
#include <unicode/dtitvfmt.h>
#include <unicode/reldatefmt.h>
#include <unicode/smpdtfmt.h>
#include <unicode/timezone.h>

using namespace icu;

static volatile int sink;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

template <typename F> static void Time(const char* name, long iters, F body)
{
    for (long i = 0; i < iters / 10; ++i)
    {
        body();
    }
    double best = 1e30;
    for (int run = 0; run < 5; ++run)
    {
        double start = now();
        for (long i = 0; i < iters; ++i)
        {
            body();
        }
        double ns = (now() - start) / (double)iters;
        best = ns < best ? ns : best;
    }
    printf("%-18s %10.1f ns\n", name, best);
}

static void Show(const UnicodeString& s)
{
    std::string u;
    s.toUTF8String(u);
    printf("  -> %s\n", u.c_str());
}

int main()
{
    UErrorCode e = U_ZERO_ERROR;
    Locale en("en");
    // The wall times 2026-10-17 15:04:05 in UTC (for the zone-less
    // formatters, whose zone is set to UTC) and 19:04:05Z in New York.
    UDate local = 1792249445000.0;
    UDate instant = 1792263845000.0;
    UDate rangeStart = 1791158400000.0; // 2026-10-05T00:00Z
    UDate rangeEnd = 1791417600000.0;   // 2026-10-08T00:00Z
    TimeZone* utc = TimeZone::createTimeZone("UTC");
    UnicodeString out;
    Time("date-init", 200000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             std::unique_ptr<DateFormat> f(DateFormat::createInstanceForSkeleton("yMMMd", en, s));
             sink = s;
         });
    std::unique_ptr<DateFormat> date(DateFormat::createInstanceForSkeleton("yMMMd", en, e));
    date->setTimeZone(*utc);
    Time("date-format", 2000000,
         [&]
         {
             out.remove();
             date->format(local, out);
             sink = out.length();
         });
    Show(out);
    std::unique_ptr<DateFormat> time(DateFormat::createInstanceForSkeleton("jm", en, e));
    time->setTimeZone(*utc);
    Time("time-format", 2000000,
         [&]
         {
             out.remove();
             time->format(local, out);
             sink = out.length();
         });
    Show(out);
    Time("datetime-init", 100000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             std::unique_ptr<DateFormat> f(DateFormat::createInstanceForSkeleton("yMMMdjm", en, s));
             sink = s;
         });
    std::unique_ptr<DateFormat> both(DateFormat::createInstanceForSkeleton("yMMMdjm", en, e));
    both->setTimeZone(*utc);
    Time("datetime-format", 2000000,
         [&]
         {
             out.remove();
             both->format(local, out);
             sink = out.length();
         });
    Show(out);
    std::unique_ptr<DateFormat> zoned(DateFormat::createInstanceForSkeleton("yMMMdjmz", en, e));
    zoned->adoptTimeZone(TimeZone::createTimeZone("America/New_York"));
    Time("zoned-format", 1000000,
         [&]
         {
             out.remove();
             zoned->format(instant, out);
             sink = out.length();
         });
    Show(out);
    std::unique_ptr<DateIntervalFormat> range(DateIntervalFormat::createInstance("yMMMd", en, e));
    range->setTimeZone(*utc);
    DateInterval interval(rangeStart, rangeEnd);
    Time("date-range", 1000000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             out.remove();
             FieldPosition pos(FieldPosition::DONT_CARE);
             range->format(&interval, out, pos, s);
             sink = out.length();
         });
    Show(out);
    Time("relative-init", 200000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             RelativeDateTimeFormatter f(en, s);
             sink = s;
         });
    RelativeDateTimeFormatter relative(en, e);
    Time("relative-format", 2000000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             out.remove();
             relative.formatNumeric(3.0, UDAT_REL_UNIT_DAY, out, s);
             sink = out.length();
         });
    Show(out);
    printf("status %s\n", u_errorName(e));
    delete utc;
    return 0;
}
