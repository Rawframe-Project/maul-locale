// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date ranges (mlocFormatDateRange): the greatest difference's pair, the
// fallback, one date, the order and the parts of each date, checked
// against ICU4C 79's DateIntervalFormat made from each option's skeleton;
// where the library differs on purpose (a leap month, fa's month items, a
// calendar ICU lacks), its own answer. Then ICU's answers kept in
// test/date_ranges_icu.tsv, test/time_ranges_icu.tsv and
// test/datetime_ranges_icu.tsv, with the library's own where it differs
// and why.

// fopen is the portable way to read the data file; MSVC's CRT deprecates
// it.
#define _CRT_SECURE_NO_WARNINGS

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// The range's dash between thin spaces, as en, de and kek write it.
#define DASH "\xE2\x80\x89\xE2\x80\x93\xE2\x80\x89"

static mlocData s_sources[4];
static mlocDataSet s_data;

static bool OpenFile(const char* path, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        return false;
    }
    // Each file's own size, which the web build's memory holds.
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    uint8_t* bytes = size > 0 && fseek(file, 0, SEEK_SET) == 0 ? malloc((size_t)size) : nullptr;
    size_t read = bytes == nullptr ? 0 : fread(bytes, 1, (size_t)size, file);
    fclose(file);
    if (read > 0 && mlocOpenData(out, bytes, read) == mloc_success)
    {
        return true; // the bytes stay for the run
    }
    free(bytes);
    return false;
}

static mlocDate Iso(int32_t year, uint32_t month, uint32_t day)
{
    mlocDate date = {0};
    mlocDateInput input = {.year = year, .month = month, .day = day};
    if (mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &date) != mloc_success)
    {
        date.day = 0;
    }
    return date;
}

// The parts written, each as its kind's letter (L literal, G era, y year,
// r related year, U year name, M month, E weekday, d day, h hour, m
// minute, s second, f fraction, a day period, z zone), its value ('='
// both, '<' the start, '>' the end) and its text in brackets.
typedef struct Parts
{
    const char* text;
    char out[512];
    size_t length;
} Parts;

static void Part(void* context, uint32_t part, size_t start, size_t length)
{
    static const char s_kinds[] = "LGyrUMEdhmsfaz";
    Parts* p = context;
    uint32_t kind = part & 0xFF;
    char side = (part & 0x300) == mloc_dateRangeStart ? '<'
                : (part & 0x300) == mloc_dateRangeEnd ? '>'
                                                      : '=';
    int n = snprintf(p->out + p->length, sizeof(p->out) - p->length, "%c%c[%.*s]",
                     kind < sizeof(s_kinds) - 1 ? s_kinds[kind] : '?', side, (int)length,
                     p->text + start);
    p->length += n > 0 ? (size_t)n : 0;
}

static bool Formatter(const char* id, mlocDateOptions options, mlocDateFormatter* formatter)
{
    mlocLocale locale;
    return mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status ==
               mloc_success &&
           mlocInitDateFormatter(formatter, &s_data, &locale, &options) >= 0;
}

// Whether a locale writes the range as want, and its parts as parts
// when given.
static bool Is(const char* id, mlocDateOptions options, mlocDate start, mlocDate end,
               const char* want, const char* parts)
{
    mlocDateFormatter formatter;
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    if (!Formatter(id, options, &formatter) ||
        mlocFormatDateRange(&formatter, &start, &end, &output) != mloc_success)
    {
        printf("  %s: not formatted\n", id);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s: got \"%.*s\" %s, want \"%s\" %s\n", id, (int)output.length, text, p.out, want,
               parts != nullptr ? parts : "");
    }
    return same;
}

static mlocDateOptions Set(mlocDateFieldSet fields, mlocDateLength length)
{
    return (mlocDateOptions){.fields = fields, .length = length};
}

static void TestDifferences(void)
{
    mlocDateOptions medium = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    CHECK(Is("en", medium, Iso(2026, 1, 5), Iso(2026, 1, 8), "Jan 5" DASH "8, 2026",
             "M=[Jan]L=[ ]d<[5]L=[" DASH "]d>[8]L=[, ]y=[2026]"),
          "a day's pair: the month and year once, each day its date's");
    CHECK(Is("en", medium, Iso(2026, 1, 5), Iso(2026, 2, 8), "Jan 5" DASH "Feb 8, 2026",
             "M<[Jan]L<[ ]d<[5]L=[" DASH "]M>[Feb]L>[ ]d>[8]L=[, ]y=[2026]"),
          "a month's pair, the text between a date's fields its own");
    CHECK(Is("en", medium, Iso(2025, 1, 5), Iso(2026, 2, 8), "Jan 5, 2025" DASH "Feb 8, 2026",
             nullptr),
          "a year's pair");
    CHECK(
        Is("en", medium, Iso(1, 1, 1), Iso(0, 12, 29), "Jan 1, 1 AD" DASH "Dec 29, 1 BC", nullptr),
        "an era's, both in the era style the earlier year needs");
    CHECK(Is("ja-u-ca-japanese", medium, Iso(2019, 4, 28), Iso(2019, 5, 3),
             "\xE5\xB9\xB3\xE6\x88\x90"
             "31\xE5\xB9\xB4"
             "4\xE6\x9C\x88"
             "28\xE6\x97\xA5"
             "\xEF\xBD\x9E\xE4\xBB\xA4\xE5\x92\x8C\xE5\x85\x83\xE5\xB9\xB4"
             "5\xE6\x9C\x88"
             "3\xE6\x97\xA5",
             nullptr),
          "a Japanese era's");
    CHECK(Is("en", medium, Iso(2026, 1, 8), Iso(2026, 1, 5), "Jan 8" DASH "5, 2026", nullptr),
          "a start past its end, as given");
    CHECK(Is("de", medium, Iso(2026, 1, 5), Iso(2026, 2, 8), "05.01." DASH "08.02.2026", nullptr),
          "de's numeric month");
    CHECK(Is("en", (mlocDateOptions){.style = mloc_dateStyleFull}, Iso(2026, 1, 5), Iso(2026, 1, 8),
             "Monday, January 5" DASH "Thursday, January 8, 2026", nullptr),
          "a standard format's skeleton");
}

