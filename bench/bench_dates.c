// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The datetime part's benchmark cases (bench_dates.h), all in en: a date
// formatter made (year, month and day, medium) and 17 October 2026
// written ("Oct 17, 2026"); 15:04:05 to the minute ("3:04 PM"); a
// date-time formatter made and both written ("Oct 17, 2026, 3:04 PM");
// the instant 19:04:05 UTC that day in America/New_York with the specific
// short zone ("Oct 17, 2026, 3:04 PM EDT"); the range from 5 to 8
// October 2026 ("Oct 5 – 8, 2026"); a relative time formatter made (long,
// numeric always) and 3 days written ("in 3 days"), and 1 day with
// 15:04:05 ("in 1 day at 3:04 PM"); a duration formatter
// made (short) and 1 hour, 2 minutes and 3 seconds written ("1 hr, 2 min,
// 3 sec").

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "bench_dates.h"

#include "maul-locale/datetime.h"
#include "maul-locale/duration.h"
#include "maul-locale/relative.h"

#include <stdio.h>
#include <string.h>

#ifndef MLOC_DATES_FILE
#define MLOC_DATES_FILE "src/generated/dates.mloc"
#endif
#ifndef MLOC_ZONE_NAMES_FILE
#define MLOC_ZONE_NAMES_FILE "src/generated/zone-names.mloc"
#endif
#ifndef MLOC_RELATIVE_FILE
#define MLOC_RELATIVE_FILE "src/generated/relative.mloc"
#endif
#ifndef MLOC_LOCALE_NAMES_FILE
#define MLOC_LOCALE_NAMES_FILE "src/generated/names.mloc"
#endif
#ifndef MLOC_UNITS_FILE
#define MLOC_UNITS_FILE "src/generated/units.mloc"
#endif
#ifndef MLOC_SPELLOUT_FILE
#define MLOC_SPELLOUT_FILE "src/generated/spellout.mloc"
#endif

static volatile int s_sink;
static mlocDataSet s_set;
static mlocLocale s_en;
static char s_text[128];

static const mlocDateOptions s_dateOptions = {.fields = mloc_dateFieldSetYearMonthDay,
                                              .length = mloc_dateLengthMedium};
static const mlocDateTimeOptions s_bothOptions = {
    .date = {.fields = mloc_dateFieldSetYearMonthDay, .length = mloc_dateLengthMedium},
    .time = {.precision = mloc_timePrecisionMinute}};
static const mlocDateTimeOptions s_zonedOptions = {
    .date = {.fields = mloc_dateFieldSetYearMonthDay, .length = mloc_dateLengthMedium},
    .time = {.precision = mloc_timePrecisionMinute, .zone = mloc_zoneStyleSpecificShort}};
static const mlocTimeOptions s_timeOptions = {.precision = mloc_timePrecisionMinute};
static const mlocRelativeOptions s_relativeOptions = {.width = mloc_relativeWidthLong};

static mlocDateFormatter s_date;
static mlocTimeFormatter s_time;
static mlocDateTimeFormatter s_both;
static mlocDateTimeFormatter s_zoned;
static mlocRelativeFormatter s_relative;
static mlocDurationFormatter s_duration;
static mlocTimeZone s_newYork;
static mlocDateTime s_value;
static mlocDate s_rangeEnd;
static mlocDate s_rangeStart;
static mlocDecimal s_three;
static mlocDecimal s_one;
static const mlocInstant s_instant = {.seconds = 1792263845};
static const mlocDurationValues s_hms = {.hours = 1, .minutes = 2, .seconds = 3};

static void Write(mlocResult r, const mlocOutput* output)
{
    s_sink = r;
    s_text[output->length < sizeof(s_text) ? output->length : 0] = '\0';
}

static void DateInit(void)
{
    mlocDateFormatter f;
    s_sink = mlocInitDateFormatter(&s_set, &s_en, &s_dateOptions, &f);
}

static void DateFormat(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocDateFormatter_Format(&s_date, &s_value.date, &o), &o);
}

static void TimeFormat(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocTimeFormatter_Format(&s_time, &s_value.time, &o), &o);
}

static void DateTimeInit(void)
{
    mlocDateTimeFormatter f;
    s_sink = mlocInitDateTimeFormatter(&s_set, &s_en, &s_bothOptions, &f);
}

static void DateTimeFormat(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocDateTimeFormatter_Format(&s_both, &s_value, &o), &o);
}

static void ZonedFormat(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocDateTimeFormatter_FormatInZone(&s_zoned, &s_newYork, &s_instant, &o), &o);
}

static void DateRange(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocDateFormatter_FormatRange(&s_date, &s_rangeStart, &s_rangeEnd, &o), &o);
}

static void RelativeInit(void)
{
    mlocRelativeFormatter f;
    s_sink = mlocInitRelativeFormatter(&s_set, &s_en, &s_relativeOptions, &f);
}

static void RelativeFormat(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocRelativeFormatter_Format(&s_relative, &s_three, mloc_relativeDay, &o), &o);
}

static void RelativeDayTime(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocRelativeFormatter_FormatDayTime(&s_relative, &s_time, &s_one, &s_value.time, &o), &o);
}

