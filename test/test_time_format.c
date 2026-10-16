// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time and date-time formatters (maul-locale/datetime.h) on the
// compiled-in data and dates.mloc: precisions and fractions, hour cycles
// from the options, -u-hc and the region, the standard formats, day
// periods, digits, the glue and a weekday with a time, parts, measuring,
// and the arguments refused. The texts are ICU4C 79's.

#define _CRT_SECURE_NO_WARNINGS

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mlocData s_sources[2];
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
    return read > 0 && mlocOpenData(out, bytes, read) == mloc_success;
}

static mlocTime Time(uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond)
{
    return (mlocTime){.hour = hour, .minute = minute, .second = second, .nanosecond = nanosecond};
}

static bool Make(const char* id, mlocTimeOptions options, mlocTimeFormatter* formatter)
{
    mlocLocale locale;
    return mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status ==
               mloc_success &&
           mlocInitTimeFormatter(formatter, &s_data, &locale, &options) >= 0;
}

// Whether a locale formats a time as want with the options.
static bool Is(const char* id, mlocTimeOptions options, mlocTime time, const char* want)
{
    mlocTimeFormatter formatter;
    char text[256];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (!Make(id, options, &formatter) ||
        mlocFormatTime(&formatter, &time, &output) != mloc_success)
    {
        printf("  %s: refused\n", id);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0;
    if (!same)
    {
        printf("  %s: got \"%.*s\", want \"%s\"\n", id, (int)output.length, text, want);
    }
    return same;
}

static mlocTimeOptions Precision(mlocTimePrecision precision)
{
    return (mlocTimeOptions){.precision = precision};
}

static mlocTimeOptions Cycle(mlocHourCycle cycle)
{
    return (mlocTimeOptions){.hourCycle = cycle};
}

static mlocTimeOptions Style(mlocTimeStyle style)
{
    return (mlocTimeOptions){.style = style};
}

// U+202F, the narrow no-break space before en's AM and PM.
#define NNBSP "\xE2\x80\xAF"

static void TestPrecisions(void)
{
    mlocTime time = Time(16, 3, 7, 250000000);
    CHECK(Is("en", (mlocTimeOptions){0}, time, "4:03:07" NNBSP "PM"), "seconds by default");
    CHECK(Is("en", Precision(mloc_timePrecisionHour), time, "4" NNBSP "PM"), "the hour");
    CHECK(Is("en", Precision(mloc_timePrecisionMinute), time, "4:03" NNBSP "PM"), "the minute");
    CHECK(Is("en", Precision(mloc_timePrecisionMinuteOptional), time, "4:03" NNBSP "PM") &&
              Is("en", Precision(mloc_timePrecisionMinuteOptional), Time(16, 0, 59, 0),
                 "4" NNBSP "PM"),
          "the minute unless it is zero");
    CHECK(Is("en", Precision(mloc_timePrecisionFraction3), time, "4:03:07.250" NNBSP "PM"),
          "three digits of the fraction");
    CHECK(Is("en", Precision(mloc_timePrecisionFraction1), Time(16, 3, 7, 999999999),
             "4:03:07.9" NNBSP "PM") &&
              Is("en", Precision(mloc_timePrecisionFraction9), Time(16, 3, 7, 5),
                 "4:03:07.000000005" NNBSP "PM"),
          "fractions truncated, nine digits");
    CHECK(Is("ff-Adlm", (mlocTimeOptions){0}, time,
             "\xF0\x9E\xA5\x91\xF0\x9E\xA5\x96:\xF0\x9E\xA5\x90\xF0\x9E\xA5\x93:"
             "\xF0\x9E\xA5\x90\xF0\x9E\xA5\x97"),
          "digits of four bytes");
    CHECK(Is("fa", Precision(mloc_timePrecisionFraction2), time,
             "\xDB\xB1\xDB\xB6:\xDB\xB0\xDB\xB3:\xDB\xB0\xDB\xB7\xD9\xAB\xDB\xB2\xDB\xB5"),
          "fa's digits and decimal separator");
}

static void TestHourCycles(void)
{
    mlocTime time = Time(16, 3, 7, 0);
    CHECK(Is("en", Cycle(mloc_hourCycleH23), time, "16:03:07") &&
              Is("en-u-hc-h23", (mlocTimeOptions){0}, time, "16:03:07"),
          "h23 asked for and by -u-hc");
    CHECK(Is("en-u-hc-h24", (mlocTimeOptions){0}, Time(0, 3, 7, 0), "24:03:07"),
          "h24 counts midnight's hour as 24");
    CHECK(Is("en-u-hc-h11", (mlocTimeOptions){0}, Time(12, 3, 7, 0), "0:03:07" NNBSP "PM"),
          "h11 counts noon's hour as 0");
    CHECK(Is("ja", Cycle(mloc_hourCycleClock12), time, "午後4:03:07") &&
              Is("ja", Cycle(mloc_hourCycleClock12), Time(12, 3, 7, 0), "午後0:03:07"),
          "ja's own 12-hour clock from 0");
    CHECK(Is("ja-u-hc-h12", (mlocTimeOptions){0}, time, "午後4:03:07"), "ja under h12");
    CHECK(Is("ja", (mlocTimeOptions){0}, time, "16:03:07"), "ja's clock");
    CHECK(Is("en-JP", (mlocTimeOptions){0}, time, "16:03:07") &&
              Is("de-US", (mlocTimeOptions){0}, time, "4:03:07" NNBSP "PM"),
          "the region's clock");
    CHECK(Is("en", Cycle(mloc_hourCycleClock24), time, "16:03:07"), "en's 24-hour clock");
    CHECK(Is("en-u-ca-iso8601", (mlocTimeOptions){0}, time, "4:03:07" NNBSP "PM"),
          "the ISO calendar's times, the Gregorian ones");
}

static void TestStyles(void)
{
    mlocTime time = Time(16, 3, 7, 0);
    CHECK(Is("en", Style(mloc_timeStyleShort), time, "4:03" NNBSP "PM") &&
              Is("en", Style(mloc_timeStyleMedium), time, "4:03:07" NNBSP "PM"),
          "en's short and medium times");
    CHECK(Is("en-u-hc-h23", Style(mloc_timeStyleShort), time, "16:03") &&
              Is("en-JP", Style(mloc_timeStyleShort), time, "16:03") &&
              Is("en-u-hc-h24", Style(mloc_timeStyleShort), Time(0, 3, 0, 0), "24:03"),
          "a cycle or region asked for takes the clock's minutes");
    CHECK(Is("de-US", Style(mloc_timeStyleShort), time, "4:03" NNBSP "PM") &&
              Is("am-u-hc-h12", Style(mloc_timeStyleShort), Time(13, 5, 0, 0), "ከሰዓት 1:05") &&
              Is("seh-u-hc-h23", Style(mloc_timeStyleShort), Time(0, 0, 0, 0), "00:00"),
          "a region's other clock, or a cycle asked for, matched without the standard formats");
    CHECK(Is("ku-Latn-IQ", Style(mloc_timeStyleShort), Time(13, 5, 0, 0), "13:05") &&
              Is("hsb-u-ca-buddhist", Style(mloc_timeStyleShort), Time(0, 0, 0, 0), "0:00 hodź."),
          "a locale's own standard format, whatever its region's clock or its quoted text");
    CHECK(Is("zh-Hant-u-hc-h12", Style(mloc_timeStyleShort), time, "下午4:03"),
          "zh_Hant's day period by the matched pattern under h12");
    CHECK(Is("en",
             (mlocTimeOptions){.style = mloc_timeStyleShort,
                               .precision = mloc_timePrecisionFraction3},
             time, "4:03" NNBSP "PM"),
          "a standard format ignores the precision");
}

static void TestDayPeriods(void)
{
    mlocTimeOptions shortTime = Style(mloc_timeStyleShort);
    CHECK(Is("zh-Hant", shortTime, Time(21, 3, 0, 0), "晚上9:03") &&
              Is("zh-Hant", shortTime, Time(16, 3, 0, 0), "下午4:03"),
          "zh_Hant's flexible periods");
    CHECK(Is("zh-Hant", shortTime, Time(12, 0, 0, 0), "中午12:00") &&
              Is("zh-Hant", shortTime, Time(12, 30, 0, 0), "中午12:30"),
          "zh's rules name no noon: the hour's period");
    CHECK(Is("zh-Hant", shortTime, Time(0, 0, 0, 0), "凌晨12:00"),
          "midnight left out, the hour's period instead");
}

static void ReportPart(void* context, uint32_t part, size_t start, size_t length)
{
    char* parts = context;
    char item[32];
    snprintf(item, sizeof(item), "%u:%zu:%zu ", part, start, length);
    strcat(parts, item);
}

static void TestOutput(void)
{
    mlocTimeFormatter formatter;
    CHECK(Make("en", Precision(mloc_timePrecisionFraction3), &formatter), "a formatter");
    mlocTime time = Time(16, 3, 7, 250000000);
    char parts[256] = "";
    char text[64];
    mlocOutput output = {
        .buffer = text, .capacity = sizeof(text), .part = ReportPart, .context = parts};
    CHECK(mlocFormatTime(&formatter, &time, &output) == mloc_success &&
              strcmp(parts, "8:0:1 0:1:1 9:2:2 0:4:1 10:5:2 0:7:1 11:8:3 0:11:3 12:14:2 ") == 0,
          "the parts of \"4:03:07.250 PM\"");
    mlocOutput measure = {0};
    CHECK(mlocFormatTime(&formatter, &time, &measure) == mloc_errorCapacity &&
              measure.length == strlen("4:03:07.250" NNBSP "PM"),
          "measuring");
    mlocOutput small = {.buffer = text, .capacity = 5};
    CHECK(mlocFormatTime(&formatter, &time, &small) == mloc_errorCapacity &&
              small.length == measure.length,
          "a buffer too small");
}

static void TestRefusals(void)
{
    mlocLocale en;
    mlocTimeFormatter formatter;
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success,
          "en parsed");
    mlocTimeOptions full = Style(mloc_timeStyleFull);
    mlocTimeOptions longTime = Style(mloc_timeStyleLong);
    mlocTimeOptions cycle = Cycle((mlocHourCycle)7);
    mlocTimeOptions precision = Precision((mlocTimePrecision)13);
    mlocTimeOptions style = Style((mlocTimeStyle)5);
    mlocTimeOptions styledZone = {.style = mloc_timeStyleFull, .zone = mloc_zoneStyleCity};
    mlocTimeOptions zone = {.zone = (mlocZoneStyle)9};
    CHECK(mlocInitTimeFormatter(&formatter, &s_data, &en, &full) == mloc_errorNotFound &&
              mlocInitTimeFormatter(&formatter, &s_data, &en, &longTime) == mloc_errorNotFound,
          "the full and long styles, which name a zone, without the zone names");
    CHECK(mlocInitTimeFormatter(&formatter, &s_data, &en, &styledZone) == mloc_errorInvalid &&
              mlocInitTimeFormatter(&formatter, &s_data, &en, &zone) == mloc_errorInvalid,
          "a zone style with a time style, an unknown zone style");
    CHECK(mlocInitTimeFormatter(&formatter, &s_data, &en, &cycle) == mloc_errorInvalid &&
              mlocInitTimeFormatter(&formatter, &s_data, &en, &precision) == mloc_errorInvalid &&
              mlocInitTimeFormatter(&formatter, &s_data, &en, &style) == mloc_errorInvalid,
          "options out of range");
    CHECK(mlocInitTimeFormatter(nullptr, &s_data, &en, nullptr) == mloc_errorInvalid &&
              mlocInitTimeFormatter(&formatter, nullptr, &en, nullptr) == mloc_errorInvalid &&
              mlocInitTimeFormatter(&formatter, &s_data, nullptr, nullptr) == mloc_errorInvalid,
          "null arguments");
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocInitTimeFormatter(&formatter, &builtin, &en, nullptr) == mloc_errorNotFound,
          "data without the dates");
    CHECK(mlocInitTimeFormatter(&formatter, &s_data, &en, nullptr) == mloc_success, "the defaults");
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    mlocTime late = Time(24, 0, 0, 0);
    mlocTime second = Time(1, 0, 60, 0);
    mlocTime nano = Time(1, 0, 0, 1000000000);
    mlocTime time = Time(1, 0, 0, 0);
    CHECK(mlocFormatTime(&formatter, &late, &output) == mloc_errorInvalid &&
              mlocFormatTime(&formatter, &second, &output) == mloc_errorInvalid &&
              mlocFormatTime(&formatter, &nano, &output) == mloc_errorInvalid,
          "times out of range");
    mlocTimeFormatter blank = {0};
    mlocOutput noBuffer = {.capacity = 4};
    CHECK(mlocFormatTime(&blank, &time, &output) == mloc_errorInvalid &&
              mlocFormatTime(nullptr, &time, &output) == mloc_errorInvalid &&
              mlocFormatTime(&formatter, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatTime(&formatter, &time, nullptr) == mloc_errorInvalid &&
              mlocFormatTime(&formatter, &time, &noBuffer) == mloc_errorInvalid,
          "a formatter the library did not make, null arguments, a capacity without a buffer");
}

