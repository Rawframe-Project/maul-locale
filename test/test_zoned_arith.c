// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic of zoned date-times through the public functions, on
// Berlin's spring change of 2024 (a day of 23 hours from March 31) and its
// autumn fold, and Monrovia's offset of -00:44:30 in 1970: days added as
// calendar days and hours as exact time, differences whose rounding counts
// the zone's day, rounding to a day of 23 hours and within a fold, offsets
// weighed against the zone, every refusal, and the refusal of
// differences across the whole day Apia skipped and the one Sitka lived
// twice.

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <string.h>

static mlocData s_builtin;
static mlocDataSet s_data;

static const int64_t s_hour = 3600;
// 2024-03-30T12:00+01:00 in Berlin, a day before its clocks go forward.
static const int64_t s_springNoon = INT64_C(1711796400);
// 2024-10-27T02:30 in Berlin, under +01:00 (the fold's later instant).
static const int64_t s_foldLater = INT64_C(1729992600);

static mlocTimeZone Zone(const char* name)
{
    mlocTimeZone zone = {0};
    CHECK(mlocFindTimeZone(&s_data, name, strlen(name), &zone) == mloc_success, "a zone");
    return zone;
}

static void TestAdd(void)
{
    mlocTimeZone berlin = Zone("Europe/Berlin");
    mlocInstant noon = {s_springNoon, 0};
    mlocInstant out = {0};
    mlocDateTimeDuration day = {{0, 0, 0, 1}, {0, 0}};
    mlocDateTimeDuration hours = {{0}, {24 * s_hour, 0}};
    CHECK(mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &noon, &day, mloc_overflowReject,
                                 &out) == mloc_success &&
              out.seconds == s_springNoon + 23 * s_hour && out.nanoseconds == 0,
          "a calendar day across the change is 23 hours");
    CHECK(mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &noon, &hours,
                                 mloc_overflowReject, &out) == mloc_success &&
              out.seconds == s_springNoon + 24 * s_hour,
          "24 hours are exact time");
    mlocDateTimeDuration mixed = {{0, 0, 0, 1}, {-1, 0}};
    mlocDateTimeDuration days = {{0, 0, 0, INT64_C(104249991375)}, {0, 0}};
    mlocTimeZone forged = {.offsetMinutes = 1440};
    mlocInstant past = {INT64_C(8640000000000), 1};
    CHECK(mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &noon, &mixed,
                                 mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &noon, &days,
                                     mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToZonedDateTime(&forged, mloc_calendarGregorian, &noon, &hours,
                                     mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &past, &hours,
                                     mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, nullptr, &day,
                                     mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &noon, nullptr,
                                     mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &noon, &day,
                                     mloc_overflowReject, nullptr) == mloc_errorInvalid,
          "two signs, too many days, a zone or instant the library would not make, nulls");
    mlocInstant last = {INT64_C(8640000000000), 0};
    CHECK(mlocAddToZonedDateTime(&berlin, mloc_calendarGregorian, &last, &day, mloc_overflowReject,
                                 &out) == mloc_errorInvalid,
          "past the range");
}