static void TestOneDateAndFallback(void)
{
    mlocDateOptions medium = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    CHECK(Is("en", medium, Iso(2026, 1, 5), Iso(2026, 1, 5), "Jan 5, 2026",
             "M=[Jan]L=[ ]d=[5]L=[, ]y=[2026]"),
          "equal dates: one, every part both's");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonth, mloc_dateLengthMedium), Iso(2026, 1, 5),
             Iso(2026, 1, 8), "Jan 2026", nullptr),
          "a day's difference a month does not show: one date");
    CHECK(Is("en", Set(mloc_dateFieldSetYear, mloc_dateLengthMedium), Iso(2026, 1, 5),
             Iso(2026, 3, 8), "2026", nullptr),
          "a month's difference a year does not show");
    CHECK(Is("en", Set(mloc_dateFieldSetWeekday, mloc_dateLengthMedium), Iso(2026, 1, 5),
             Iso(2026, 1, 12), "Mon" DASH "Mon", "E<[Mon]L=[" DASH "]E>[Mon]"),
          "a weekday a week apart: its pair, as ICU writes it");
    CHECK(Is("en", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthShort), Iso(2026, 1, 5),
             Iso(2026, 1, 8), "1/5/26" DASH "1/8/26",
             "M<[1]L<[/]d<[5]L<[/]y<[26]L=[" DASH "]M>[1]L>[/]d>[8]L>[/]y>[26]"),
          "the short date's pair, each whole");
    CHECK(Is("kek", medium, Iso(2026, 1, 5), Iso(2027, 2, 8),
             "8 Xkab\xCA\xBC Po, 2027" DASH "5 Xb\xCA\xBC"
             "een Po, 2026",
             nullptr),
          "kek's fallback, \"{1} \xE2\x80\x93 {0}\": the end first");
}

// Whether a locale writes the range of two times as want, and its parts
// (letters as Part's: h hour, m minute, s second, f fraction, a day
// period, z zone) as parts when given.
static bool TimeIs(const char* id, mlocTimeOptions options, mlocTime start, mlocTime end,
                   const char* want, const char* parts)
{
    mlocLocale locale;
    mlocTimeFormatter formatter;
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    if (mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status != mloc_success ||
        mlocInitTimeFormatter(&formatter, &s_data, &locale, &options) < 0 ||
        mlocFormatTimeRange(&formatter, &start, &end, &output) != mloc_success)
    {
        printf("  %s: not formatted\n", id);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s: got \"%.*s\" %s, want \"%s\"\n", id, (int)output.length, text, p.out, want);
    }
    return same;
}

// As TimeIs, two instants in a zone.
static bool ZonedIs(const char* id, mlocTimeOptions options, const char* zoneId, int64_t start,
                    int64_t end, const char* want, const char* parts)
{
    mlocLocale locale;
    mlocTimeFormatter formatter;
    mlocTimeZone zone;
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    mlocInstant a = {.seconds = start};
    mlocInstant b = {.seconds = end};
    if (mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status != mloc_success ||
        mlocFindTimeZone(&s_data, zoneId, strlen(zoneId), &zone) < 0 ||
        mlocInitTimeFormatter(&formatter, &s_data, &locale, &options) < 0 ||
        mlocFormatTimeRangeInZone(&formatter, &zone, &a, &b, &output) != mloc_success)
    {
        printf("  %s: not formatted\n", id);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s: got \"%.*s\" %s, want \"%s\"\n", id, (int)output.length, text, p.out, want);
    }
    return same;
}

#define NNBSP "\xE2\x80\xAF"

static mlocTime At(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond)
{
    return (mlocTime){.hour = hour, .minute = minute, .second = second, .nanosecond = nanosecond};
}

static mlocTimeOptions Clock(mlocHourCycle cycle, mlocTimePrecision precision)
{
    return (mlocTimeOptions){.hourCycle = cycle, .precision = precision};
}