// Whether a locale formats the fifteenth of October 2026 at a time as
// want with the options.
static bool DateTimeIs(const char* id, mlocDateTimeOptions options, mlocTime time, const char* want)
{
    mlocLocale locale;
    mlocDateTimeFormatter formatter;
    mlocDateTime value = {.time = time};
    mlocDateInput input = {.year = 2026, .month = 10, .day = 15};
    char text[256];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    if (mlocCanonicalizeLocale(&s_data, id, strlen(id), &locale, nullptr).status != mloc_success ||
        mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &value.date) !=
            mloc_success ||
        mlocInitDateTimeFormatter(&formatter, &s_data, &locale, &options) < 0 ||
        mlocFormatDateTime(&formatter, &value, &output) != mloc_success)
    {
        printf("  %s: refused\n", id);
        return false;
    }
    bool same = output.length == strlen(want) && memcmp(text, want, output.length) == 0;
    if (!same)
    {
        printf("  %s: got \"%.*s\", want \"%s\"\n", id, (int)output.length, text, want);
    }
    return same;
}

static mlocDateTimeOptions Composite(mlocDateFieldSet fields, mlocDateLength length,
                                     mlocTimePrecision precision)
{
    return (mlocDateTimeOptions){.date = {.fields = fields, .length = length},
                                 .time = {.precision = precision}};
}

