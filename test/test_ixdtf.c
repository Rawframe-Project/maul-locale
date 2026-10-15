// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The parsers of Temporal's strings through the public functions (record
// mloc-0041): each goal and the precedence between them, basic and
// extended forms, the early errors, the annotation rules, offsets, time
// zone and calendar strings, durations with exact fractions and their
// limits, and every refusal. tools/check_ixdtf.py compares the parsers
// with a second reading of the grammar over many more strings.

#include "test_harness.h"

#include "maul-locale/ixdtf.h"

#include <string.h>

static bool Parse(const char* text, uint32_t formats, mlocTemporalParse* out)
{
    return mlocParseTemporalString(text, strlen(text), formats, out) == mloc_success;
}

static bool SpanIs(const char* text, mlocTextSpan span, const char* want)
{
    return span.length == strlen(want) && memcmp(text + span.start, want, span.length) == 0;
}

static void TestDateTimes(void)
{
    const char* full = "1976-11-18T15:23:30.123456789+01:00[Europe/Vienna][u-ca=hebrew]";
    mlocTemporalParse p;
    CHECK(Parse(full, 63, &p) && p.format == mloc_formatZonedDateTime && p.year == 1976 &&
              p.month == 11 && p.day == 18 && p.hasTime && p.time.hour == 15 &&
              p.time.minute == 23 && p.time.second == 30 && p.time.nanosecond == 123456789 &&
              p.hasOffset && p.offsetNanoseconds == INT64_C(3600000000000) && !p.offsetHasSeconds &&
              SpanIs(full, p.timeZone, "Europe/Vienna") && SpanIs(full, p.calendar, "hebrew"),
          "every part");
    CHECK(Parse("19761118T152330,5-0800", mloc_formatDateTime, &p) &&
              p.time.nanosecond == 500000000 && p.offsetNanoseconds == -INT64_C(28800000000000),
          "basic forms and a comma");
    CHECK(Parse("2016-12-31t23:59:60z", mloc_formatInstant, &p) && p.utc && p.time.second == 59,
          "lower case designators, a leap second as 59");
    CHECK(!Parse("1970-01-01T00:00Z", mloc_formatDateTime, &p) &&
              Parse("1970-01-01T00:00Z[UTC]", mloc_formatZonedDateTime, &p) &&
              !Parse("1970-01-01T00:00", mloc_formatZonedDateTime, &p) &&
              Parse("1970-01-01[UTC]", mloc_formatZonedDateTime, &p) && !p.hasTime,
          "Z only where a zone or instant is read; a zoned string needs its annotation");
    CHECK(Parse("1970-01-01 00:00:00+00:00:00.5", mloc_formatInstant, &p) && p.offsetHasSeconds &&
              p.offsetNanoseconds == 500000000,
          "a space, and an offset with seconds and a fraction");
    CHECK(Parse("-271821-04-20", mloc_formatDateTime, &p) && p.year == -271821 &&
              Parse("+275760-09-13", mloc_formatDateTime, &p) && p.year == 275760,
          "six-digit years");
    const char* refused[] = {
        "2020-02-30",
        "2021-02-29",
        "2020-04-31",
        "-000000-01-01",
        "1970-0101",
        "1970-01-01T0123:45",
        "1970-01-01T24:00",
        "1970-01-01T12:34:56.1234567890",
        "1970-01-01T12:60",
        "2020-01-01T12:00\342\210\22202:00",
        "20200-01-01",
        "1970-01-01[u-ca=a][!u-ca=b]",
        "1970-01-01[!u-ca=a][u-ca=b]",
        "1970-01-01[!foo=bar]",
        "1970-01-01[U-CA=hebrew]",
        "1970-01-01[UTC][UTC]",
        "",
        "T",
    };
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++)
    {
        CHECK(!Parse(refused[i], 63, &p), "an early error, a malformed string or annotation");
    }
    CHECK(Parse("1970-01-01[u-ca=a][u-ca=b][foo=bar]", 63, &p) &&
              SpanIs("1970-01-01[u-ca=a][u-ca=b][foo=bar]", p.calendar, "a"),
          "the first calendar, others ignored when none is critical");
    CHECK(!Parse("1970-01-01", 0, &p) &&
              mlocParseTemporalString(nullptr, 0, 63, &p) == mloc_errorInvalid &&
              mlocParseTemporalString("x", 1, 63, nullptr) == mloc_errorInvalid,
          "no goal, null pointers");
    // The length is the caller's: a zero byte inside is just a byte.
    CHECK(mlocParseTemporalString("2020-01-01\0junk", 10, 63, &p) == mloc_success,
          "the length given, not a terminator");
}

