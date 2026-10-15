// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic of date-times through the public functions (record
// mloc-0041): adding across days and into a date's overflow, the
// limits at both ends of the range, differences with each kind of
// largest unit, rounding by sub-second steps whose half-even parity
// takes the seconds, and every refusal.

#include "test_harness.h"

#include "maul-locale/zone.h"

static mlocDateTime At(mlocCalendar calendar, int32_t year, uint32_t month, uint32_t day,
                       uint8_t hour, uint8_t minute, uint8_t second, uint32_t nanosecond)
{
    mlocDateInput input = {.year = year, .month = month, .day = day};
    mlocDate iso = {0};
    mlocDate date = {0};
    mlocTime time = {hour, minute, second, 0, nanosecond};
    mlocDateTime out = {0};
    CHECK(mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &iso) == mloc_success &&
              mlocMakeDateFromDay(calendar, iso.day, &date) == mloc_success &&
              mlocMakeDateTime(&date, &time, &out) == mloc_success,
          "a date-time");
    return out;
}

static bool Is(const mlocDateTime* d, const mlocDateTime* want)
{
    return d->date.day == want->date.day && d->date.calendar == want->date.calendar &&
           d->time.hour == want->time.hour && d->time.minute == want->time.minute &&
           d->time.second == want->time.second && d->time.nanosecond == want->time.nanosecond;
}

