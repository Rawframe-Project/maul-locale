// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a failed call of zone.h and ixdtf.h leaves in its outputs: each
// output filled with a pattern, each call made to fail in the ways the
// compiled-in data lets it, and every byte of the pattern checked after.
// A failure leaves its outputs as they were (base.h), but for the length
// mloc_errorCapacity gives an mlocOutput.

#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/ixdtf.h"
#include "maul-locale/zone.h"

#include <stdint.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static mlocDataSet s_data;
static mlocDataSet s_empty;
// A zone of a fixed offset, a named one, and one the library did not make.
static mlocTimeZone s_offset;
static mlocTimeZone s_named;
static mlocTimeZone s_forged;
static mlocDateTime s_dateTime;
// Past Temporal's range of instants.
static const mlocInstant s_far = {INT64_MAX / 2, 0};
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

static void TestParse(void)
{
    mlocTemporalParse parse;
    Fill(&parse, sizeof(parse));
    CHECK(mlocParseTemporalString("2026-13-01", 10, mloc_formatDateTime, &parse) ==
                  mloc_errorSyntax &&
              mlocParseTemporalString("2026-01-01", 10, 0, &parse) == mloc_errorInvalid &&
              Kept(&parse, sizeof(parse)),
          "a string refused leaves the parse");
    int64_t nanoseconds;
    bool hasSeconds;
    Fill(&nanoseconds, sizeof(nanoseconds));
    Fill(&hasSeconds, sizeof(hasSeconds));
    CHECK(mlocParseUTCOffset("+25:00", 6, &nanoseconds, &hasSeconds) == mloc_errorSyntax &&
              Kept(&nanoseconds, sizeof(nanoseconds)) && Kept(&hasSeconds, sizeof(hasSeconds)),
          "an offset refused leaves the offset and the seconds flag");
    mlocTimeZoneParse zone;
    mlocTextSpan calendar;
    mlocDurationValues values;
    Fill(&zone, sizeof(zone));
    Fill(&calendar, sizeof(calendar));
    Fill(&values, sizeof(values));
    CHECK(mlocParseTimeZoneString("+01:00:30", 9, &zone) == mloc_errorSyntax &&
              mlocParseCalendarString("[u-ca=", 6, &calendar) == mloc_errorSyntax &&
              mlocParseDuration("P1", 2, &values) == mloc_errorSyntax &&
              mlocParseDuration("P9999999999Y", 12, &values) == mloc_errorInvalid &&
              Kept(&zone, sizeof(zone)) && Kept(&calendar, sizeof(calendar)) &&
              Kept(&values, sizeof(values)),
          "a zone, calendar or duration refused leaves its output");
}

static void TestFormat(void)
{
    mlocTemporalStringOptions bad = {.secondsPrecision = 99};
    mlocDurationValues mixed = {.years = 1, .days = -1};
    Text text;
    FillText(&text);
    CHECK(mlocFormatTemporalDate(&s_dateTime.date, &bad, &text.output) == mloc_errorInvalid &&
              mlocFormatYearMonth(&s_dateTime.date, &bad, &text.output) == mloc_errorInvalid &&
              mlocFormatMonthDay(&s_dateTime.date, &bad, &text.output) == mloc_errorInvalid &&
              mlocFormatTemporalTime(&s_dateTime.time, &bad, &text.output) == mloc_errorInvalid &&
              mlocFormatTemporalDateTime(&s_dateTime, &bad, &text.output) == mloc_errorInvalid &&
              mlocFormatInstant(&s_far, nullptr, nullptr, &text.output) == mloc_errorInvalid &&
              mlocFormatZonedDateTime(&s_forged, &s_epoch, nullptr, &text.output) ==
                  mloc_errorInvalid &&
              mlocFormatTemporalDuration(&mixed, nullptr, &text.output) == mloc_errorInvalid &&
              mlocFormatUTCOffset(86400000000000, &text.output) == mloc_errorInvalid &&
              KeptText(&text),
          "a string refused leaves the output");
}