static void TestTimes(void)
{
    mlocTimeOptions minute = Clock(mloc_hourCycleClock12, mloc_timePrecisionMinute);
    CHECK(TimeIs("en", minute, At(10, 0, 0, 0), At(10, 30, 0, 0), "10:00" DASH "10:30" NNBSP "AM",
                 "h<[10]L<[:]m<[00]L=[" DASH "]h>[10]L>[:]m>[30]L=[" NNBSP "]a=[AM]"),
          "a minute's pair: am once");
    CHECK(TimeIs("en", minute, At(10, 0, 0, 0), At(12, 0, 0, 0),
                 "10:00" NNBSP "AM" DASH "12:00" NNBSP "PM", nullptr),
          "am or pm's pair");
    CHECK(TimeIs("en", minute, At(12, 0, 0, 0), At(10, 0, 0, 0),
                 "12:00" NNBSP "PM" DASH "10:00" NNBSP "AM", nullptr),
          "a start past its end");
    CHECK(TimeIs("en", minute, At(10, 0, 0, 0), At(10, 0, 0, 0), "10:00" NNBSP "AM", nullptr),
          "equal times: one");
    CHECK(TimeIs("en", Clock(mloc_hourCycleClock24, mloc_timePrecisionMinute), At(10, 0, 0, 0),
                 At(12, 0, 0, 0), "10:00" DASH "12:00", nullptr),
          "the 24-hour clock's hour pair");
    mlocTimeOptions rematched = {.hourCycle = mloc_hourCycleClock24, .style = mloc_timeStyleShort};
    CHECK(TimeIs("en", rematched, At(10, 0, 0, 0), At(11, 30, 0, 0), "10:00" DASH "11:30", nullptr),
          "a standard format on a clock asked for: the minutes' pairs of that clock");
    CHECK(TimeIs("en", Clock(mloc_hourCycleClock12, mloc_timePrecisionSecond), At(10, 0, 0, 0),
                 At(10, 0, 45, 0), "10:00:00" NNBSP "AM" DASH "10:00:45" NNBSP "AM", nullptr),
          "a second's difference: the fallback, seconds having no pair");
    mlocTimeOptions fraction = Clock(mloc_hourCycleClock12, mloc_timePrecisionFraction3);
    CHECK(TimeIs("en", fraction, At(10, 0, 0, 123000000), At(10, 0, 0, 456000000),
                 "10:00:00.123" NNBSP "AM" DASH "10:00:00.456" NNBSP "AM", nullptr) &&
              TimeIs("en", fraction, At(10, 0, 0, 123000000), At(10, 0, 0, 123999999),
                     "10:00:00.123" NNBSP "AM", nullptr),
          "a fraction's difference to the digits shown, past them none");
    // ICU4C has no precision that leaves zero minutes out.
    mlocTimeOptions optional = Clock(mloc_hourCycleClock12, mloc_timePrecisionMinuteOptional);
    CHECK(TimeIs("en", optional, At(10, 0, 0, 0), At(11, 0, 0, 0), "10" DASH "11" NNBSP "AM",
                 nullptr) &&
              TimeIs("en", optional, At(10, 0, 0, 0), At(11, 30, 0, 0),
                     "10:00" DASH "11:30" NNBSP "AM", nullptr),
          "minute-optional: the hour's pattern when both minutes are zero");
}

static void TestTimesInZone(void)
{
    // 2026-01-05T10:00Z.
    int64_t at = 1767607200;
    mlocTimeOptions zoned = Clock(mloc_hourCycleClock12, mloc_timePrecisionMinute);
    zoned.zone = mloc_zoneStyleSpecificShort;
    CHECK(ZonedIs("en", zoned, "America/Los_Angeles", at, at + 7200,
                  "2:00" DASH "4:00" NNBSP "AM PST",
                  "h<[2]L<[:]m<[00]L=[" DASH "]h>[4]L>[:]m>[00]L=[" NNBSP "]a=[AM]L=[ ]z=[PST]"),
          "a zone written once");
    CHECK(ZonedIs("en", zoned, "America/Los_Angeles", at, at + 86400,
                  "1/5/2026, 2:00" NNBSP "AM PST" DASH "1/6/2026, 2:00" NNBSP "AM PST",
                  "M<[1]L<[/]d<[5]L<[/]y<[2026]L<[, ]h<[2]L<[:]m<[00]L<[" NNBSP "]a<[AM]L<[ ]"
                  "z<[PST]L=[" DASH "]M>[1]L>[/]d>[6]L>[/]y>[2026]L>[, ]h>[2]L>[:]m>[00]L>[" NNBSP
                  "]a>[AM]L>[ ]z>[PST]"),
          "two dates: each whole, the date and the time joined by the short glue");
    mlocTimeOptions generic = zoned;
    generic.zone = mloc_zoneStyleGenericShort;
    CHECK(ZonedIs("en", generic, "America/Los_Angeles", 1772962200, 1772965800,
                  "1:30" DASH "3:30" NNBSP "AM PT", nullptr),
          "across the change to daylight time");
    CHECK(ZonedIs("en", (mlocTimeOptions){.style = mloc_timeStyleFull}, "America/Los_Angeles", at,
                  at + 7200,
                  "2:00:00" NNBSP "AM Pacific Standard Time" DASH "4:00:00" NNBSP
                  "AM Pacific Standard Time",
                  nullptr),
          "the full format: seconds, so the fallback");
    CHECK(ZonedIs("ja-u-ca-japanese", zoned, "Asia/Tokyo", 1556600400, 1556686800,
                  "H31/4/30 \xE5\x8D\x88\xE5\xBE\x8C"
                  "2:00 JST\xEF\xBD\x9E"
                  "R1/5/1 \xE5\x8D\x88\xE5\xBE\x8C"
                  "2:00 JST",
                  nullptr),
          "two eras: the short date with the era");
}

