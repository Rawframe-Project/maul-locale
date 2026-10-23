// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Durations through the public functions: fields of float64 integers made
// into internal records and given back, exactly at 2^53 seconds; time
// durations added, rounded and totalled; totals from plain date-times and
// zoned date-times, each the float64 nearest the exact quotient; and every
// refusal.

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/zone.h"

#include <string.h>

static const int64_t s_maxSeconds = INT64_C(9007199254740992);

static double DoubleOf(uint64_t bits)
{
    double out;
    memcpy(&out, &bits, sizeof out);
    return out;
}

static uint64_t BitsOf(double value)
{
    uint64_t out;
    memcpy(&out, &value, sizeof out);
    return out;
}

static void TestMakeDuration(void)
{
    mlocDurationValues v = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    mlocDateTimeDuration d = {0};
    CHECK(mlocMakeDuration(&v, &d) == mloc_success && d.date.years == 1 && d.date.months == 2 &&
              d.date.weeks == 3 && d.date.days == 4 && d.time.seconds == 5 * 3600 + 6 * 60 + 7 &&
              d.time.nanoseconds == 8009010,
          "fields into a record, the days kept apart");
    // 2^63 nanoseconds, past int64_t but exact as a double.
    mlocDurationValues big = {.nanoseconds = 9223372036854775808.0};
    CHECK(mlocMakeDuration(&big, &d) == mloc_success && d.time.seconds == INT64_C(9223372036) &&
              d.time.nanoseconds == 854775808,
          "nanoseconds past int64_t, exactly");
    mlocDurationValues negative = {.days = -1, .nanoseconds = -1};
    CHECK(mlocMakeDuration(&negative, &d) == mloc_success && d.date.days == -1 &&
              d.time.seconds == 0 && d.time.nanoseconds == -1,
          "negative");
    mlocDurationValues zeros = {.years = -0.0, .days = 1};
    CHECK(mlocMakeDuration(&zeros, &d) == mloc_success && d.date.days == 1 && d.time.seconds == 0 &&
              d.time.nanoseconds == 0,
          "-0 is no sign; whole days leave no time");
    // Only years, months and weeks stop at 2^32; days stop with the time
    // at 2^53 seconds.
    mlocDurationValues manyDays = {.days = 4294967296.0, .hours = 1};
    CHECK(mlocMakeDuration(&manyDays, &d) == mloc_success && d.date.days == INT64_C(4294967296) &&
              d.time.seconds == 3600,
          "days past 2^32");
    // 2^53 seconds less a nanosecond, and 2^53 seconds.
    mlocDurationValues last = {.seconds = 9007199254740991.0,
                               .milliseconds = 999,
                               .microseconds = 999,
                               .nanoseconds = 999};
    mlocDurationValues over = {.seconds = 9007199254740991.0, .milliseconds = 1000};
    CHECK(mlocMakeDuration(&last, &d) == mloc_success && d.time.seconds == s_maxSeconds - 1 &&
              d.time.nanoseconds == 999999999 && mlocMakeDuration(&over, &d) == mloc_errorInvalid,
          "the limit of 2^53 seconds");
    const mlocDurationValues refused[] = {
        {.years = 0.5},           {.days = 1, .hours = -1}, {.years = 4294967296.0},
        {.weeks = -4294967296.0}, {.days = 104249991375.0}, {.nanoseconds = 1e30},
        {.microseconds = 1e300},
    };
    mlocDurationValues nan = {.hours = DoubleOf(UINT64_C(0x7FF8000000000000))};
    mlocDurationValues infinite = {.minutes = DoubleOf(UINT64_C(0xFFF0000000000000))};
    mlocDurationValues tiny = {.seconds = DoubleOf(1)};
    CHECK(mlocMakeDuration(&nan, &d) == mloc_errorInvalid &&
              mlocMakeDuration(&infinite, &d) == mloc_errorInvalid &&
              mlocMakeDuration(&tiny, &d) == mloc_errorInvalid,
          "NaN, infinity, a subnormal");
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++)
    {
        CHECK(mlocMakeDuration(&refused[i], &d) == mloc_errorInvalid,
              "a fraction, two signs, or past the limits");
    }
    CHECK(mlocMakeDuration(nullptr, &d) == mloc_errorInvalid &&
              mlocMakeDuration(&v, nullptr) == mloc_errorInvalid,
          "null pointers");
}