static void TestUntil(void)
{
    mlocTimeZone berlin = Zone("Europe/Berlin");
    mlocInstant noon = {s_springNoon, 0};
    mlocInstant next = {s_springNoon + 23 * s_hour, 0};
    mlocDateTimeDuration d = {0};
    mlocDateTimeRounding r = {mloc_unitDay, mloc_unitNanosecond, 1, mloc_roundTrunc};
    CHECK(mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &next, &r, &d) ==
                  mloc_success &&
              d.date.days == 1 && d.time.seconds == 0,
          "noon to noon is a day");
    r.largestUnit = mloc_unitHour;
    CHECK(mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &next, &r, &d) ==
                  mloc_success &&
              d.date.days == 0 && d.time.seconds == 23 * s_hour,
          "or 23 hours");
    // 11 hours 45 minutes of a day of 23 hours: past half of it.
    mlocInstant late = {s_springNoon + 11 * s_hour + 2700, 0};
    r = (mlocDateTimeRounding){mloc_unitDay, mloc_unitDay, 1, mloc_roundHalfExpand};
    CHECK(mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &late, &r, &d) ==
                  mloc_success &&
              d.date.days == 1 && d.time.seconds == 0,
          "a day as long as the zone makes it");
    // 22 hours 40 minutes rounds to 23 hours, the whole day.
    mlocInstant before = {s_springNoon + 22 * s_hour + 2400, 0};
    r = (mlocDateTimeRounding){mloc_unitDay, mloc_unitHour, 1, mloc_roundHalfExpand};
    CHECK(mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &before, &r, &d) ==
                  mloc_success &&
              d.date.days == 1 && d.time.seconds == 0,
          "hours rounded past the day's end become a day");
    CHECK(mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &noon, &r, &d) ==
                  mloc_success &&
              d.date.days == 0 && d.time.seconds == 0 && d.time.nanoseconds == 0,
          "equal instants");
    static const mlocDateTimeRounding s_refused[] = {
        {mloc_unitHour, mloc_unitHour, 24, mloc_roundTrunc},
        {mloc_unitDay, mloc_unitDay, 0, mloc_roundTrunc},
        {mloc_unitSecond, mloc_unitDay, 1, mloc_roundTrunc},
        {mloc_unitYear, (mlocUnit)10, 1, mloc_roundTrunc},
        {mloc_unitYear, mloc_unitDay, 1, (mlocRoundingMode)9},
    };
    for (size_t i = 0; i < sizeof s_refused / sizeof s_refused[0]; i++)
    {
        CHECK(mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &next, &s_refused[i],
                                     &d) == mloc_errorInvalid,
              "a unit, increment or mode refused");
    }
    mlocTimeZone forged = {.offsetMinutes = 1440};
    mlocInstant past = {INT64_C(8640000000000), 1};
    r = (mlocDateTimeRounding){mloc_unitDay, mloc_unitSecond, 1, mloc_roundTrunc};
    CHECK(mlocZonedDateTimeUntil(&forged, mloc_calendarGregorian, &noon, &next, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &past, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &past, &noon, &r, &d) ==
                  mloc_errorInvalid,
          "a zone or instant the library would not make, with days");
    r = (mlocDateTimeRounding){mloc_unitHour, mloc_unitSecond, 1, mloc_roundTrunc};
    CHECK(mlocZonedDateTimeUntil(&forged, mloc_calendarGregorian, &noon, &next, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &past, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, (mlocCalendar)200, &noon, &next, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, nullptr, &next, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, nullptr, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &next, nullptr, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&berlin, mloc_calendarGregorian, &noon, &next, &r, nullptr) ==
                  mloc_errorInvalid,
          "a zone, instant or calendar the library would not make, nulls, with hours too");
    // NudgeToZonedTime finds the instant a day after the start's date at
    // the start's time; from the last hour of the range that is past it.
    mlocTimeZone utc = {0};
    mlocInstant last = {INT64_C(8640000000000), 0};
    mlocInstant hourBefore = {INT64_C(8640000000000) - 3600, 0};
    r = (mlocDateTimeRounding){mloc_unitDay, mloc_unitHour, 1, mloc_roundHalfExpand};
    CHECK(mlocZonedDateTimeUntil(&utc, mloc_calendarIso8601, &hourBefore, &last, &r, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeUntil(&utc, mloc_calendarIso8601, &last, &last, &r, &d) ==
                  mloc_errorInvalid,
          "a rounding whose next day passes the range");
}