// A range of two instants (seconds since 1970) by a date-time formatter:
// kind 'T' their date-times in UTC, 'Z' in the zone; or by a date
// formatter with a zone, 'D'.
static bool BothIs(const char* id, char kind, mlocDateTimeOptions options, const char* zoneId,
                   int64_t start, int64_t end, const char* want, const char* parts)
{
    mlocLocale locale;
    mlocTimeZone zone;
    mlocTimeZone utc;
    char text[256];
    Parts p = {.text = text};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Part, .context = &p};
    mlocInstant a = {.seconds = start};
    mlocInstant b = {.seconds = end};
    mlocResult result =
        mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status == mloc_success
            ? mlocFindTimeZone(&s_data, zoneId, strlen(zoneId), &zone)
            : mloc_errorSyntax;
    result = result < 0 ? result : mlocFindTimeZone(&s_data, "UTC", 3, &utc);
    if (result >= 0 && kind == 'D')
    {
        mlocDateFormatter formatter;
        result = mlocInitDateFormatter(&formatter, &s_data, &locale, &options.date);
        result =
            result < 0 ? result : mlocFormatDateRangeInZone(&formatter, &zone, &a, &b, &output);
    }
    else if (result >= 0)
    {
        mlocDateTimeFormatter formatter;
        mlocDateTime x;
        mlocDateTime y;
        result = mlocInitDateTimeFormatter(&formatter, &s_data, &locale, &options);
        if (result >= 0 && kind == 'Z')
        {
            result = mlocFormatDateTimeRangeInZone(&formatter, &zone, &a, &b, &output);
        }
        else if (result >= 0)
        {
            result = mlocGetDateTimeFor(&utc, &a, mloc_calendarIso8601, &x) == mloc_success &&
                             mlocGetDateTimeFor(&utc, &b, mloc_calendarIso8601, &y) == mloc_success
                         ? mlocFormatDateTimeRange(&formatter, &x, &y, &output)
                         : mloc_errorInvalid;
        }
    }
    bool same = result == mloc_success && output.length == strlen(want) &&
                memcmp(text, want, output.length) == 0 &&
                (parts == nullptr || strcmp(p.out, parts) == 0);
    if (!same)
    {
        printf("  %s %c: %d \"%.*s\" %s, want \"%s\"\n", id, kind, result, (int)output.length, text,
               p.out, want);
    }
    return same;
}

static mlocDateTimeOptions DateTime(mlocDateFieldSet fields, mlocTimePrecision precision)
{
    return (mlocDateTimeOptions){
        .date = {.fields = fields, .length = mloc_dateLengthMedium},
        .time = {.hourCycle = mloc_hourCycleClock12, .precision = precision}};
}

static void TestDateTimes(void)
{
    // 2026-01-05T10:00Z.
    int64_t at = 1767607200;
    int64_t day = 86400;
    mlocDateTimeOptions ymd = DateTime(mloc_dateFieldSetYearMonthDay, mloc_timePrecisionMinute);
    CHECK(BothIs("en", 'T', ymd, "UTC", at, at + 5400, "Jan 5, 2026, 10:00" DASH "11:30" NNBSP "AM",
                 "M=[Jan]L=[ ]d=[5]L=[, ]y=[2026]L=[, ]h<[10]L<[:]m<[00]L=[" DASH
                 "]h>[11]L>[:]m>[30]L=[" NNBSP "]a=[AM]"),
          "one day: the date once, the time's pair in the Gregorian glue");
    CHECK(BothIs("en", 'T', ymd, "UTC", at, at, "Jan 5, 2026, 10:00" NNBSP "AM", nullptr) &&
              BothIs("en", 'T', ymd, "UTC", at, at + 30, "Jan 5, 2026, 10:00" NNBSP "AM", nullptr),
          "equal, or differing below the minute: one date-time");
    CHECK(BothIs("en", 'T', ymd, "UTC", at, at + 3 * day,
                 "Jan 5, 2026, 10:00" NNBSP "AM" DASH "Jan 8, 2026, 10:00" NNBSP "AM",
                 "M<[Jan]L<[ ]d<[5]L<[, ]y<[2026]L<[, ]h<[10]L<[:]m<[00]L<[" NNBSP "]a<[AM]L=[" DASH
                 "]M>[Jan]L>[ ]d>[8]L>[, ]y>[2026]L>[, ]h>[10]L>[:]m>[00]L>[" NNBSP "]a>[AM]"),
          "two days: each whole, the glue between a date-time's own");
    mlocDateTimeOptions md = DateTime(mloc_dateFieldSetMonthDay, mloc_timePrecisionMinute);
    CHECK(BothIs("en", 'T', md, "UTC", at, at + 400 * day,
                 "Jan 5, 2026, 10:00" NNBSP "AM" DASH "Feb 9, 2027, 10:00" NNBSP "AM", nullptr),
          "two years: the month and day with the year prefixed");
    mlocDateTimeOptions weekday = DateTime(mloc_dateFieldSetWeekday, mloc_timePrecisionMinute);
    weekday.date.length = mloc_dateLengthLong;
    CHECK(BothIs("en", 'T', weekday, "UTC", at, at + 3 * day,
                 "5 Monday, 10:00" NNBSP "AM" DASH "8 Thursday, 10:00" NNBSP "AM", nullptr),
          "a weekday: the day prefixed");
    CHECK(BothIs("en", 'T', DateTime(mloc_dateFieldSetYearMonthDay, mloc_timePrecisionSecond),
                 "UTC", at, at + 1800,
                 "Jan 5, 2026, 10:00:00" NNBSP "AM" DASH "10:30:00" NNBSP "AM", nullptr),
          "seconds on one day: the date once, the time's fallback");
    mlocDateTimeOptions zoned = ymd;
    zoned.time.zone = mloc_zoneStyleSpecificShort;
    CHECK(BothIs("en", 'Z', zoned, "America/Los_Angeles", at, at + 5400,
                 "Jan 5, 2026, 2:00" DASH "3:30" NNBSP "AM PST", nullptr),
          "in a zone, one day");
    mlocDateTimeOptions date = {.date = {.fields = mloc_dateFieldSetYearMonthDay,
                                         .length = mloc_dateLengthMedium,
                                         .zone = mloc_zoneStyleSpecificShort}};
    CHECK(BothIs("en", 'D', date, "America/Los_Angeles", at, at + 3600, "Jan 5, 2026, PST",
                 nullptr) &&
              BothIs("en", 'D', date, "America/Los_Angeles", at, at + 3 * day,
                     "Jan 5, 2026, PST" DASH "Jan 8, 2026, PST", nullptr),
          "a date in a zone: one day one date, two days each whole");
}