static void TestTimesAndPartialDates(void)
{
    mlocTemporalParse p;
    CHECK(Parse("T1214", mloc_formatTime, &p) && p.format == mloc_formatTime && p.time.hour == 12 &&
              p.time.minute == 14,
          "T and a basic time");
    CHECK(!Parse("1214", mloc_formatTime, &p) && !Parse("12-14", mloc_formatTime, &p) &&
              !Parse("2021-12", mloc_formatTime, &p) && Parse("12:14", mloc_formatTime, &p),
          "a time that reads as a month-day or year-month needs its T");
    CHECK(Parse("1214", 63, &p) && p.format == mloc_formatMonthDay && !p.hasYear && p.month == 12 &&
              p.day == 14,
          "so all goals read it as a month-day");
    CHECK(Parse("--02-29", mloc_formatMonthDay, &p) && p.month == 2 && p.day == 29 &&
              !Parse("02-30", mloc_formatMonthDay, &p),
          "a month-day, February 29 allowed without a year");
    CHECK(Parse("2021-12", mloc_formatYearMonth, &p) && p.year == 2021 && p.day == 1 &&
              Parse("202112", mloc_formatYearMonth, &p) &&
              !Parse("2021-12[u-ca=hebrew]", mloc_formatYearMonth, &p) &&
              Parse("2021-12[u-ca=ISO8601]", mloc_formatYearMonth, &p) &&
              Parse("2021-12-03[u-ca=hebrew]", mloc_formatYearMonth, &p) && p.day == 3,
          "a year-month: another calendar only with a full date");
    CHECK(Parse("1970-01-01T00:00", mloc_formatTime, &p) && p.time.hour == 0 &&
              !Parse("1970-01-01", mloc_formatTime, &p),
          "a date-time as a time, its time required");
}