static void TestGetDurationValues(void)
{
    mlocDateTimeDuration d = {{0, 1, 0, 2}, {90061, 1001001}};
    mlocDurationValues v = {0};
    CHECK(mlocGetDurationValues(&d, mloc_unitDay, &v) == mloc_success && v.months == 1 &&
              v.days == 3 && v.hours == 1 && v.minutes == 1 && v.seconds == 1 &&
              v.milliseconds == 1 && v.microseconds == 1 && v.nanoseconds == 1,
          "balanced into days for a date unit");
    CHECK(mlocGetDurationValues(&d, mloc_unitMinute, &v) == mloc_success && v.days == 2 &&
              v.hours == 0 && v.minutes == 1501 && v.seconds == 1,
          "up to minutes");
    // 2^53 seconds less one, as microseconds: 9007199254740991000000 is no
    // float64; the nearest is.
    mlocDateTimeDuration longest = {{0}, {s_maxSeconds - 1, 0}};
    CHECK(mlocGetDurationValues(&longest, mloc_unitMicrosecond, &v) == mloc_success &&
              v.microseconds == 9007199254740991000000.0,
          "microseconds as the nearest float64");
    // 2^53 - 1 seconds and 975,424,000 nanoseconds: as milliseconds the
    // float64 rounds past 2^53 seconds.
    mlocDateTimeDuration edge = {{0}, {s_maxSeconds - 1, 975424000}};
    CHECK(mlocGetDurationValues(&edge, mloc_unitSecond, &v) == mloc_success &&
              mlocGetDurationValues(&edge, mloc_unitMillisecond, &v) == mloc_errorInvalid,
          "refused when a field's float64 passes the limit");
    mlocDateTimeDuration mixed = {{0, 0, 0, 1}, {-1, 0}};
    mlocDateTimeDuration years = {{INT64_C(4294967296), 0, 0, 0}, {0, 0}};
    mlocDateTimeDuration forged = {{0}, {0, 1000000000}};
    CHECK(mlocGetDurationValues(&mixed, mloc_unitDay, &v) == mloc_errorInvalid &&
              mlocGetDurationValues(&years, mloc_unitDay, &v) == mloc_errorInvalid &&
              mlocGetDurationValues(&forged, mloc_unitDay, &v) == mloc_errorInvalid &&
              mlocGetDurationValues(&d, (mlocUnit)10, &v) == mloc_errorInvalid &&
              mlocGetDurationValues(nullptr, mloc_unitDay, &v) == mloc_errorInvalid &&
              mlocGetDurationValues(&d, mloc_unitDay, nullptr) == mloc_errorInvalid,
          "two signs, years past 2^32, a time part the library would not make, nulls");
}