static void TestRound(void)
{
    mlocTimeZone berlin = Zone("Europe/Berlin");
    // 13:00 on the day of 23 hours is 12 hours in: past its half.
    mlocInstant afternoon = {INT64_C(1711882800), 0};
    mlocInstant out = {0};
    CHECK(mlocRoundZonedDateTime(&berlin, &afternoon, mloc_timeUnitDay, 1, mloc_roundHalfTrunc,
                                 &out) == mloc_success &&
              out.seconds == INT64_C(1711922400),
          "to the next day's start");
    CHECK(mlocRoundZonedDateTime(&berlin, &afternoon, mloc_timeUnitDay, 1, mloc_roundFloor, &out) ==
                  mloc_success &&
              out.seconds == INT64_C(1711839600),
          "to the day's start");
    mlocInstant fold = {s_foldLater, 0};
    CHECK(mlocRoundZonedDateTime(&berlin, &fold, mloc_timeUnitHour, 1, mloc_roundFloor, &out) ==
                  mloc_success &&
              out.seconds == INT64_C(1729990800),
          "within a fold, the offset kept");
    mlocInstant last = {INT64_C(8640000000000), 0};
    mlocTimeZone forged = {.offsetMinutes = 1440};
    CHECK(mlocRoundZonedDateTime(&berlin, &last, mloc_timeUnitDay, 1, mloc_roundFloor, &out) ==
                  mloc_errorInvalid &&
              mlocRoundZonedDateTime(&berlin, &fold, mloc_timeUnitDay, 2, mloc_roundFloor, &out) ==
                  mloc_errorInvalid &&
              mlocRoundZonedDateTime(&berlin, &fold, mloc_timeUnitHour, 5, mloc_roundFloor, &out) ==
                  mloc_errorInvalid &&
              mlocRoundZonedDateTime(&berlin, &fold, mloc_timeUnitHour, 1, (mlocRoundingMode)9,
                                     &out) == mloc_errorInvalid &&
              mlocRoundZonedDateTime(&forged, &fold, mloc_timeUnitHour, 1, mloc_roundFloor, &out) ==
                  mloc_errorInvalid &&
              mlocRoundZonedDateTime(&berlin, nullptr, mloc_timeUnitHour, 1, mloc_roundFloor,
                                     &out) == mloc_errorInvalid &&
              mlocRoundZonedDateTime(&berlin, &fold, mloc_timeUnitHour, 1, mloc_roundFloor,
                                     nullptr) == mloc_errorInvalid,
          "the last day, an increment, mode or zone refused, nulls");
}

static mlocDateTime LocalSecond(int32_t day, uint8_t hour, uint8_t minute, uint8_t second)
{
    mlocDate date = {0};
    mlocTime time = {hour, minute, second, 0};
    mlocDateTime out = {0};
    CHECK(mlocMakeDateFromDay(mloc_calendarGregorian, day, &date) == mloc_success &&
              mlocMakeDateTime(&date, &time, &out) == mloc_success,
          "a date-time");
    return out;
}

static mlocDateTime Local(int32_t day, uint8_t hour, uint8_t minute)
{
    return LocalSecond(day, hour, minute, 0);
}

// The first day of Temporal's range, and N'Djamena's fold of 12 seconds
// in 1912 (+01:00:12 to +01:00), whose two offsets round to one minute.
static void TestOffsetEdges(void)
{
    mlocTimeZone utc = Zone("UTC");
    const int64_t h = INT64_C(3600000000000);
    const int64_t first = -INT64_C(8640000000000);
    mlocDateTime noon = Local(719163 - 100000000, 12, 0);
    mlocInstant out = {0};
    CHECK(mlocGetInstantForOffset(&utc, &noon, h, mloc_offsetUse, mloc_disambiguationCompatible,
                                  false, &out) == mloc_success &&
              out.seconds == first + 43200 - 3600,
          "an offset used on the first day");
    CHECK(mlocGetInstantForOffset(&utc, &noon, 0, mloc_offsetReject, mloc_disambiguationCompatible,
                                  false, &out) == mloc_success &&
              out.seconds == first + 43200,
          "an offset matched on the first day");
    mlocTimeZone berlin = Zone("Europe/Berlin");
    mlocDateTime october = Local(739176, 12, 0);
    CHECK(mlocGetInstantForOffset(&berlin, &october, 2 * h, mloc_offsetReject,
                                  mloc_disambiguationReject, false, &out) == mloc_success &&
              out.seconds == INT64_C(1729159200),
          "the one instant matched");
    mlocTimeZone ndjamena = Zone("Africa/Ndjamena");
    mlocDateTime fold = LocalSecond(697977, 23, 59, 54);
    CHECK(mlocGetInstantForOffset(&ndjamena, &fold, h, mloc_offsetReject,
                                  mloc_disambiguationCompatible, true, &out) == mloc_success &&
              out.seconds == -INT64_C(1830387618),
          "minutes matching both, the earlier");
    CHECK(mlocGetInstantForOffset(&ndjamena, &fold, h, mloc_offsetReject,
                                  mloc_disambiguationCompatible, false, &out) == mloc_success &&
              out.seconds == -INT64_C(1830387606),
          "exactly, the later");
}

