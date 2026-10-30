// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a failed call of calendar.h, datetime.h, relative.h and duration.h
// leaves in its outputs: each output filled with a pattern, each call
// made to fail in the ways the compiled-in data lets it (which has none of
// the dates, relative or unit names data files), and every byte of the
// pattern checked after. A failure leaves its outputs as they were
// (base.h), but for the length mloc_errorCapacity gives an mlocOutput.

#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/datetime.h"
#include "maul-locale/duration.h"
#include "maul-locale/relative.h"
#include "maul-locale/zone.h"

#include <stdint.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static mlocDataSet s_data;
static mlocLocale s_en;
static mlocDate s_date;
static mlocTimeZone s_zone;
static const mlocInstant s_epoch = {0, 0};

static void Fill(void* output, size_t size)
{
    memset(output, pattern, size);
}

static bool Kept(const void* output, size_t size)
{
    const unsigned char* bytes = output;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != pattern)
        {
            return false;
        }
    }
    return true;
}

// An output over a patterned buffer, its length a pattern too.
typedef struct Text
{
    char buffer[64];
    mlocOutput output;
} Text;

static void FillText(Text* text)
{
    Fill(text->buffer, sizeof(text->buffer));
    text->output = (mlocOutput){.buffer = text->buffer, .capacity = sizeof(text->buffer)};
    Fill(&text->output.length, sizeof(text->output.length));
}

static bool KeptText(const Text* text)
{
    return Kept(text->buffer, sizeof(text->buffer)) &&
           Kept(&text->output.length, sizeof(text->output.length));
}

static void TestCalendar(void)
{
    mlocDateInput thirteenth = {.year = 2026, .month = 13, .day = 1};
    mlocDateChanges changes = {.given = UINT32_MAX};
    mlocDateChanges monthDay = {.month = 13, .day = 1};
    mlocDateDuration far = {.years = INT64_MAX};
    mlocDateRounding rounding = {mloc_dateUnitYear, mloc_dateUnitDay, 0, 0};
    mlocDate forged = s_date;
    forged.calendar = UINT8_MAX;
    mlocDate date;
    mlocDateFields fields;
    mlocDateDuration duration;
    Fill(&date, sizeof(date));
    Fill(&fields, sizeof(fields));
    Fill(&duration, sizeof(duration));
    CHECK(mlocMakeDate(mloc_calendarGregorian, &thirteenth, mloc_overflowReject, &date) ==
                  mloc_errorInvalid &&
              mlocMakeDateFromDay(mloc_calendarGregorian, INT32_MAX, &date) == mloc_errorInvalid &&
              mlocGetDateFields(&forged, &fields) == mloc_errorInvalid &&
              mlocAddToDate(&s_date, &far, mloc_overflowConstrain, &date) == mloc_errorInvalid &&
              mlocDateUntil(&s_date, &forged, mloc_dateUnitDay, &duration) == mloc_errorInvalid &&
              mlocDateUntilRounded(&s_date, &s_date, &rounding, &duration) == mloc_errorInvalid &&
              mlocWithDate(&s_date, &changes, mloc_overflowConstrain, &date) == mloc_errorInvalid &&
              mlocMakeMonthDay(mloc_calendarGregorian, &monthDay, mloc_overflowReject, &date) ==
                  mloc_errorInvalid &&
              Kept(&date, sizeof(date)) && Kept(&fields, sizeof(fields)) &&
              Kept(&duration, sizeof(duration)),
          "calendar arithmetic refused leaves its outputs");
}