// Where the library writes the range ICU4C 79 does not.
static void TestOwnAnswers(void)
{
    // ICU leaves the year out: its date-times with seconds find no
    // interval skeleton and take no prefixed fallback ("Jan 5, 10:00:00
    // AM – Feb 9, 10:00:00 AM"); its dates with a zone neither.
    int64_t at = 1767607200;
    CHECK(BothIs("en", 'T', DateTime(mloc_dateFieldSetMonthDay, mloc_timePrecisionSecond), "UTC",
                 at, at + 400 * 86400,
                 "Jan 5, 2026, 10:00:00" NNBSP "AM" DASH "Feb 9, 2027, 10:00:00" NNBSP "AM",
                 nullptr),
          "seconds two years apart: the year prefixed");
    mlocDateTimeOptions md = {.date = {.fields = mloc_dateFieldSetMonthDay,
                                       .length = mloc_dateLengthMedium,
                                       .zone = mloc_zoneStyleSpecificShort}};
    CHECK(BothIs("en", 'D', md, "America/Los_Angeles", at, at + 400 * 86400,
                 "Jan 5, 2026, PST" DASH "Feb 9, 2027, PST", nullptr),
          "a month and day in a zone two years apart: the year prefixed");
    // ICU compares the month number alone and writes one date.
    CHECK(Is("zh-u-ca-chinese", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium),
             Iso(2025, 6, 29), Iso(2025, 7, 29),
             "2025\xE5\xB9\xB4\xE5\x85\xAD\xE6\x9C\x88\xE5\x88\x9D\xE4\xBA\x94\xE2\x80\x93"
             "2025\xE5\xB9\xB4\xE9\x97\xB0\xE5\x85\xAD\xE6\x9C\x88\xE5\x88\x9D\xE4\xBA\x94",
             nullptr),
          "a leap month differs from the month it follows");
    // fa's "LLLL تا MMMM y G": ICU writes the first month twice.
    mlocDateOptions era = Set(mloc_dateFieldSetYearMonth, mloc_dateLengthLong);
    era.yearStyle = mloc_yearStyleWithEra;
    CHECK(Is("fa-u-ca-gregory", era, Iso(2026, 1, 5), Iso(2026, 2, 8),
             "\xDA\x98\xD8\xA7\xD9\x86\xD9\x88\xDB\x8C\xD9\x87 \xD8\xAA\xD8\xA7 "
             "\xD9\x81\xD9\x88\xD8\xB1\xDB\x8C\xD9\x87 \xDB\xB2\xDB\xB0\xDB\xB2\xDB\xB6 "
             "\xD9\x85.",
             nullptr),
          "a month met twice as LLLL and MMMM is split there");
}

static void TestRefusals(void)
{
    mlocDateFormatter plain;
    mlocDateFormatter zoned;
    mlocDateOptions zone = Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium);
    zone.zone = mloc_zoneStyleGenericShort;
    mlocDate a = Iso(2026, 1, 5);
    mlocDate b = Iso(2026, 1, 8);
    mlocDate far = {.day = 300000000};
    char text[8];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocOutput measure = {0};
    mlocOutput broken = {.buffer = nullptr, .capacity = 4};
    CHECK(Formatter("en", Set(mloc_dateFieldSetYearMonthDay, mloc_dateLengthMedium), &plain),
          "a formatter");
    CHECK(mlocFormatDateRange(&plain, &a, &b, &measure) == mloc_errorCapacity &&
              measure.length == strlen("Jan 5" DASH "8, 2026") &&
              mlocFormatDateRange(&plain, &a, &b, &output) == mloc_errorCapacity,
          "measured, and a buffer too small");
    CHECK(mlocFormatDateRange(nullptr, &a, &b, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, nullptr, &b, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, &b, nullptr) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, &b, &broken) == mloc_errorInvalid,
          "null pointers");
    CHECK(mlocFormatDateRange(&plain, &far, &b, &output) == mloc_errorInvalid &&
              mlocFormatDateRange(&plain, &a, &far, &output) == mloc_errorInvalid,
          "a date outside Temporal's range at either end");
    CHECK(Formatter("en", zone, &zoned) &&
              mlocFormatDateRange(&zoned, &a, &b, &output) == mloc_errorInvalid,
          "a formatter with a zone");
    mlocDateFormatter forged;
    memset(&forged, 0x5A, sizeof(forged));
    CHECK(mlocFormatDateRange(&forged, &a, &b, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
}

static void TestTimeRefusals(void)
{
    mlocLocale en;
    mlocTimeFormatter plain;
    mlocTimeFormatter zoned;
    mlocTimeZone zone;
    mlocTimeOptions zoneOptions = Clock(mloc_hourCycleClock12, mloc_timePrecisionMinute);
    zoneOptions.zone = mloc_zoneStyleSpecificShort;
    mlocTime a = At(10, 0, 0, 0);
    mlocTime b = At(11, 0, 0, 0);
    mlocTime bad = At(24, 0, 0, 0);
    mlocInstant x = {.seconds = 0};
    mlocInstant far = {.seconds = INT64_MAX};
    char text[8];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocOutput broken = {.buffer = nullptr, .capacity = 4};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitTimeFormatter(&plain, &s_data, &en, nullptr) >= 0 &&
              mlocInitTimeFormatter(&zoned, &s_data, &en, &zoneOptions) >= 0 &&
              mlocFindTimeZone(&s_data, "Europe/Berlin", 13, &zone) >= 0,
          "formatters and a zone");
    CHECK(mlocFormatTimeRange(&plain, &a, &b, &output) == mloc_errorCapacity &&
              mlocFormatTimeRange(nullptr, &a, &b, &output) == mloc_errorInvalid &&
              mlocFormatTimeRange(&plain, nullptr, &b, &output) == mloc_errorInvalid &&
              mlocFormatTimeRange(&plain, &a, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatTimeRange(&plain, &a, &b, nullptr) == mloc_errorInvalid &&
              mlocFormatTimeRange(&plain, &a, &b, &broken) == mloc_errorInvalid &&
              mlocFormatTimeRange(&plain, &bad, &b, &output) == mloc_errorInvalid &&
              mlocFormatTimeRange(&plain, &a, &bad, &output) == mloc_errorInvalid &&
              mlocFormatTimeRange(&zoned, &a, &b, &output) == mloc_errorInvalid,
          "a time range: too small, null pointers, an hour of 24, a formatter with a zone");
    CHECK(mlocFormatTimeRangeInZone(&zoned, &zone, &x, &x, &output) == mloc_errorCapacity &&
              mlocFormatTimeRangeInZone(nullptr, &zone, &x, &x, &output) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, nullptr, &x, &x, &output) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, &zone, nullptr, &x, &output) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, &zone, &x, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, &zone, &x, &x, nullptr) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, &zone, &x, &x, &broken) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, &zone, &far, &x, &output) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&zoned, &zone, &x, &far, &output) == mloc_errorInvalid &&
              mlocFormatTimeRangeInZone(&plain, &zone, &x, &x, &output) == mloc_errorInvalid,
          "a range in a zone: too small, null pointers, instants out of range, no zone");
}