static void TestZones(void)
{
    mlocTimeZone zone;
    Fill(&zone, sizeof(zone));
    CHECK(mlocFindTimeZone(&s_data, "+1", 2, &zone) == mloc_errorSyntax &&
              mlocFindTimeZone(&s_data, "Mars/Olympus", 12, &zone) == mloc_errorNotFound &&
              mlocFindTimeZone(&s_empty, "Europe/Paris", 12, &zone) == mloc_errorNotFound &&
              mlocGetTimeZone(&s_data, SIZE_MAX, &zone) == mloc_errorInvalid &&
              mlocFindTimeZoneBcp47(&s_data, "zzzzz", 5, &zone) == mloc_errorNotFound &&
              mlocFindTimeZoneWindows(&s_data, "Mars Time", 9, nullptr, 0, &zone) ==
                  mloc_errorNotFound &&
              Kept(&zone, sizeof(zone)),
          "a zone not found leaves the zone");
    Text text;
    FillText(&text);
    CHECK(mlocFormatTimeZoneId(&s_forged, &text.output) == mloc_errorInvalid &&
              mlocFormatPrimaryTimeZoneId(&s_forged, &text.output) == mloc_errorInvalid &&
              mlocGetTimeZoneRelease(&s_empty, &text.output) == mloc_errorNotFound &&
              mlocFormatTimeZoneBcp47(&s_empty, &s_named, &text.output) == mloc_errorNotFound &&
              mlocFormatTimeZoneWindows(&s_empty, &s_named, &text.output) == mloc_errorNotFound &&
              KeptText(&text),
          "an identifier refused leaves the output");
    bool equal;
    int32_t offset;
    mlocInstant transition;
    Fill(&equal, sizeof(equal));
    Fill(&offset, sizeof(offset));
    Fill(&transition, sizeof(transition));
    CHECK(mlocTimeZonesEqual(&s_named, &s_forged, &equal) == mloc_errorInvalid &&
              mlocGetTimeZoneOffset(&s_named, &s_far, &offset) == mloc_errorInvalid &&
              mlocGetTimeZoneTransition(&s_offset, &s_epoch, mloc_transitionNext, &transition) ==
                  mloc_errorNotFound &&
              mlocGetTimeZoneTransition(&s_named, &s_epoch, (mlocTransitionDirection)2,
                                        &transition) == mloc_errorInvalid &&
              Kept(&equal, sizeof(equal)) && Kept(&offset, sizeof(offset)) &&
              Kept(&transition, sizeof(transition)),
          "a comparison, offset or transition refused leaves its output");
}

static void TestTimes(void)
{
    mlocTimeInput late = {.hour = 24};
    mlocTime time;
    mlocDateTime dateTime;
    mlocTimeDuration duration;
    mlocTimeDurationFields fields;
    mlocTimeDurationFields huge = {.seconds = INT64_MAX};
    mlocTimeDuration most = {.seconds = 9007199254740991};
    mlocTimeDuration bad = {.seconds = 1, .nanoseconds = -1};
    double total;
    int64_t days;
    Fill(&time, sizeof(time));
    Fill(&dateTime, sizeof(dateTime));
    Fill(&duration, sizeof(duration));
    Fill(&fields, sizeof(fields));
    Fill(&total, sizeof(total));
    Fill(&days, sizeof(days));
    mlocTimeRounding rounding = {mloc_timeUnitHour, mloc_timeUnitNanosecond, 7, 0};
    CHECK(mlocMakeTime(&late, mloc_overflowReject, &time) == mloc_errorInvalid &&
              mlocMakeDateTime(&s_dateTime.date, &time, &dateTime) == mloc_errorInvalid &&
              mlocMakeTimeDuration(&huge, &duration) == mloc_errorInvalid &&
              mlocBalanceTimeDuration(&bad, mloc_timeUnitHour, &fields) == mloc_errorInvalid &&
              mlocAddTimeDurations(&most, &most, &duration) == mloc_errorInvalid &&
              mlocRoundTimeDuration(&most, mloc_timeUnitHour, 0, mloc_roundHalfExpand, &duration) ==
                  mloc_errorInvalid &&
              mlocTotalTimeDuration(&bad, mloc_timeUnitHour, &total) == mloc_errorInvalid &&
              mlocAddToTime(&s_dateTime.time, &bad, &time, &days) == mloc_errorInvalid &&
              mlocTimeUntil(&s_dateTime.time, &s_dateTime.time, &rounding, &fields) ==
                  mloc_errorInvalid &&
              mlocRoundTime(&s_dateTime.time, mloc_timeUnitHour, 7, mloc_roundHalfExpand, &time,
                            &days) == mloc_errorInvalid &&
              Kept(&time, sizeof(time)) && Kept(&dateTime, sizeof(dateTime)) &&
              Kept(&duration, sizeof(duration)) && Kept(&fields, sizeof(fields)) &&
              Kept(&total, sizeof(total)) && Kept(&days, sizeof(days)),
          "time arithmetic refused leaves its outputs");
}

static void TestDateTimes(void)
{
    mlocDurationValues mixed = {.years = 1, .days = -1};
    mlocDateTimeDuration twoSigns = {.date = {.years = 1}, .time = {.seconds = -1}};
    mlocDateTimeDuration duration;
    mlocDurationValues values;
    mlocDateTime dateTime;
    double total;
    Fill(&duration, sizeof(duration));
    Fill(&values, sizeof(values));
    Fill(&dateTime, sizeof(dateTime));
    Fill(&total, sizeof(total));
    mlocDateTimeRounding rounding = {mloc_unitDay, mloc_unitYear, 1, 0};
    CHECK(mlocMakeDuration(&mixed, &duration) == mloc_errorInvalid &&
              mlocGetDurationValues(&twoSigns, mloc_unitDay, &values) == mloc_errorInvalid &&
              mlocAddToDateTime(&s_dateTime, &twoSigns, mloc_overflowConstrain, &dateTime) ==
                  mloc_errorInvalid &&
              mlocDateTimeUntil(&s_dateTime, &s_dateTime, &rounding, &duration) ==
                  mloc_errorInvalid &&
              mlocRoundDateTime(&s_dateTime, mloc_timeUnitHour, 7, mloc_roundHalfExpand,
                                &dateTime) == mloc_errorInvalid &&
              mlocDateTimeTotal(&s_dateTime, &s_dateTime, (mlocUnit)10, &total) ==
                  mloc_errorInvalid &&
              Kept(&duration, sizeof(duration)) && Kept(&values, sizeof(values)) &&
              Kept(&dateTime, sizeof(dateTime)) && Kept(&total, sizeof(total)),
          "date-time arithmetic refused leaves its outputs");
}