static void TestOffsetsZonesCalendars(void)
{
    int64_t ns = 0;
    bool seconds = false;
    CHECK(mlocParseUTCOffset("+01", 3, &ns, &seconds) == mloc_success &&
              ns == INT64_C(3600000000000) && !seconds &&
              mlocParseUTCOffset("-0530", 5, &ns, nullptr) == mloc_success &&
              ns == -INT64_C(19800000000000) &&
              mlocParseUTCOffset("+00:00:01.000000001", 19, &ns, &seconds) == mloc_success &&
              ns == 1000000001 && seconds,
          "offsets");
    CHECK(mlocParseUTCOffset("+01:0000", 8, &ns, nullptr) == mloc_errorSyntax &&
              mlocParseUTCOffset("+24:00", 6, &ns, nullptr) == mloc_errorSyntax &&
              mlocParseUTCOffset("01:00", 5, &ns, nullptr) == mloc_errorSyntax &&
              mlocParseUTCOffset(nullptr, 0, &ns, nullptr) == mloc_errorInvalid &&
              mlocParseUTCOffset("+01", 3, nullptr, nullptr) == mloc_errorInvalid,
          "mixed forms, hour 24, no sign, nulls");
    mlocTimeZoneParse z;
    const char* annotated = "2020-01-01T00:00+05:00[America/New_York]";
    CHECK(mlocParseTimeZoneString("Europe/Vienna", 13, &z) == mloc_success &&
              SpanIs("Europe/Vienna", z.name, "Europe/Vienna") && !z.hasOffset &&
              mlocParseTimeZoneString("-08:00", 6, &z) == mloc_success && z.hasOffset &&
              z.offsetMinutes == -480 &&
              mlocParseTimeZoneString(annotated, strlen(annotated), &z) == mloc_success &&
              SpanIs(annotated, z.name, "America/New_York") &&
              mlocParseTimeZoneString("2020-01-01T00:00Z", 17, &z) == mloc_success && z.utc &&
              mlocParseTimeZoneString("2020-01-01T00:00+01:30", 22, &z) == mloc_success &&
              z.offsetMinutes == 90,
          "a name, an offset, an annotation, Z, a date-time's offset");
    CHECK(mlocParseTimeZoneString("2020-01-01T00:00+01:00:01", 25, &z) == mloc_errorSyntax &&
              mlocParseTimeZoneString("2020-01-01", 10, &z) == mloc_errorSyntax &&
              mlocParseTimeZoneString("+01:00:00", 9, &z) == mloc_errorSyntax &&
              mlocParseTimeZoneString("", 0, &z) == mloc_errorSyntax &&
              mlocParseTimeZoneString(nullptr, 0, &z) == mloc_errorInvalid,
          "an offset with seconds, no zone, nothing, null");
    mlocTextSpan c;
    CHECK(mlocParseCalendarString("hebrew", 6, &c) == mloc_success && c.length == 6 &&
              mlocParseCalendarString("2020-01-01[u-ca=japanese]", 25, &c) == mloc_success &&
              c.start == 16 && c.length == 8 &&
              mlocParseCalendarString("2020-01-01", 10, &c) == mloc_success && c.length == 0 &&
              mlocParseCalendarString("not a calendar", 14, &c) == mloc_errorSyntax &&
              mlocParseCalendarString("", 0, &c) == mloc_errorSyntax &&
              mlocParseCalendarString("x", 1, nullptr) == mloc_errorInvalid,
          "calendar strings");
}

static bool DurationIs(const char* text, double h, double mi, double s, double ms, double us,
                       double ns)
{
    mlocDurationValues v;
    return mlocParseDuration(text, strlen(text), &v) == mloc_success && v.hours == h &&
           v.minutes == mi && v.seconds == s && v.milliseconds == ms && v.microseconds == us &&
           v.nanoseconds == ns;
}

static void TestDurations(void)
{
    mlocDurationValues v;
    CHECK(mlocParseDuration("-P1Y2M3W4DT5H", 13, &v) == mloc_success && v.years == -1 &&
              v.months == -2 && v.weeks == -3 && v.days == -4 && v.hours == -5,
          "every date unit, negative");
    CHECK(DurationIs("PT46H66M71.50040904S", 46, 66, 71, 500, 409, 40) &&
              DurationIs("PT1.000000001H", 1, 0, 0, 0, 3, 600) &&
              DurationIs("pt0,5m", 0, 0, 30, 0, 0, 0) && DurationIs("PT1H30M", 1, 30, 0, 0, 0, 0),
          "fractions carried down exactly");
    const char* refused[] = {"P",
                             "PT",
                             "P1M1Y",
                             "PT1.5H30M",
                             "PT1.5M1S",
                             "P1.5D",
                             "PT1.1234567891S",
                             "P4294967296Y",
                             "PT9007199254740992S",
                             "1D",
                             "P1D T1H",
                             "+-P1D"};
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++)
    {
        mlocResult r = mlocParseDuration(refused[i], strlen(refused[i]), &v);
        CHECK(r == (i == 7 || i == 8 ? mloc_errorInvalid : mloc_errorSyntax),
              "a malformed duration, or one past the limits");
    }
    CHECK(mlocParseDuration("P0000000000000000000000000000001Y", 33, &v) == mloc_success &&
              v.years == 1 && mlocParseDuration(nullptr, 0, &v) == mloc_errorInvalid &&
              mlocParseDuration("P1D", 3, nullptr) == mloc_errorInvalid,
          "leading zeroes, nulls");
}

int main(void)
{
    TestDateTimes();
    TestTimesAndPartialDates();
    TestOffsetsZonesCalendars();
    TestDurations();
    return s_failures == 0 ? 0 : 1;
}