static void TestTimeDurations(void)
{
    mlocTimeDuration a = {s_maxSeconds - 2, 500000000};
    mlocTimeDuration b = {1, 499999999};
    mlocTimeDuration c = {-3, -1};
    mlocTimeDuration out = {0};
    CHECK(mlocAddTimeDurations(&a, &b, &out) == mloc_success && out.seconds == s_maxSeconds - 1 &&
              out.nanoseconds == 999999999,
          "to the last nanosecond");
    b.nanoseconds = 500000000;
    CHECK(mlocAddTimeDurations(&a, &b, &out) == mloc_errorInvalid, "past it");
    CHECK(mlocAddTimeDurations(&a, &c, &out) == mloc_success && out.seconds == s_maxSeconds - 5 &&
              out.nanoseconds == 499999999,
          "signs mixed into one");
    mlocTimeDuration day = {86400 + 43200, 0};
    CHECK(mlocRoundTimeDuration(&day, mloc_timeUnitDay, 1, mloc_roundHalfEven, &out) ==
                  mloc_success &&
              out.seconds == 2 * 86400,
          "a day and a half to two days");
    CHECK(mlocRoundTimeDuration(&day, mloc_timeUnitDay, 1000000000, mloc_roundCeil, &out) ==
                  mloc_success &&
              out.seconds == INT64_C(86400000000000),
          "to 10^9 days");
    mlocTimeDuration near = {s_maxSeconds - 1, 0};
    CHECK(mlocRoundTimeDuration(&near, mloc_timeUnitHour, 1, mloc_roundCeil, &out) ==
              mloc_errorInvalid,
          "rounded past 2^53 seconds");
    mlocTimeDuration ms = {0, -1500000};
    CHECK(mlocRoundTimeDuration(&ms, mloc_timeUnitMillisecond, 1, mloc_roundHalfTrunc, &out) ==
                  mloc_success &&
              out.nanoseconds == -1000000,
          "half toward zero, negative");
    CHECK(mlocRoundTimeDuration(&day, mloc_timeUnitDay, 0, mloc_roundCeil, &out) ==
                  mloc_errorInvalid &&
              mlocRoundTimeDuration(&day, mloc_timeUnitDay, 1000000001, mloc_roundCeil, &out) ==
                  mloc_errorInvalid &&
              mlocRoundTimeDuration(&day, mloc_timeUnitHour, 24, mloc_roundCeil, &out) ==
                  mloc_errorInvalid &&
              mlocRoundTimeDuration(&day, mloc_timeUnitMinute, 7, mloc_roundCeil, &out) ==
                  mloc_errorInvalid &&
              mlocRoundTimeDuration(&day, (mlocTimeUnit)7, 1, mloc_roundCeil, &out) ==
                  mloc_errorInvalid &&
              mlocRoundTimeDuration(&day, mloc_timeUnitHour, 1, (mlocRoundingMode)9, &out) ==
                  mloc_errorInvalid &&
              mlocRoundTimeDuration(nullptr, mloc_timeUnitHour, 1, mloc_roundCeil, &out) ==
                  mloc_errorInvalid &&
              mlocAddTimeDurations(nullptr, &a, &out) == mloc_errorInvalid &&
              mlocAddTimeDurations(&a, &a, nullptr) == mloc_errorInvalid,
          "increments, units and modes refused, nulls");
    double total = 0;
    mlocTimeDuration second = {1, 0};
    CHECK(mlocTotalTimeDuration(&second, mloc_timeUnitMinute, &total) == mloc_success &&
              total == 1.0 / 60.0,
          "a second in minutes");
    mlocTimeDuration longest = {-(s_maxSeconds - 1), -999999999};
    CHECK(mlocTotalTimeDuration(&longest, mloc_timeUnitNanosecond, &total) == mloc_success &&
              total == -9007199254740991999999999.0,
          "every nanosecond, the nearest float64");
    // A numerator ten bits shorter than an hour's nanoseconds is scaled by
    // exactly 2^64 before the division; the bits are Python's
    // float(Fraction(2147483762, 3600 * 10**9)).
    mlocTimeDuration scaled = {2, 147483762};
    CHECK(mlocTotalTimeDuration(&scaled, mloc_timeUnitHour, &total) == mloc_success &&
              BitsOf(total) == UINT64_C(0x3f438bfff6465ea5),
          "a quotient scaled by 2^64");
    mlocTimeDuration zero = {0, 0};
    CHECK(mlocTotalTimeDuration(&zero, mloc_timeUnitDay, &total) == mloc_success &&
              BitsOf(total) == 0,
          "zero is +0");
    mlocTimeDuration forged = {1, -1};
    CHECK(mlocTotalTimeDuration(&forged, mloc_timeUnitDay, &total) == mloc_errorInvalid &&
              mlocTotalTimeDuration(&zero, (mlocTimeUnit)7, &total) == mloc_errorInvalid &&
              mlocTotalTimeDuration(&zero, mloc_timeUnitDay, nullptr) == mloc_errorInvalid,
          "a duration the library would not make, a unit refused, nulls");
}

static mlocDateTime At(int32_t year, uint32_t month, uint32_t day, uint8_t hour)
{
    mlocDateInput input = {.year = year, .month = month, .day = day};
    mlocDate date = {0};
    mlocTime time = {hour, 0, 0, 0, 0};
    mlocDateTime out = {0};
    CHECK(mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &date) ==
                  mloc_success &&
              mlocMakeDateTime(&date, &time, &out) == mloc_success,
          "a date-time");
    return out;
}

