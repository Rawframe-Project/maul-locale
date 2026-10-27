// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The parsers of Temporal's strings through the public functions: each
// goal and the precedence between them, basic and extended forms, the
// early errors, the annotation rules, offsets, time zone and calendar
// strings, durations with exact fractions and their limits, and every
// refusal. tools/check_ixdtf.py compares the parsers with a second reading
// of the grammar over many more strings.

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
    mlocTemporalParse p;
    const char* lower = "2020-01-01T00:00[America/Mazatlan][u-ca=zz-z9]";
    CHECK(mlocParseTimeZoneString("America/Mazatlan", 16, &z) == mloc_success &&
              z.name.length == 16 && Parse(lower, mloc_formatZonedDateTime, &p) &&
              SpanIs(lower, p.timeZone, "America/Mazatlan") && SpanIs(lower, p.calendar, "zz-z9"),
          "every lower-case letter in names and annotation values");
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
    CHECK(mlocParseDuration("P1\0", 3, &v) == mloc_errorSyntax &&
              mlocParseDuration("PT1\0", 4, &v) == mloc_errorSyntax,
          "a zero byte is no designator");
    CHECK(mlocParseDuration("P0000000000000000000000000000001Y", 33, &v) == mloc_success &&
              v.years == 1 && mlocParseDuration(nullptr, 0, &v) == mloc_errorInvalid &&
              mlocParseDuration("P1D", 3, nullptr) == mloc_errorInvalid,
          "leading zeroes, nulls");
}

static char s_text[256];

// The string a writer gave, or "refused".
static const char* Text(mlocResult r, const mlocOutput* out)
{
    if (r != mloc_success)
    {
        return "refused";
    }
    s_text[out->length] = '\0';
    return s_text;
}

static mlocOutput Out(void)
{
    return (mlocOutput){.buffer = s_text, .capacity = sizeof s_text - 1};
}

static const char* DateText(int32_t day, mlocTemporalStringOptions* o, int kind)
{
    mlocDate date;
    mlocOutput out = Out();
    if (mlocMakeDateFromDay(mloc_calendarGregorian, day, &date) != mloc_success)
    {
        return "no date";
    }
    mlocResult r = kind == 0   ? mlocFormatTemporalDate(&date, o, &out)
                   : kind == 1 ? mlocFormatYearMonth(&date, o, &out)
                               : mlocFormatMonthDay(&date, o, &out);
    return Text(r, &out);
}