static void TestDateTimeRefusals(void)
{
    mlocLocale en;
    mlocDateTimeFormatter plain;
    mlocDateTimeFormatter zoned;
    mlocDateFormatter date;
    mlocDateFormatter dateZoned;
    mlocTimeZone zone;
    mlocDateTimeOptions zoneOptions =
        DateTime(mloc_dateFieldSetYearMonthDay, mloc_timePrecisionMinute);
    zoneOptions.time.zone = mloc_zoneStyleSpecificShort;
    mlocDateOptions dateZone = {.zone = mloc_zoneStyleSpecificShort};
    mlocDateTime a = {.date = Iso(2026, 1, 5), .time = At(10, 0, 0, 0)};
    mlocDateTime bad = {.date = Iso(2026, 1, 5), .time = At(25, 0, 0, 0)};
    mlocInstant x = {.seconds = 0};
    mlocInstant far = {.seconds = INT64_MAX};
    char text[8];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocOutput broken = {.buffer = nullptr, .capacity = 4};
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitDateTimeFormatter(&plain, &s_data, &en, nullptr) >= 0 &&
              mlocInitDateTimeFormatter(&zoned, &s_data, &en, &zoneOptions) >= 0 &&
              mlocInitDateFormatter(&date, &s_data, &en, nullptr) >= 0 &&
              mlocInitDateFormatter(&dateZoned, &s_data, &en, &dateZone) >= 0 &&
              mlocFindTimeZone(&s_data, "Europe/Berlin", 13, &zone) >= 0,
          "formatters and a zone");
    CHECK(mlocFormatDateTimeRange(&plain, &a, &a, &output) == mloc_errorCapacity &&
              mlocFormatDateTimeRange(nullptr, &a, &a, &output) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&plain, nullptr, &a, &output) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&plain, &a, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&plain, &a, &a, nullptr) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&plain, &a, &a, &broken) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&plain, &bad, &a, &output) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&plain, &a, &bad, &output) == mloc_errorInvalid &&
              mlocFormatDateTimeRange(&zoned, &a, &a, &output) == mloc_errorInvalid,
          "a date-time range: too small, null pointers, a time out of range, a zone");
    CHECK(
        mlocFormatDateTimeRangeInZone(&zoned, &zone, &x, &x, &output) == mloc_errorCapacity &&
            mlocFormatDateTimeRangeInZone(nullptr, &zone, &x, &x, &output) == mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, nullptr, &x, &x, &output) == mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, &zone, nullptr, &x, &output) ==
                mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, &zone, &x, nullptr, &output) ==
                mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, &zone, &x, &x, nullptr) == mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, &zone, &x, &x, &broken) == mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, &zone, &far, &x, &output) == mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&zoned, &zone, &x, &far, &output) == mloc_errorInvalid &&
            mlocFormatDateTimeRangeInZone(&plain, &zone, &x, &x, &output) == mloc_errorInvalid,
        "a date-time range in a zone: as the date-time's");
    CHECK(
        mlocFormatDateRangeInZone(&dateZoned, &zone, &x, &x, &output) == mloc_errorCapacity &&
            mlocFormatDateRangeInZone(nullptr, &zone, &x, &x, &output) == mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, nullptr, &x, &x, &output) == mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, &zone, nullptr, &x, &output) ==
                mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, &zone, &x, nullptr, &output) ==
                mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, &zone, &x, &x, nullptr) == mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, &zone, &x, &x, &broken) == mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, &zone, &far, &x, &output) == mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&dateZoned, &zone, &x, &far, &output) == mloc_errorInvalid &&
            mlocFormatDateRangeInZone(&date, &zone, &x, &x, &output) == mloc_errorInvalid,
        "a date range in a zone: as the date's");
}

// The parts' spans in UTF-16 units, as ICU counts them: the start's from
// its first part to its last, then the end's ("0:4:5,1:8:9"), or "-".
typedef struct Spans
{
    const char* text;
    long first[2];
    long last[2];
} Spans;

static long Units(const char* text, size_t bytes)
{
    long units = 0;
    for (size_t i = 0; i < bytes; i++)
    {
        uint8_t c = (uint8_t)text[i];
        units += (c & 0xC0) == 0x80 ? 0 : c >= 0xF0 ? 2 : 1;
    }
    return units;
}