static void TestInstants(void)
{
    mlocTimeDuration most = {.seconds = 9007199254740991};
    mlocTimeRounding rounding = {mloc_timeUnitHour, mloc_timeUnitNanosecond, 7, 0};
    mlocInstant instant;
    mlocTimeDuration duration;
    mlocPossibleInstants possible;
    mlocDateTime dateTime;
    Fill(&instant, sizeof(instant));
    Fill(&duration, sizeof(duration));
    Fill(&possible, sizeof(possible));
    Fill(&dateTime, sizeof(dateTime));
    CHECK(mlocAddToInstant(&s_epoch, &most, &instant) == mloc_errorInvalid &&
              mlocInstantUntil(&s_epoch, &s_epoch, &rounding, &duration) == mloc_errorInvalid &&
              mlocRoundInstant(&s_epoch, mloc_timeUnitHour, 7, mloc_roundHalfExpand, &instant) ==
                  mloc_errorInvalid &&
              mlocGetPossibleInstants(&s_forged, &s_dateTime, &possible) == mloc_errorInvalid &&
              mlocGetInstantFor(&s_named, &s_dateTime, (mlocDisambiguation)4, &instant) ==
                  mloc_errorInvalid &&
              mlocGetStartOfDay(&s_forged, &s_dateTime.date, &instant) == mloc_errorInvalid &&
              mlocGetDateTimeFor(&s_named, &s_far, mloc_calendarGregorian, &dateTime) ==
                  mloc_errorInvalid &&
              mlocGetInstantForOffset(&s_named, &s_dateTime, 86400000000000, mloc_offsetUse,
                                      mloc_disambiguationCompatible, false,
                                      &instant) == mloc_errorInvalid &&
              Kept(&instant, sizeof(instant)) && Kept(&duration, sizeof(duration)) &&
              Kept(&possible, sizeof(possible)) && Kept(&dateTime, sizeof(dateTime)),
          "instant arithmetic refused leaves its outputs");
}

static void TestZoned(void)
{
    mlocDateTimeDuration twoSigns = {.date = {.years = 1}, .time = {.seconds = -1}};
    mlocDateTimeRounding rounding = {mloc_unitDay, mloc_unitYear, 1, 0};
    mlocInstant instant;
    mlocDateTimeDuration duration;
    double total;
    Fill(&instant, sizeof(instant));
    Fill(&duration, sizeof(duration));
    Fill(&total, sizeof(total));
    CHECK(mlocAddToZonedDateTime(&s_named, mloc_calendarGregorian, &s_epoch, &twoSigns,
                                 mloc_overflowConstrain, &instant) == mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&s_named, mloc_calendarGregorian, &s_epoch, &s_epoch,
                                     &rounding, &duration) == mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&s_named, mloc_calendarGregorian, &s_epoch, &s_epoch,
                                     (mlocUnit)10, &total) == mloc_errorInvalid &&
              mlocRoundZonedDateTime(&s_named, &s_epoch, mloc_timeUnitHour, 7, mloc_roundHalfExpand,
                                     &instant) == mloc_errorInvalid &&
              Kept(&instant, sizeof(instant)) && Kept(&duration, sizeof(duration)) &&
              Kept(&total, sizeof(total)),
          "zoned arithmetic refused leaves its outputs");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    s_empty = (mlocDataSet){nullptr, 0};
    mlocDate date;
    mlocTimeInput noon = {.hour = 12};
    CHECK(mlocFindTimeZone(&s_data, "+01:00", 6, &s_offset) == mloc_success &&
              mlocFindTimeZone(&s_data, "Europe/Paris", 12, &s_named) == mloc_success &&
              mlocMakeDateFromDay(mloc_calendarGregorian, 739000, &date) == mloc_success &&
              mlocMakeTime(&noon, mloc_overflowReject, &s_dateTime.time) == mloc_success &&
              mlocMakeDateTime(&date, &s_dateTime.time, &s_dateTime) == mloc_success,
          "zones and a date-time to work with");
    s_forged = s_named;
    s_forged.identifier = UINT16_MAX;
    TestParse();
    TestFormat();
    TestZones();
    TestTimes();
    TestDateTimes();
    TestInstants();
    TestZoned();
    return s_failures == 0 ? 0 : 1;
}