static void TestDateWriters(void)
{
    // Day 1 is January 1 of year 1, and day 719163 the first of 1970.
    mlocTemporalStringOptions hebrew = {.calendar = "hebrew", .calendarLength = 6};
    CHECK(strcmp(DateText(719163, nullptr, 0), "1970-01-01") == 0 &&
              strcmp(DateText(1, nullptr, 0), "0001-01-01") == 0 &&
              strcmp(DateText(mlocDateMinDay, nullptr, 0), "-271821-04-19") == 0 &&
              strcmp(DateText(mlocDateMaxDay, nullptr, 0), "+275760-09-13") == 0 &&
              strcmp(DateText(0, nullptr, 0), "0000-12-31") == 0 &&
              strcmp(DateText(719163, &hebrew, 0), "1970-01-01[u-ca=hebrew]") == 0,
          "dates: four digits, or six with a sign; another calendar shown");
    mlocTemporalStringOptions show = {.showCalendar = mloc_showCalendarAlways};
    CHECK(strcmp(DateText(719163, &show, 0), "1970-01-01[u-ca=iso8601]") == 0, "always");
    show.showCalendar = mloc_showCalendarCritical;
    CHECK(strcmp(DateText(719163, &show, 0), "1970-01-01[!u-ca=iso8601]") == 0, "critical");
    hebrew.showCalendar = mloc_showCalendarNever;
    CHECK(strcmp(DateText(719163, &hebrew, 0), "1970-01-01") == 0, "never");
    hebrew.showCalendar = mloc_showCalendarAuto;
    CHECK(strcmp(DateText(719163, nullptr, 1), "1970-01") == 0 &&
              strcmp(DateText(719163, &hebrew, 1), "1970-01-01[u-ca=hebrew]") == 0 &&
              strcmp(DateText(719163, &show, 1), "1970-01-01[!u-ca=iso8601]") == 0 &&
              strcmp(DateText(719163, nullptr, 2), "01-01") == 0 &&
              strcmp(DateText(719163, &hebrew, 2), "1970-01-01[u-ca=hebrew]") == 0,
          "a year-month and a month-day, their reference parts when shown");
    mlocTemporalStringOptions bad[] = {
        {.calendar = "", .calendarLength = 0},
        {.calendar = "a]b", .calendarLength = 3},
        {.calendar = "toolongname", .calendarLength = 11},
        {.calendar = "abcdefghi", .calendarLength = 9},
        {.calendar = "a--b", .calendarLength = 4},
        {.calendar = "ab-", .calendarLength = 3},
        {.calendar = nullptr, .calendarLength = 3},
        {.secondsPrecision = mloc_secondsDigits, .fractionDigits = 10},
        {.secondsPrecision = 3},
        {.showCalendar = 4},
        {.showTimeZone = 3},
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        CHECK(strcmp(DateText(719163, &bad[i], 0), "refused") == 0,
              "a calendar id outside the grammar, options out of range");
    }
    mlocTemporalStringOptions dashed = {.calendar = "islamic-umalqura", .calendarLength = 16};
    mlocTemporalStringOptions digits = {.calendar = "abcdefg9-0z", .calendarLength = 11};
    CHECK(strcmp(DateText(719163, &dashed, 0), "1970-01-01[u-ca=islamic-umalqura]") == 0 &&
              strcmp(DateText(719163, &digits, 0), "1970-01-01[u-ca=abcdefg9-0z]") == 0,
          "calendar ids with a hyphen, digits and components of eight");
    mlocDate forged = {.day = mlocDateMaxDay + 1};
    mlocOutput out = Out();
    CHECK(mlocFormatTemporalDate(&forged, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatTemporalDate(nullptr, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatYearMonth(&forged, nullptr, &out) == mloc_errorInvalid,
          "a date the library would not make, null");
    mlocDate date;
    (void)mlocMakeDateFromDay(mloc_calendarGregorian, 719163, &date);
    mlocOutput measure = {0};
    mlocOutput shortOut = {.buffer = s_text, .capacity = 4};
    mlocOutput forgedOut = {.buffer = nullptr, .capacity = 4};
    CHECK(mlocFormatTemporalDate(&date, nullptr, &measure) == mloc_errorCapacity &&
              measure.length == 10 &&
              mlocFormatTemporalDate(&date, nullptr, &shortOut) == mloc_errorCapacity &&
              shortOut.length == 10 &&
              mlocFormatTemporalDate(&date, nullptr, &forgedOut) == mloc_errorInvalid &&
              mlocFormatTemporalDate(&date, nullptr, nullptr) == mloc_errorInvalid,
          "measured, too short, a null buffer with a capacity, no output");
    char exact[10];
    mlocOutput exactOut = {.buffer = exact, .capacity = sizeof exact};
    CHECK(mlocFormatTemporalDate(&date, nullptr, &exactOut) == mloc_success &&
              exactOut.length == 10 && memcmp(exact, "1970-01-01", 10) == 0,
          "a buffer the string fills exactly");
}

enum
{
    // A precision for the helpers: 0 to 9 digits, or these.
    autoDigits = -1,
    minute = -2,
};

static mlocTemporalStringOptions WithPrecision(int precision)
{
    return precision == autoDigits ? (mlocTemporalStringOptions){0}
           : precision == minute
               ? (mlocTemporalStringOptions){.secondsPrecision = mloc_secondsMinute}
               : (mlocTemporalStringOptions){.secondsPrecision = mloc_secondsDigits,
                                             .fractionDigits = (uint8_t)precision};
}

static const char* TimeText(uint8_t h, uint8_t m, uint8_t s, uint32_t ns, int precision)
{
    mlocTime t = {h, m, s, ns};
    mlocTemporalStringOptions o = WithPrecision(precision);
    mlocOutput out = Out();
    return Text(mlocFormatTemporalTime(&t, &o, &out), &out);
}

static void TestTimeWriters(void)
{
    CHECK(strcmp(TimeText(1, 2, 3, 0, autoDigits), "01:02:03") == 0 &&
              strcmp(TimeText(1, 2, 3, 400000000, autoDigits), "01:02:03.4") == 0 &&
              strcmp(TimeText(1, 2, 3, 1, autoDigits), "01:02:03.000000001") == 0 &&
              strcmp(TimeText(23, 59, 59, 999999999, minute), "23:59") == 0 &&
              strcmp(TimeText(1, 2, 3, 987654321, 0), "01:02:03") == 0 &&
              strcmp(TimeText(1, 2, 3, 987654321, 3), "01:02:03.987") == 0 &&
              strcmp(TimeText(1, 2, 3, 0, 9), "01:02:03.000000000") == 0 &&
              strcmp(TimeText(1, 2, 3, 120000000, 1), "01:02:03.1") == 0,
          "times to each precision, truncated");
    CHECK(strcmp(TimeText(24, 0, 0, 0, autoDigits), "refused") == 0 &&
              strcmp(TimeText(0, 0, 0, 1000000000, autoDigits), "refused") == 0,
          "a time the library would not make");
    mlocDateTime dt;
    (void)mlocMakeDateFromDay(mloc_calendarGregorian, 719163, &dt.date);
    dt.time = (mlocTime){12, 34, 56, 5000};
    mlocTemporalStringOptions o = {.calendar = "japanese",
                                   .calendarLength = 8,
                                   .secondsPrecision = mloc_secondsDigits,
                                   .fractionDigits = 6};
    mlocOutput out = Out();
    CHECK(strcmp(Text(mlocFormatTemporalDateTime(&dt, &o, &out), &out),
                 "1970-01-01T12:34:56.000005[u-ca=japanese]") == 0,
          "a date-time");
    dt.time.second = 60;
    CHECK(mlocFormatTemporalDateTime(&dt, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatTemporalTime(nullptr, nullptr, &out) == mloc_errorInvalid,
          "a forged date-time, null");
}

static void TestInstantWriters(void)
{
    mlocData data;
    mlocOpenBuiltinData(&data);
    mlocDataSet set = {&data, 1};
    mlocTimeZone amsterdam;
    mlocTimeZone monrovia;
    mlocTimeZone plus;
    CHECK(mlocFindTimeZone(&set, "Europe/Amsterdam", 16, &amsterdam) == mloc_success &&
              mlocFindTimeZone(&set, "Africa/Monrovia", 15, &monrovia) == mloc_success &&
              mlocFindTimeZone(&set, "+05:30", 6, &plus) == mloc_success,
          "zones");
    mlocInstant epoch = {0, 0};
    // Monrovia kept -00:44:30 until 1972; half a minute rounds away from
    // zero.
    mlocInstant fraction = {0, 500000000};
    mlocInstant last = {INT64_C(8640000000000), 0};
    mlocOutput out = Out();
    CHECK(strcmp(Text(mlocFormatInstant(&epoch, nullptr, nullptr, &out), &out),
                 "1970-01-01T00:00:00Z") == 0,
          "an instant in universal time");
    CHECK(strcmp(Text(mlocFormatInstant(&fraction, &monrovia, nullptr, &out), &out),
                 "1969-12-31T23:15:30.5-00:45") == 0,
          "a zone's local time, the offset rounded to the minute");
    CHECK(strcmp(Text(mlocFormatInstant(&last, nullptr, nullptr, &out), &out),
                 "+275760-09-13T00:00:00Z") == 0,
          "the last instant");
    mlocTemporalStringOptions o = WithPrecision(minute);
    CHECK(strcmp(Text(mlocFormatZonedDateTime(&plus, &epoch, &o, &out), &out),
                 "1970-01-01T05:30+05:30[+05:30]") == 0,
          "an offset zone");
    o = (mlocTemporalStringOptions){.calendar = "gregory",
                                    .calendarLength = 7,
                                    .showCalendar = mloc_showCalendarCritical,
                                    .showTimeZone = mloc_showTimeZoneCritical,
                                    .hideOffset = true};
    CHECK(strcmp(Text(mlocFormatZonedDateTime(&amsterdam, &epoch, &o, &out), &out),
                 "1970-01-01T01:00:00[!Europe/Amsterdam][!u-ca=gregory]") == 0,
          "critical annotations, the offset hidden");
    o.showTimeZone = mloc_showTimeZoneNever;
    o.showCalendar = mloc_showCalendarNever;
    o.hideOffset = false;
    CHECK(strcmp(Text(mlocFormatZonedDateTime(&amsterdam, &epoch, &o, &out), &out),
                 "1970-01-01T01:00:00+01:00") == 0,
          "no annotations");
    mlocInstant past = {INT64_C(8640000000000), 1};
    mlocTimeZone forged = {.offsetMinutes = 1440};
    CHECK(mlocFormatInstant(&past, nullptr, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatInstant(&epoch, &forged, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatZonedDateTime(nullptr, &epoch, nullptr, &out) == mloc_errorInvalid &&
              mlocFormatZonedDateTime(&plus, nullptr, nullptr, &out) == mloc_errorInvalid,
          "an instant out of range, a forged zone, nulls");
}

static const char* DurationText(mlocDurationValues v, int precision)
{
    mlocTemporalStringOptions o = WithPrecision(precision);
    mlocOutput out = Out();
    return Text(mlocFormatTemporalDuration(&v, &o, &out), &out);
}

static void TestDurationWriters(void)
{
    mlocDurationValues zero = {0};
    mlocDurationValues all = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    mlocDurationValues negative = {.days = -1, .milliseconds = -500};
    mlocDurationValues minutes = {.minutes = 90};
    mlocDurationValues weeks = {.weeks = 2};
    // 2^53 - 1 seconds' worth of nanoseconds and microseconds, past 2^64.
    mlocDurationValues big = {.microseconds = 9007199254740991.0, .nanoseconds = 1e24};
    CHECK(strcmp(DurationText(zero, autoDigits), "PT0S") == 0 &&
              strcmp(DurationText(zero, 2), "PT0.00S") == 0 &&
              strcmp(DurationText(all, autoDigits), "P1Y2M3W4DT5H6M7.00800901S") == 0 &&
              strcmp(DurationText(negative, autoDigits), "-P1DT0.5S") == 0 &&
              strcmp(DurationText(minutes, autoDigits), "PT90M") == 0 &&
              strcmp(DurationText(minutes, 0), "PT90M0S") == 0 &&
              strcmp(DurationText(weeks, autoDigits), "P2W") == 0 &&
              strcmp(DurationText(all, 1), "P1Y2M3W4DT5H6M7.0S") == 0,
          "durations: designators, signs, seconds when needed or asked");
    CHECK(strcmp(DurationText(big, autoDigits), "PT1000009007199254.724213784S") == 0,
          "sub-second fields past 2^64, summed exactly");
    mlocDurationValues mixed = {.days = 1, .hours = -1};
    CHECK(strcmp(DurationText(mixed, autoDigits), "refused") == 0 &&
              strcmp(DurationText(all, minute), "refused") == 0,
          "mixed signs, minutes as a precision");
    mlocOutput out = Out();
    CHECK(strcmp(Text(mlocFormatUTCOffset(0, &out), &out), "+00:00") == 0 &&
              strcmp(Text(mlocFormatUTCOffset(-INT64_C(19800000000000), &out), &out), "-05:30") ==
                  0 &&
              strcmp(Text(mlocFormatUTCOffset(INT64_C(1172000000000), &out), &out), "+00:19:32") ==
                  0 &&
              strcmp(Text(mlocFormatUTCOffset(-INT64_C(86399999999999), &out), &out),
                     "-23:59:59.999999999") == 0 &&
              mlocFormatUTCOffset(INT64_C(86400000000000), &out) == mloc_errorInvalid &&
              mlocFormatUTCOffset(-INT64_C(86400000000000), &out) == mloc_errorInvalid &&
              mlocFormatUTCOffset(0, nullptr) == mloc_errorInvalid,
          "offsets");
}

// Every string written parses back to what it was written from.
static void TestRoundTrips(void)
{
    char text[128];
    for (int32_t day = mlocDateMinDay; day <= mlocDateMaxDay; day += 7919)
    {
        mlocDateTime dt;
        (void)mlocMakeDateFromDay(mloc_calendarGregorian, day, &dt.date);
        dt.time = (mlocTime){(uint8_t)(day & 15), 7, 59, (uint32_t)day & 0x3FFFFFFF};
        dt.time.nanosecond %= 1000000000;
        mlocOutput out = {.buffer = text, .capacity = sizeof text};
        mlocTemporalParse p;
        mlocDate back;
        bool ok =
            mlocFormatTemporalDateTime(&dt, nullptr, &out) == mloc_success &&
            mlocParseTemporalString(text, out.length, mloc_formatDateTime, &p) == mloc_success &&
            mlocMakeDateFromDay(mloc_calendarGregorian, dt.date.day, &back) == mloc_success;
        mlocDateFields f;
        ok = ok && mlocGetDateFields(&back, &f) == mloc_success && p.year == f.year &&
             p.month == f.month && p.day == f.day && p.time.hour == dt.time.hour &&
             p.time.nanosecond == dt.time.nanosecond;
        if (!ok)
        {
            CHECK(false, "a date-time read back");
            return;
        }
    }
    for (int64_t s = -8640000000000; s <= 8640000000000; s += 99999999937)
    {
        mlocDurationValues v = {.seconds = (double)(s < 0 ? -s : s), .nanoseconds = 123};
        mlocDurationValues back;
        mlocOutput out = {.buffer = text, .capacity = sizeof text};
        if (mlocFormatTemporalDuration(&v, nullptr, &out) != mloc_success ||
            mlocParseDuration(text, out.length, &back) != mloc_success ||
            back.seconds != v.seconds || back.nanoseconds != 123)
        {
            CHECK(false, "a duration read back");
            return;
        }
    }
    CHECK(true, "strings read back");
}

int main(void)
{
    TestDateTimes();
    TestTimesAndPartialDates();
    TestOffsetsZonesCalendars();
    TestDurations();
    TestDateWriters();
    TestTimeWriters();
    TestInstantWriters();
    TestDurationWriters();
    TestRoundTrips();
    return s_failures == 0 ? 0 : 1;
}