static void Span(void* context, uint32_t part, size_t start, size_t length)
{
    Spans* s = context;
    uint32_t side = part & 0x300;
    if (side == 0)
    {
        return;
    }
    int i = side == mloc_dateRangeStart ? 0 : 1;
    long a = Units(s->text, start);
    long b = Units(s->text, start + length);
    s->first[i] = s->first[i] < 0 || a < s->first[i] ? a : s->first[i];
    s->last[i] = b > s->last[i] ? b : s->last[i];
}

// A table line's tab-separated fields; their count.
static size_t Split(char* line, char* field[11])
{
    size_t count = 0;
    for (char* at = line; count < 11 && at != nullptr; count++)
    {
        field[count] = at;
        at = strchr(at, '\t');
        if (at != nullptr)
        {
            *at++ = '\0';
        }
    }
    return count;
}

// Whether a range written matches a table line's answer: ICU's text and
// spans (fields at from), or the library's where the line has them (the
// two fields after, then the cause).
static bool Matches(size_t number, char* const field[11], size_t count, size_t from,
                    const char* text, size_t length, const Spans* s)
{
    const char* want = count >= from + 5 ? field[from + 2] : field[from];
    const char* spans = count >= from + 5 ? field[from + 3] : field[from + 1];
    char got[64] = "-";
    if (s->first[0] >= 0 || s->first[1] >= 0)
    {
        snprintf(got, sizeof(got), "0:%ld:%ld,1:%ld:%ld", s->first[0], s->last[0], s->first[1],
                 s->last[1]);
    }
    bool same =
        length == strlen(want) && memcmp(text, want, length) == 0 && strcmp(got, spans) == 0;
    if (!same)
    {
        printf("  line %zu: %s got \"%.*s\" %s, want \"%s\" %s\n", number, field[0], (int)length,
               text, got, want, spans);
    }
    return same;
}

// A date line: locale, the options' fields, length, year style and style
// (mlocDateOptions' values), the start and the end (ISO dates), then the
// answers.
static bool DateLine(char* line, size_t number)
{
    char* field[11] = {0};
    size_t count = Split(line, field);
    int o[4] = {0};
    int d[6] = {0};
    if (count < 6 || sscanf(field[1], "%d %d %d %d", &o[0], &o[1], &o[2], &o[3]) != 4 ||
        sscanf(field[2], "%d-%d-%d", &d[0], &d[1], &d[2]) != 3 ||
        sscanf(field[3], "%d-%d-%d", &d[3], &d[4], &d[5]) != 3)
    {
        printf("  line %zu: unreadable\n", number);
        return false;
    }
    mlocDateOptions options = {.fields = (mlocDateFieldSet)o[0],
                               .length = (mlocDateLength)o[1],
                               .yearStyle = (mlocYearStyle)o[2],
                               .style = (mlocDateStyle)o[3]};
    mlocDateFormatter formatter;
    mlocDate start = Iso(d[0], (uint32_t)d[1], (uint32_t)d[2]);
    mlocDate end = Iso(d[3], (uint32_t)d[4], (uint32_t)d[5]);
    char text[512];
    Spans s = {text, {-1, -1}, {0, 0}};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Span, .context = &s};
    if (!Formatter(field[0], options, &formatter) ||
        mlocFormatDateRange(&formatter, &start, &end, &output) != mloc_success)
    {
        printf("  line %zu: %s not formatted\n", number, field[0]);
        return false;
    }
    return Matches(number, field, count, 4, text, output.length, &s);
}

// A time of day from seconds since 1970, in UTC.
static mlocTime TimeOf(long long seconds)
{
    long long s = (seconds % 86400 + 86400) % 86400;
    return At((uint8_t)(s / 3600), (uint8_t)(s / 60 % 60), (uint8_t)(s % 60), 0);
}

// A time line: locale, the options' hour cycle, precision, style and zone
// (mlocTimeOptions' values), the zone (UTC for times without one), the
// start and the end (seconds since 1970), then the answers.
static bool TimeLine(char* line, size_t number)
{
    char* field[11] = {0};
    size_t count = Split(line, field);
    int o[4] = {0};
    long long a = 0;
    long long b = 0;
    if (count < 7 || sscanf(field[1], "%d %d %d %d", &o[0], &o[1], &o[2], &o[3]) != 4 ||
        sscanf(field[3], "%lld", &a) != 1 || sscanf(field[4], "%lld", &b) != 1)
    {
        printf("  line %zu: unreadable\n", number);
        return false;
    }
    mlocTimeOptions options = {.hourCycle = (mlocHourCycle)o[0],
                               .precision = (mlocTimePrecision)o[1],
                               .style = (mlocTimeStyle)o[2],
                               .zone = (mlocZoneStyle)o[3]};
    mlocLocale locale;
    mlocTimeFormatter formatter;
    mlocTimeZone zone;
    char text[512];
    Spans s = {text, {-1, -1}, {0, 0}};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Span, .context = &s};
    mlocTime x = TimeOf(a);
    mlocTime y = TimeOf(b);
    mlocInstant from = {.seconds = a};
    mlocInstant to = {.seconds = b};
    bool zoned = strcmp(field[2], "UTC") != 0;
    mlocResult result =
        mlocCanonicalizeLocale(&s_data, field[0], strlen(field[0]), &locale, nullptr).status ==
                mloc_success
            ? mlocInitTimeFormatter(&formatter, &s_data, &locale, &options)
            : mloc_errorSyntax;
    if (result >= 0 && zoned)
    {
        result = mlocFindTimeZone(&s_data, field[2], strlen(field[2]), &zone);
        result =
            result < 0 ? result : mlocFormatTimeRangeInZone(&formatter, &zone, &from, &to, &output);
    }
    else if (result >= 0)
    {
        result = mlocFormatTimeRange(&formatter, &x, &y, &output);
    }
    if (result != mloc_success)
    {
        printf("  line %zu: %s not formatted (%d)\n", number, field[0], result);
        return false;
    }
    return Matches(number, field, count, 5, text, output.length, &s);
}