static void TestAdd(void)
{
    mlocDateTime start = At(mloc_calendarGregorian, 2020, 1, 31, 23, 0, 0, 0);
    mlocDateTime out = {0};
    mlocDateTimeDuration d = {{0, 1, 0, 0}, {7200, 0}};
    mlocDateTime want = At(mloc_calendarGregorian, 2020, 3, 1, 1, 0, 0, 0);
    CHECK(mlocAddToDateTime(&start, &d, mloc_overflowConstrain, &out) == mloc_success &&
              Is(&out, &want),
          "a month to the 29th, then the day the time passed");
    CHECK(mlocAddToDateTime(&start, &d, mloc_overflowReject, &out) == mloc_errorInvalid,
          "February 31 refused, the days added after");
    mlocDateTimeDuration back = {{0, 0, 0, -1}, {-1, -1}};
    want = At(mloc_calendarGregorian, 2020, 1, 30, 22, 59, 58, 999999999);
    CHECK(mlocAddToDateTime(&start, &back, mloc_overflowReject, &out) == mloc_success &&
              Is(&out, &want),
          "back a day and a second and a nanosecond");
    mlocDateTimeDuration mixed = {{0, 0, 0, 1}, {-1, 0}};
    mlocDateTimeDuration days = {{0, 0, 0, INT64_C(104249991375)}, {0, 0}};
    mlocDateTimeDuration wide = {{0, 0, 0, 0}, {0, 1000000000}};
    CHECK(mlocAddToDateTime(&start, &mixed, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDateTime(&start, &days, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDateTime(&start, &wide, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDateTime(nullptr, &d, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDateTime(&start, nullptr, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDateTime(&start, &d, mloc_overflowReject, nullptr) == mloc_errorInvalid,
          "two signs, too many days, a duration the library would not make, null pointers");
    mlocDateTime first = At(mloc_calendarGregorian, -271821, 4, 19, 0, 0, 0, 1);
    mlocDateTimeDuration nano = {{0}, {0, -1}};
    CHECK(mlocAddToDateTime(&first, &nano, mloc_overflowReject, &out) == mloc_errorInvalid,
          "to the first day's midnight, out of the limits");
}

static void TestUntil(void)
{
    mlocDateTime one = At(mloc_calendarGregorian, 2019, 1, 31, 12, 0, 0, 0);
    mlocDateTime two = At(mloc_calendarGregorian, 2019, 3, 1, 6, 30, 0, 0);
    mlocDateTimeDuration d = {0};
    mlocDateTimeRounding r = {mloc_unitMonth, mloc_unitNanosecond, 1, mloc_roundTrunc};
    CHECK(mlocDateTimeUntil(&one, &two, &r, &d) == mloc_success && d.date.months == 0 &&
              d.date.days == 28 && d.time.seconds == 18 * 3600 + 1800,
          "the day before when the times run back");
    r = (mlocDateTimeRounding){mloc_unitHour, mloc_unitNanosecond, 1, mloc_roundTrunc};
    CHECK(mlocDateTimeUntil(&one, &two, &r, &d) == mloc_success && d.date.days == 0 &&
              d.time.seconds == (28 * 24 + 18) * 3600 + 1800,
          "hours fold the days in");
    r = (mlocDateTimeRounding){mloc_unitMonth, mloc_unitMonth, 1, mloc_roundHalfExpand};
    CHECK(mlocDateTimeUntil(&one, &two, &r, &d) == mloc_success && d.date.months == 1 &&
              d.date.days == 0 && d.time.seconds == 0,
          "rounded to a month");
    mlocDateTime a = At(mloc_calendarGregorian, 2000, 1, 1, 0, 0, 0, 0);
    mlocDateTime b = At(mloc_calendarGregorian, 2000, 1, 1, 0, 0, 1, 300000000);
    r = (mlocDateTimeRounding){mloc_unitSecond, mloc_unitMillisecond, 200, mloc_roundHalfEven};
    CHECK(mlocDateTimeUntil(&a, &b, &r, &d) == mloc_success && d.time.seconds == 1 &&
              d.time.nanoseconds == 200000000,
          "1.3 s to 200 ms steps, half to the even sixth");
    CHECK(mlocDateTimeUntil(&a, &a, &r, &d) == mloc_success && d.time.seconds == 0 &&
              d.date.days == 0,
          "equal date-times");
    mlocDateTime hebrew = At(mloc_calendarHebrew, 2000, 1, 1, 0, 0, 0, 0);
    static const mlocDateTimeRounding s_refused[] = {
        {mloc_unitHour, mloc_unitHour, 5, mloc_roundTrunc},
        {mloc_unitHour, mloc_unitHour, 24, mloc_roundTrunc},
        {mloc_unitDay, mloc_unitDay, 1000000001, mloc_roundTrunc},
        {mloc_unitDay, mloc_unitDay, 0, mloc_roundTrunc},
        {mloc_unitSecond, mloc_unitDay, 1, mloc_roundTrunc},
        {mloc_unitYear, (mlocUnit)10, 1, mloc_roundTrunc},
        {mloc_unitYear, mloc_unitDay, 1, (mlocRoundingMode)9},
    };
    for (size_t i = 0; i < sizeof s_refused / sizeof s_refused[0]; i++)
    {
        CHECK(mlocDateTimeUntil(&a, &b, &s_refused[i], &d) == mloc_errorInvalid,
              "a unit, increment or mode refused");
    }
    r = (mlocDateTimeRounding){mloc_unitDay, mloc_unitDay, 1000000000, mloc_roundTrunc};
    CHECK(mlocDateTimeUntil(&a, &b, &r, &d) == mloc_success &&
              mlocDateTimeUntil(&a, &hebrew, &r, &d) == mloc_errorInvalid &&
              mlocDateTimeUntil(nullptr, &b, &r, &d) == mloc_errorInvalid &&
              mlocDateTimeUntil(&a, nullptr, &r, &d) == mloc_errorInvalid &&
              mlocDateTimeUntil(&a, &b, nullptr, &d) == mloc_errorInvalid &&
              mlocDateTimeUntil(&a, &b, &r, nullptr) == mloc_errorInvalid,
          "10^9 days; two calendars, null pointers");
}

static void TestRound(void)
{
    mlocDateTime late = At(mloc_calendarGregorian, 275760, 9, 13, 23, 59, 59, 900000000);
    mlocDateTime early = At(mloc_calendarGregorian, -271821, 4, 19, 0, 0, 0, 500000000);
    mlocDateTime out = {0};
    CHECK(mlocRoundDateTime(&late, mloc_timeUnitSecond, 1, mloc_roundFloor, &out) == mloc_success &&
              out.time.second == 59 && out.time.nanosecond == 0,
          "the last day, down");
    CHECK(mlocRoundDateTime(&late, mloc_timeUnitSecond, 1, mloc_roundCeil, &out) ==
              mloc_errorInvalid,
          "past the last day");
    CHECK(mlocRoundDateTime(&early, mloc_timeUnitSecond, 1, mloc_roundFloor, &out) ==
              mloc_errorInvalid,
          "down to the first day's midnight, out of the limits");
    mlocDateTime noon = At(mloc_calendarJapanese, 2020, 2, 28, 12, 0, 0, 0);
    mlocDateTime want = At(mloc_calendarJapanese, 2020, 2, 29, 0, 0, 0, 0);
    CHECK(mlocRoundDateTime(&noon, mloc_timeUnitDay, 1, mloc_roundHalfExpand, &out) ==
                  mloc_success &&
              Is(&out, &want),
          "noon to the next day, in its calendar");
    CHECK(mlocRoundDateTime(&noon, mloc_timeUnitDay, 2, mloc_roundHalfExpand, &out) ==
                  mloc_errorInvalid &&
              mlocRoundDateTime(nullptr, mloc_timeUnitDay, 1, mloc_roundHalfExpand, &out) ==
                  mloc_errorInvalid &&
              mlocRoundDateTime(&noon, mloc_timeUnitDay, 1, mloc_roundHalfExpand, nullptr) ==
                  mloc_errorInvalid,
          "an increment refused, null pointers");
}

int main(void)
{
    TestAdd();
    TestUntil();
    TestRound();
    return s_failures == 0 ? 0 : 1;
}