static mlocDateTimeOptions Styles(mlocDateStyle date, mlocTimeStyle time)
{
    return (mlocDateTimeOptions){.date = {.style = date}, .time = {.style = time}};
}

static void TestDateTimes(void)
{
    mlocTime time = Time(16, 3, 7, 0);
    CHECK(
        DateTimeIs("en", (mlocDateTimeOptions){0}, time, "October 15, 2026 at 4:03:07" NNBSP "PM"),
        "the defaults: the long glue");
    CHECK(DateTimeIs("en",
                     Composite(mloc_dateFieldSetYearMonthDayWeekday, mloc_dateLengthLong,
                               mloc_timePrecisionMinute),
                     time, "Thursday, October 15, 2026 at 4:03" NNBSP "PM") &&
              DateTimeIs("de",
                         Composite(mloc_dateFieldSetYearMonthDayWeekday, mloc_dateLengthLong,
                                   mloc_timePrecisionMinute),
                         time, "Donnerstag, 15. Oktober 2026 um 16:03"),
          "the full glue: a month written out with a weekday");
    CHECK(DateTimeIs("es",
                     Composite(mloc_dateFieldSetYearMonthDayWeekday, mloc_dateLengthLong,
                               mloc_timePrecisionMinute),
                     time, "jueves, 15 de octubre de 2026, 16:03") &&
              DateTimeIs("es",
                         Composite(mloc_dateFieldSetYearMonthDay, mloc_dateLengthLong,
                                   mloc_timePrecisionMinute),
                         time, "15 de octubre de 2026 a las 16:03"),
          "es's full and long glue, which differ");
    CHECK(DateTimeIs(
              "en",
              Composite(mloc_dateFieldSetMonthDay, mloc_dateLengthMedium, mloc_timePrecisionMinute),
              time, "Oct 15, 4:03" NNBSP "PM") &&
              DateTimeIs(
                  "en",
                  Composite(mloc_dateFieldSetDay, mloc_dateLengthShort, mloc_timePrecisionMinute),
                  time, "15, 4:03" NNBSP "PM") &&
              DateTimeIs("en",
                         Composite(mloc_dateFieldSetDayWeekday, mloc_dateLengthMedium,
                                   mloc_timePrecisionMinute),
                         time, "15 Thu, 4:03" NNBSP "PM"),
          "the medium and short glue");
    CHECK(DateTimeIs("en",
                     Composite(mloc_dateFieldSetMonthDay, mloc_dateLengthLong,
                               mloc_timePrecisionMinuteOptional),
                     Time(16, 0, 0, 0), "October 15 at 4" NNBSP "PM"),
          "minute-optional precision with a date");
    CHECK(DateTimeIs(
              "en",
              Composite(mloc_dateFieldSetWeekday, mloc_dateLengthLong, mloc_timePrecisionMinute),
              time, "Thursday 4:03" NNBSP "PM") &&
              DateTimeIs("en",
                         Composite(mloc_dateFieldSetWeekday, mloc_dateLengthShort,
                                   mloc_timePrecisionSecond),
                         time, "Thu 4:03:07" NNBSP "PM") &&
              DateTimeIs("ja",
                         Composite(mloc_dateFieldSetWeekday, mloc_dateLengthLong,
                                   mloc_timePrecisionMinute),
                         time, "16:03 (木曜日)") &&
              DateTimeIs("en",
                         Composite(mloc_dateFieldSetWeekday, mloc_dateLengthLong,
                                   mloc_timePrecisionMinuteOptional),
                         Time(16, 0, 0, 0), "Thursday 4" NNBSP "PM"),
          "a weekday with a time, matched whole");
    CHECK(DateTimeIs("en", Styles(mloc_dateStyleFull, mloc_timeStyleMedium), time,
                     "Thursday, October 15, 2026 at 4:03:07" NNBSP "PM") &&
              DateTimeIs("en", Styles(mloc_dateStyleShort, mloc_timeStyleShort), time,
                         "10/15/26, 4:03" NNBSP "PM") &&
              DateTimeIs("fr", Styles(mloc_dateStyleLong, mloc_timeStyleShort), time,
                         "15 octobre 2026 à 16:03"),
          "standard formats glued by the date's length");
    mlocDateTimeOptions standard = Styles(mloc_dateStyleFull, mloc_timeStyleShort);
    standard.glue = mloc_dateTimeGlueStandard;
    CHECK(DateTimeIs("en", standard, time, "Thursday, October 15, 2026, 4:03" NNBSP "PM"),
          "the standard glue asked for");
}