static void TestDateTimeFormatters(void)
{
    mlocDateFormatter date;
    mlocTimeFormatter time;
    mlocDateTimeFormatter dateTime;
    mlocZoneFormatter zone;
    Fill(&date, sizeof(date));
    Fill(&time, sizeof(time));
    Fill(&dateTime, sizeof(dateTime));
    Fill(&zone, sizeof(zone));
    CHECK(mlocInitDateFormatter(&s_data, &s_en, nullptr, &date) == mloc_errorNotFound &&
              mlocInitTimeFormatter(&s_data, &s_en, nullptr, &time) == mloc_errorNotFound &&
              mlocInitDateTimeFormatter(&s_data, &s_en, nullptr, &dateTime) == mloc_errorNotFound &&
              mlocInitZoneFormatter(&s_data, &s_en, (mlocZoneStyle)99, &zone) ==
                  mloc_errorInvalid &&
              Kept(&date, sizeof(date)) && Kept(&time, sizeof(time)) &&
              Kept(&dateTime, sizeof(dateTime)) && Kept(&zone, sizeof(zone)),
          "a formatter refused leaves the formatter");
    // Formatters the library did not make.
    memset(&date, 0, sizeof(date));
    memset(&time, 0, sizeof(time));
    memset(&dateTime, 0, sizeof(dateTime));
    memset(&zone, 0, sizeof(zone));
    mlocTime noon = {.hour = 12};
    mlocDateTime when = {s_date, noon};
    Text text;
    FillText(&text);
    CHECK(
        mlocDateFormatter_Format(&date, &s_date, &text.output) == mloc_errorInvalid &&
            mlocDateFormatter_FormatInZone(&date, &s_zone, &s_epoch, &text.output) ==
                mloc_errorInvalid &&
            mlocDateFormatter_FormatRange(&date, &s_date, &s_date, &text.output) ==
                mloc_errorInvalid &&
            mlocDateFormatter_FormatRangeInZone(&date, &s_zone, &s_epoch, &s_epoch, &text.output) ==
                mloc_errorInvalid &&
            mlocTimeFormatter_Format(&time, &noon, &text.output) == mloc_errorInvalid &&
            mlocTimeFormatter_FormatInZone(&time, &s_zone, &s_epoch, &text.output) ==
                mloc_errorInvalid &&
            mlocTimeFormatter_FormatRange(&time, &noon, &noon, &text.output) == mloc_errorInvalid &&
            mlocTimeFormatter_FormatRangeInZone(&time, &s_zone, &s_epoch, &s_epoch, &text.output) ==
                mloc_errorInvalid &&
            mlocDateTimeFormatter_Format(&dateTime, &when, &text.output) == mloc_errorInvalid &&
            mlocDateTimeFormatter_FormatInZone(&dateTime, &s_zone, &s_epoch, &text.output) ==
                mloc_errorInvalid &&
            mlocDateTimeFormatter_FormatRange(&dateTime, &when, &when, &text.output) ==
                mloc_errorInvalid &&
            mlocDateTimeFormatter_FormatRangeInZone(&dateTime, &s_zone, &s_epoch, &s_epoch,
                                                    &text.output) == mloc_errorInvalid &&
            mlocZoneFormatter_Format(&zone, &s_zone, &s_epoch, &text.output) == mloc_errorInvalid &&
            KeptText(&text),
        "a format refused leaves the output");
}

static void TestRelativeAndDuration(void)
{
    mlocRelativeFormatter relative;
    mlocDurationFormatter duration;
    Fill(&relative, sizeof(relative));
    Fill(&duration, sizeof(duration));
    CHECK(mlocInitRelativeFormatter(&s_data, &s_en, nullptr, &relative) == mloc_errorNotFound &&
              mlocInitDurationFormatter(&s_data, &s_en, nullptr, &duration) == mloc_errorNotFound &&
              Kept(&relative, sizeof(relative)) && Kept(&duration, sizeof(duration)),
          "a formatter refused leaves the formatter");
    memset(&relative, 0, sizeof(relative));
    memset(&duration, 0, sizeof(duration));
    mlocDecimal one;
    mlocDurationValues values = {.days = 1};
    mlocDurationOptions options;
    Fill(&options, sizeof(options));
    Text text;
    FillText(&text);
    CHECK(mlocMakeDecimal(1, &one) == mloc_success &&
              mlocRelativeFormatter_Format(&relative, &one, mloc_relativeYear, &text.output) ==
                  mloc_errorInvalid &&
              mlocDurationFormatter_Format(&duration, &values, &text.output) == mloc_errorInvalid &&
              mlocDurationFormatter_GetOptions(&duration, &options) == mloc_errorInvalid &&
              KeptText(&text) && Kept(&options, sizeof(options)),
          "a format or options refused leave the output");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success &&
              mlocMakeDateFromDay(mloc_calendarGregorian, 739000, &s_date) == mloc_success &&
              mlocFindTimeZone(&s_data, "Europe/Paris", 12, &s_zone) == mloc_success,
          "a locale, a date and a zone to work with");
    TestCalendar();
    TestDateTimeFormatters();
    TestRelativeAndDuration();
    return s_failures == 0 ? 0 : 1;
}