static void TestOffset(void)
{
    mlocTimeZone berlin = Zone("Europe/Berlin");
    mlocDateTime fold = Local(739186, 2, 30);
    mlocInstant out = {0};
    const int64_t h = INT64_C(3600000000000);
    CHECK(mlocGetInstantForOffset(&berlin, &fold, 2 * h, mloc_offsetPrefer,
                                  mloc_disambiguationLater, false, &out) == mloc_success &&
              out.seconds == INT64_C(1729989000),
          "the earlier instant by its offset");
    CHECK(mlocGetInstantForOffset(&berlin, &fold, h, mloc_offsetReject, mloc_disambiguationEarlier,
                                  false, &out) == mloc_success &&
              out.seconds == s_foldLater,
          "the later instant by its offset");
    CHECK(mlocGetInstantForOffset(&berlin, &fold, 3 * h, mloc_offsetReject,
                                  mloc_disambiguationCompatible, false, &out) == mloc_errorInvalid,
          "an offset the zone does not give, refused");
    CHECK(mlocGetInstantForOffset(&berlin, &fold, 3 * h, mloc_offsetPrefer,
                                  mloc_disambiguationLater, false, &out) == mloc_success &&
              out.seconds == s_foldLater,
          "or left to the disambiguation");
    CHECK(mlocGetInstantForOffset(&berlin, &fold, 3 * h, mloc_offsetUse, mloc_disambiguationReject,
                                  false, &out) == mloc_success &&
              out.seconds == INT64_C(1729985400),
          "or used alone");
    CHECK(mlocGetInstantForOffset(&berlin, &fold, 3 * h, mloc_offsetIgnore,
                                  mloc_disambiguationReject, false, &out) == mloc_errorInvalid,
          "or ignored, the fold refused");
    mlocTimeZone monrovia = Zone("Africa/Monrovia");
    mlocDateTime epoch = Local(719163, 0, 0);
    const int64_t m = INT64_C(60000000000);
    CHECK(mlocGetInstantForOffset(&monrovia, &epoch, -45 * m, mloc_offsetReject,
                                  mloc_disambiguationCompatible, false, &out) == mloc_errorInvalid,
          "-00:44:30 is not -00:45");
    CHECK(mlocGetInstantForOffset(&monrovia, &epoch, -45 * m, mloc_offsetReject,
                                  mloc_disambiguationCompatible, true, &out) == mloc_success &&
              out.seconds == 2670,
          "unless minutes match");
    const int64_t day = 24 * h;
    mlocTimeZone forged = {.offsetMinutes = 1440};
    CHECK(mlocGetInstantForOffset(&berlin, &fold, day, mloc_offsetUse,
                                  mloc_disambiguationCompatible, false,
                                  &out) == mloc_errorInvalid &&
              mlocGetInstantForOffset(&berlin, &fold, -day, mloc_offsetUse,
                                      mloc_disambiguationCompatible, false,
                                      &out) == mloc_errorInvalid &&
              mlocGetInstantForOffset(&berlin, &fold, 0, (mlocOffsetOption)4,
                                      mloc_disambiguationCompatible, false,
                                      &out) == mloc_errorInvalid &&
              mlocGetInstantForOffset(&berlin, &fold, 0, mloc_offsetUse, (mlocDisambiguation)4,
                                      false, &out) == mloc_errorInvalid &&
              mlocGetInstantForOffset(&forged, &fold, 0, mloc_offsetUse,
                                      mloc_disambiguationCompatible, false,
                                      &out) == mloc_errorInvalid &&
              mlocGetInstantForOffset(&berlin, nullptr, 0, mloc_offsetUse,
                                      mloc_disambiguationCompatible, false,
                                      &out) == mloc_errorInvalid &&
              mlocGetInstantForOffset(&berlin, &fold, 0, mloc_offsetUse,
                                      mloc_disambiguationCompatible, false,
                                      nullptr) == mloc_errorInvalid,
          "an offset of a day, an option, disambiguation or zone refused, nulls");
    mlocTimeZone utc = Zone("UTC");
    mlocDateTime first = Local(719163 - 100000000, 0, 30);
    CHECK(mlocGetInstantForOffset(&utc, &first, h, mloc_offsetUse, mloc_disambiguationCompatible,
                                  false, &out) == mloc_errorInvalid,
          "an offset taking the first day's date-time out of the range");
}