static void ReportDateTimePart(void* context, uint32_t part, size_t start, size_t length)
{
    char* parts = context;
    char item[32];
    snprintf(item, sizeof(item), "%u:%zu:%zu ", part, start, length);
    strcat(parts, item);
}

static void TestDateTimeOutput(void)
{
    mlocLocale en;
    mlocDateTimeFormatter formatter;
    mlocDateTimeOptions options =
        Composite(mloc_dateFieldSetMonthDay, mloc_dateLengthMedium, mloc_timePrecisionHour);
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, &en, nullptr).status == mloc_success &&
              mlocInitDateTimeFormatter(&formatter, &s_data, &en, &options) == mloc_success,
          "a date-time formatter");
    mlocDateTime value = {.date = {0}, .time = Time(16, 3, 7, 0)};
    mlocDateInput input = {.year = 2026, .month = 10, .day = 15};
    char parts[256] = "";
    char text[64];
    mlocOutput output = {
        .buffer = text, .capacity = sizeof(text), .part = ReportDateTimePart, .context = parts};
    CHECK(mlocMakeDate(mloc_calendarIso8601, &input, mloc_overflowReject, &value.date) ==
                  mloc_success &&
              mlocFormatDateTime(&formatter, &value, &output) == mloc_success &&
              strcmp(parts, "5:0:3 0:3:1 7:4:2 0:6:2 8:8:1 0:9:3 12:12:2 ") == 0,
          "the parts of \"Oct 15, 4 PM\"");
    mlocOutput measure = {0};
    CHECK(mlocFormatDateTime(&formatter, &value, &measure) == mloc_errorCapacity &&
              measure.length == output.length,
          "measuring");
    mlocDateTimeOptions period =
        Composite(mloc_dateFieldSetMonth, mloc_dateLengthLong, mloc_timePrecisionMinute);
    mlocDateTimeOptions oneSided = {.date = {.style = mloc_dateStyleShort}};
    mlocDateTimeOptions zone =
        Composite(mloc_dateFieldSetDay, mloc_dateLengthShort, mloc_timePrecisionMinute);
    zone.date.zone = mloc_zoneStyleGenericShort;
    mlocDateTimeOptions glue = {.glue = (mlocDateTimeGlue)2};
    CHECK(mlocInitDateTimeFormatter(&formatter, &s_data, &en, &period) == mloc_errorInvalid &&
              mlocInitDateTimeFormatter(&formatter, &s_data, &en, &oneSided) == mloc_errorInvalid &&
              mlocInitDateTimeFormatter(&formatter, &s_data, &en, &zone) == mloc_errorInvalid &&
              mlocInitDateTimeFormatter(&formatter, &s_data, &en, &glue) == mloc_errorInvalid,
          "a calendar period, a standard format on one side, a zone on the date, an unknown glue");
    CHECK(mlocInitDateTimeFormatter(nullptr, &s_data, &en, nullptr) == mloc_errorInvalid &&
              mlocInitDateTimeFormatter(&formatter, nullptr, &en, nullptr) == mloc_errorInvalid &&
              mlocInitDateTimeFormatter(&formatter, &s_data, nullptr, nullptr) == mloc_errorInvalid,
          "null arguments");
    mlocDataSet builtin = {s_sources, 1};
    CHECK(mlocInitDateTimeFormatter(&formatter, &builtin, &en, nullptr) == mloc_errorNotFound,
          "data without the dates");
    CHECK(mlocInitDateTimeFormatter(&formatter, &s_data, &en, nullptr) == mloc_success,
          "the defaults");
    mlocDateTime late = value;
    late.time.hour = 24;
    mlocDateTime far = value;
    far.date.day = mlocDateMaxDay + 1;
    mlocDateTimeFormatter blank = {0};
    mlocOutput noBuffer = {.capacity = 4};
    output.part = nullptr;
    CHECK(mlocFormatDateTime(&formatter, &late, &output) == mloc_errorInvalid &&
              mlocFormatDateTime(&formatter, &far, &output) == mloc_errorInvalid &&
              mlocFormatDateTime(&blank, &value, &output) == mloc_errorInvalid &&
              mlocFormatDateTime(nullptr, &value, &output) == mloc_errorInvalid &&
              mlocFormatDateTime(&formatter, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatDateTime(&formatter, &value, nullptr) == mloc_errorInvalid &&
              mlocFormatDateTime(&formatter, &value, &noBuffer) == mloc_errorInvalid,
          "refused values and arguments");
}

int main(void)
{
    mlocOpenBuiltinData(&s_sources[0]);
    if (!OpenFile(MLOC_DATES_FILE, &s_sources[1]))
    {
        fputs("test_time_format: cannot open " MLOC_DATES_FILE "\n", stderr);
        return 2;
    }
    s_data = (mlocDataSet){s_sources, 2};
    TestPrecisions();
    TestHourCycles();
    TestStyles();
    TestDayPeriods();
    TestOutput();
    TestRefusals();
    TestDateTimes();
    TestDateTimeOutput();
    return s_failures == 0 ? 0 : 1;
}