static void TestDateTimeTotal(void)
{
    mlocDateTime one = At(2020, 1, 1, 0);
    mlocDateTime two = At(2020, 1, 7, 20);
    double total = 0;
    CHECK(mlocDateTimeTotal(&one, &two, mloc_unitWeek, &total) == mloc_success &&
              total == (6.0 + 20.0 / 24.0) / 7.0,
          "6 days 20 hours in weeks");
    mlocDateTime march = At(2020, 3, 16, 0);
    CHECK(mlocDateTimeTotal(&one, &march, mloc_unitMonth, &total) == mloc_success &&
              total == 2.0 + 15.0 / 31.0,
          "2 months and 15 of March's 31 days");
    CHECK(mlocDateTimeTotal(&march, &one, mloc_unitMonth, &total) == mloc_success &&
              total == -(2.0 + 15.0 / 31.0),
          "back, 15 of the 31 days from December 16 to January 16");
    CHECK(mlocDateTimeTotal(&one, &two, mloc_unitHour, &total) == mloc_success && total == 164,
          "hours");
    CHECK(mlocDateTimeTotal(&one, &one, mloc_unitYear, &total) == mloc_success && total == 0,
          "equal date-times");
    mlocDateTime hebrew = one;
    CHECK(mlocMakeDateFromDay(mloc_calendarHebrew, one.date.day, &hebrew.date) == mloc_success &&
              mlocDateTimeTotal(&one, &hebrew, mloc_unitDay, &total) == mloc_errorInvalid &&
              mlocDateTimeTotal(&one, &two, (mlocUnit)10, &total) == mloc_errorInvalid &&
              mlocDateTimeTotal(nullptr, &two, mloc_unitDay, &total) == mloc_errorInvalid &&
              mlocDateTimeTotal(&one, nullptr, mloc_unitDay, &total) == mloc_errorInvalid &&
              mlocDateTimeTotal(&one, &two, mloc_unitDay, nullptr) == mloc_errorInvalid,
          "two calendars, a unit refused, nulls");
    mlocDateTime last = At(275760, 9, 13, 0);
    mlocDateTime before = At(275760, 8, 13, 0);
    CHECK(mlocDateTimeTotal(&before, &last, mloc_unitYear, &total) == mloc_errorInvalid,
          "the next year past the range");
    // Equal date-times on the range's first full day: no month before it
    // is asked for.
    mlocDateTime first = At(-271821, 4, 20, 0);
    CHECK(mlocDateTimeTotal(&first, &first, mloc_unitMonth, &total) == mloc_success && total == 0,
          "zero in months at the start of the range");
}

static void TestZonedTotal(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    mlocTimeZone berlin = {0};
    CHECK(mlocFindTimeZone(&data, "Europe/Berlin", 13, &berlin) == mloc_success, "Berlin");
    // Noon before the spring change of 2024, and 23 hours on.
    mlocInstant noon = {INT64_C(1711796400), 0};
    mlocInstant next = {INT64_C(1711796400) + 23 * 3600, 0};
    mlocInstant half = {INT64_C(1711796400) + 23 * 1800, 0};
    double total = 0;
    CHECK(mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &noon, &next, mloc_unitDay,
                                 &total) == mloc_success &&
              total == 1,
          "a day of 23 hours");
    CHECK(mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &noon, &half, mloc_unitDay,
                                 &total) == mloc_success &&
              total == 0.5,
          "half of it");
    CHECK(mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &noon, &next, mloc_unitHour,
                                 &total) == mloc_success &&
              total == 23,
          "23 hours");
    mlocTimeZone forged = {.offsetMinutes = 1440};
    mlocInstant past = {INT64_C(8640000000000), 1};
    CHECK(mlocZonedDateTimeTotal(&forged, mloc_calendarGregorian, &noon, &next, mloc_unitDay,
                                 &total) == mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &noon, &past, mloc_unitHour,
                                     &total) == mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&berlin, (mlocCalendar)200, &noon, &next, mloc_unitDay,
                                     &total) == mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &noon, &next, (mlocUnit)10,
                                     &total) == mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, nullptr, &next, mloc_unitDay,
                                     &total) == mloc_errorInvalid &&
              mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &noon, &next, mloc_unitDay,
                                     nullptr) == mloc_errorInvalid,
          "a zone, instant, calendar or unit refused, nulls");
    mlocInstant last = {INT64_C(8640000000000), 0};
    mlocInstant before = {INT64_C(8640000000000) - 86400, 0};
    CHECK(mlocZonedDateTimeTotal(&berlin, mloc_calendarGregorian, &before, &last, mloc_unitMonth,
                                 &total) == mloc_errorInvalid,
          "the next month past the range");
}

int main(void)
{
    TestMakeDuration();
    TestGetDurationValues();
    TestTimeDurations();
    TestDateTimeTotal();
    TestZonedTotal();
    return s_failures == 0 ? 0 : 1;
}