// A date-time line: kind (T date-times in UTC, Z in a zone, D dates in a
// zone), locale, the options' date fields, length and year style and
// time hour cycle, precision and zone, the zone, the start and the end
// (seconds since 1970), then the answers.
static bool DateTimeLine(char* line, size_t number)
{
    char* field[11] = {0};
    size_t count = Split(line, field);
    int o[6] = {0};
    long long a = 0;
    long long b = 0;
    if (count < 8 ||
        sscanf(field[2], "%d %d %d %d %d %d", &o[0], &o[1], &o[2], &o[3], &o[4], &o[5]) != 6 ||
        sscanf(field[4], "%lld", &a) != 1 || sscanf(field[5], "%lld", &b) != 1)
    {
        printf("  line %zu: unreadable\n", number);
        return false;
    }
    char kind = field[0][0];
    mlocDateTimeOptions options = {
        .date = {.fields = (mlocDateFieldSet)o[0],
                 .length = (mlocDateLength)o[1],
                 .yearStyle = (mlocYearStyle)o[2],
                 .zone = kind == 'D' ? (mlocZoneStyle)o[5] : mloc_zoneStyleNone},
        .time = {.hourCycle = (mlocHourCycle)o[3],
                 .precision = (mlocTimePrecision)o[4],
                 .zone = kind == 'D' ? mloc_zoneStyleNone : (mlocZoneStyle)o[5]}};
    mlocLocale locale;
    mlocTimeZone zone;
    mlocTimeZone utc;
    char text[512];
    Spans s = {text, {-1, -1}, {0, 0}};
    mlocOutput output = {.buffer = text, .capacity = sizeof(text), .part = Span, .context = &s};
    mlocInstant from = {.seconds = a};
    mlocInstant to = {.seconds = b};
    mlocResult result =
        mlocCanonicalizeLocale(&s_data, field[1], strlen(field[1]), &locale, nullptr).status ==
                mloc_success
            ? mlocFindTimeZone(&s_data, field[3], strlen(field[3]), &zone)
            : mloc_errorSyntax;
    result = result < 0 ? result : mlocFindTimeZone(&s_data, "UTC", 3, &utc);
    if (result >= 0 && kind == 'D')
    {
        mlocDateFormatter formatter;
        result = mlocInitDateFormatter(&formatter, &s_data, &locale, &options.date);
        result =
            result < 0 ? result : mlocFormatDateRangeInZone(&formatter, &zone, &from, &to, &output);
    }
    else if (result >= 0)
    {
        mlocDateTimeFormatter formatter;
        mlocDateTime x;
        mlocDateTime y;
        result = mlocInitDateTimeFormatter(&formatter, &s_data, &locale, &options);
        if (result >= 0 && kind == 'Z')
        {
            result = mlocFormatDateTimeRangeInZone(&formatter, &zone, &from, &to, &output);
        }
        else if (result >= 0 &&
                 mlocGetDateTimeFor(&utc, &from, mloc_calendarIso8601, &x) == mloc_success &&
                 mlocGetDateTimeFor(&utc, &to, mloc_calendarIso8601, &y) == mloc_success)
        {
            result = mlocFormatDateTimeRange(&formatter, &x, &y, &output);
        }
    }
    if (result != mloc_success)
    {
        printf("  line %zu: %s not formatted (%d)\n", number, field[1], result);
        return false;
    }
    return Matches(number, field, count, 6, text, output.length, &s);
}

static void TestTable(const char* path, bool (*check)(char* line, size_t number))
{
    FILE* file = fopen(path, "rb");
    CHECK(file != nullptr, "a table of ICU's ranges");
    if (file == nullptr)
    {
        return;
    }
    char line[1024];
    size_t number = 0;
    size_t lines = 0;
    size_t failed = 0;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        number++;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0')
        {
            continue;
        }
        lines++;
        failed += check(line, number) ? 0 : 1;
    }
    fclose(file);
    printf("%s: %zu of %zu ranges as the table has them\n", path, lines - failed, lines);
    CHECK(lines > 0 && failed == 0, "every range of a table");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    if (!OpenFile(MLOC_DATES_FILE, &s_sources[1]) || !OpenFile(MLOC_SPELLOUT_FILE, &s_sources[2]) ||
        !OpenFile(MLOC_ZONE_NAMES_FILE, &s_sources[3]))
    {
        fputs("test_date_range: cannot open " MLOC_DATES_FILE ", " MLOC_SPELLOUT_FILE
              " or " MLOC_ZONE_NAMES_FILE "\n",
              stderr);
        return 2;
    }
    s_data = (mlocDataSet){s_sources, 4};
    TestDifferences();
    TestOneDateAndFallback();
    TestOwnAnswers();
    TestRefusals();
    TestTimes();
    TestTimesInZone();
    TestTimeRefusals();
    TestDateTimes();
    TestDateTimeRefusals();
    TestTable(MLOC_DATE_RANGES_FILE, DateLine);
    TestTable(MLOC_TIME_RANGES_FILE, TimeLine);
    TestTable(MLOC_DATETIME_RANGES_FILE, DateTimeLine);
    return s_failures == 0 ? 0 : 1;
}