// Differences rounded or totaled across a jump of a whole wall-clock
// day, where ECMA-262 asserts that the windows of NudgeToCalendarUnit
// and the day of NudgeToZonedTime last a while and hold the destination,
// and gives no answer when they do not: each is refused.
static void TestDayJumps(void)
{
    // Pacific/Apia skipped 30 December 2011; from 11:59:59 to 10:59:59 UTC
    // that day.
    mlocTimeZone apia = Zone("Pacific/Apia");
    mlocInstant apiaOne = {INT64_C(1325246399), 0};
    mlocInstant apiaTwo = {INT64_C(1325242799), 0};
    // America/Sitka went from +14:58:47 to -09:01:13 in October 1867;
    // 1867-10-16T01:01:13Z to 1867-10-19T00:31:13Z.
    mlocTimeZone sitka = Zone("America/Sitka");
    mlocInstant sitkaOne = {INT64_C(-3225481127), 0};
    mlocInstant sitkaTwo = {INT64_C(-3225223727), 0};
    mlocDateTimeRounding days = {mloc_unitDay, mloc_unitDay, 1, mloc_roundHalfExpand};
    mlocDateTimeDuration d = {0};
    double total = 0;
    CHECK(mlocZonedDateTimeUntil(&apia, mloc_calendarGregorian, &apiaOne, &apiaTwo, &days, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&apia, mloc_calendarGregorian, &apiaOne, &apiaTwo,
                                     mloc_unitDay, &total) == mloc_errorInvalid,
          "days across a skipped day: an empty window");
    CHECK(mlocZonedDateTimeUntil(&sitka, mloc_calendarGregorian, &sitkaOne, &sitkaTwo, &days, &d) ==
                  mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&sitka, mloc_calendarGregorian, &sitkaOne, &sitkaTwo,
                                     mloc_unitDay, &total) == mloc_errorInvalid,
          "days across a repeated day: a shifted window that misses the end");
    // 2012-01-01T12:00+14:00 in Apia, and a day back from it, rounded to
    // hours: the day before the end has no length.
    mlocInstant noon = {INT64_C(1325368800), 0};
    mlocInstant back = {0};
    mlocDateTimeDuration dayBack = {{0, 0, 0, -1}, {0, 0}};
    mlocDateTimeRounding hours = {mloc_unitDay, mloc_unitHour, 1, mloc_roundHalfExpand};
    CHECK(mlocAddToZonedDateTime(&apia, mloc_calendarGregorian, &noon, &dayBack,
                                 mloc_overflowConstrain, &back) == mloc_success &&
              mlocZonedDateTimeUntil(&apia, mloc_calendarGregorian, &noon, &back, &hours, &d) ==
                  mloc_errorInvalid,
          "hours across a skipped day: a day of no length");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    TestAdd();
    TestUntil();
    TestRound();
    TestOffset();
    TestOffsetEdges();
    TestDayJumps();
    return s_failures == 0 ? 0 : 1;
}