static void DurationInit(void)
{
    mlocDurationFormatter f;
    s_sink = mlocInitDurationFormatter(&s_set, &s_en, nullptr, &f);
}

static void DurationFormat(void)
{
    mlocOutput o = {.buffer = s_text, .capacity = sizeof(s_text) - 1};
    Write(mlocDurationFormatter_Format(&s_duration, &s_hms, &o), &o);
}

const BenchCase g_dateCases[] = {
    {"date-init", DateInit, 200000},
    {"date-format", DateFormat, 2000000},
    {"time-format", TimeFormat, 2000000},
    {"datetime-init", DateTimeInit, 100000},
    {"datetime-format", DateTimeFormat, 2000000},
    {"zoned-format", ZonedFormat, 1000000},
    {"date-range", DateRange, 1000000},
    {"relative-init", RelativeInit, 200000},
    {"relative-format", RelativeFormat, 2000000},
    {"relative-day-time", RelativeDayTime, 1000000},
    {"duration-init", DurationInit, 100000},
    {"duration-format", DurationFormat, 1000000},
};
const size_t g_dateCaseCount = sizeof(g_dateCases) / sizeof(g_dateCases[0]);

static bool OpenFile(const char* path, mlocData* out, uint8_t* bytes, size_t capacity)
{
    FILE* file = fopen(path, "rb");
    size_t size = file == nullptr ? 0 : fread(bytes, 1, capacity, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    return size > 0 && size < capacity && mlocOpenData(bytes, size, out) == mloc_success;
}

static bool OpenFiles(void)
{
    static mlocData s_sources[7];
    alignas(8) static uint8_t s_dates[4 << 20];
    alignas(8) static uint8_t s_zones[8 << 20];
    alignas(8) static uint8_t s_names[8 << 20];
    alignas(8) static uint8_t s_relativeBytes[2 << 20];
    alignas(8) static uint8_t s_units[8 << 20];
    alignas(8) static uint8_t s_rules[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_set = (mlocDataSet){s_sources, 7};
    return OpenFile(MLOC_DATES_FILE, &s_sources[1], s_dates, sizeof(s_dates)) &&
           OpenFile(MLOC_ZONE_NAMES_FILE, &s_sources[2], s_zones, sizeof(s_zones)) &&
           OpenFile(MLOC_LOCALE_NAMES_FILE, &s_sources[3], s_names, sizeof(s_names)) &&
           OpenFile(MLOC_RELATIVE_FILE, &s_sources[4], s_relativeBytes, sizeof(s_relativeBytes)) &&
           OpenFile(MLOC_UNITS_FILE, &s_sources[5], s_units, sizeof(s_units)) &&
           OpenFile(MLOC_SPELLOUT_FILE, &s_sources[6], s_rules, sizeof(s_rules));
}

const mlocDataSet* BenchDataSet(void)
{
    return &s_set;
}

static bool MakeValues(void)
{
    mlocDateInput day = {.year = 2026, .month = 10, .day = 17};
    mlocDateInput start = {.year = 2026, .month = 10, .day = 5};
    mlocDateInput end = {.year = 2026, .month = 10, .day = 8};
    s_value.time = (mlocTime){.hour = 15, .minute = 4, .second = 5};
    return mlocMakeDate(mloc_calendarIso8601, &day, mloc_overflowReject, &s_value.date) ==
               mloc_success &&
           mlocMakeDate(mloc_calendarIso8601, &start, mloc_overflowReject, &s_rangeStart) ==
               mloc_success &&
           mlocMakeDate(mloc_calendarIso8601, &end, mloc_overflowReject, &s_rangeEnd) ==
               mloc_success &&
           mlocMakeDecimal(3, &s_three) == mloc_success &&
           mlocMakeDecimal(1, &s_one) == mloc_success &&
           mlocFindTimeZone(&s_set, "America/New_York", 16, &s_newYork) == mloc_success;
}

bool PrepareDateBench(void)
{
    return OpenFiles() && mlocParseLocale("en", 2, &s_en).status == mloc_success && MakeValues() &&
           mlocInitDateFormatter(&s_set, &s_en, &s_dateOptions, &s_date) >= 0 &&
           mlocInitTimeFormatter(&s_set, &s_en, &s_timeOptions, &s_time) >= 0 &&
           mlocInitDateTimeFormatter(&s_set, &s_en, &s_bothOptions, &s_both) >= 0 &&
           mlocInitDateTimeFormatter(&s_set, &s_en, &s_zonedOptions, &s_zoned) >= 0 &&
           mlocInitRelativeFormatter(&s_set, &s_en, &s_relativeOptions, &s_relative) >= 0 &&
           mlocInitDurationFormatter(&s_set, &s_en, nullptr, &s_duration) >= 0;
}

void ShowDateBench(void)
{
    const BenchBody show[] = {DateFormat, TimeFormat,     DateTimeFormat,  ZonedFormat,
                              DateRange,  RelativeFormat, RelativeDayTime, DurationFormat};
    for (size_t i = 0; i < sizeof(show) / sizeof(show[0]); i++)
    {
        memset(s_text, 0, sizeof(s_text));
        show[i]();
        printf("[%s]\n", s_text);
    }
}
