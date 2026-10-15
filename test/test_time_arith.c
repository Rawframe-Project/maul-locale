// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time durations and the arithmetic of times of day through the public
// functions (record mloc-0041): durations from fields at their limits and
// back to fields in each largest unit, a time plus a duration across days
// both ways, the difference of two times rounded, rounding to each unit
// and to a day, and every refusal.

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <string.h>

static const int64_t s_maxSeconds = INT64_C(9007199254740992);

static bool DurationIs(const mlocTimeDurationFields* f, int64_t seconds, int32_t nanoseconds)
{
    mlocTimeDuration d = {0};
    return mlocMakeTimeDuration(f, &d) == mloc_success && d.seconds == seconds &&
           d.nanoseconds == nanoseconds;
}

static bool FieldsAre(const mlocTimeDurationFields* f, int64_t days, int64_t hours, int64_t minutes,
                      int64_t seconds, int64_t ms, int64_t us, int64_t ns)
{
    return f->days == days && f->hours == hours && f->minutes == minutes && f->seconds == seconds &&
           f->milliseconds == ms && f->microseconds == us && f->nanoseconds == ns;
}

static void TestMakeDuration(void)
{
    mlocTimeDurationFields f = {1, 2, 3, 4, 5, 6, 7};
    CHECK(DurationIs(&f, 86400 + 7200 + 180 + 4, 5006007), "fields summed, days of 24 hours");
    f = (mlocTimeDurationFields){.seconds = 1, .nanoseconds = -1};
    CHECK(DurationIs(&f, 0, 999999999), "of one sign after a borrow");
    f = (mlocTimeDurationFields){.seconds = -1, .nanoseconds = 1};
    CHECK(DurationIs(&f, 0, -999999999), "and the other way");
    f = (mlocTimeDurationFields){
        .milliseconds = -1500, .microseconds = -2500000, .nanoseconds = -3500000000};
    CHECK(DurationIs(&f, -7, -500000000), "subsecond fields carry into seconds");
    f = (mlocTimeDurationFields){.seconds = s_maxSeconds - 1, .nanoseconds = 999999999};
    CHECK(DurationIs(&f, s_maxSeconds - 1, 999999999), "the largest duration");
    mlocTimeDuration d = {0};
    f = (mlocTimeDurationFields){.seconds = s_maxSeconds - 1, .nanoseconds = 1000000000};
    CHECK(mlocMakeTimeDuration(&f, &d) == mloc_errorInvalid, "a nanosecond more is out");
    static const mlocTimeDurationFields s_out[] = {
        {.days = INT64_C(104249991375)},
        {.hours = INT64_C(2501999792984)},
        {.minutes = INT64_C(150119987579017)},
        {.seconds = INT64_C(9007199254740992)},
        {.milliseconds = INT64_MIN},
        {.microseconds = INT64_MIN},
        {.nanoseconds = INT64_MIN},
        {.days = INT64_C(-104249991375)},
    };
    for (size_t i = 0; i < sizeof s_out / sizeof s_out[0]; i++)
    {
        CHECK(mlocMakeTimeDuration(&s_out[i], &d) == mloc_errorInvalid, "a field past 2^53 s");
    }
    f = (mlocTimeDurationFields){.days = INT64_C(104249991374), .hours = 7};
    CHECK(DurationIs(&f, INT64_C(9007199254738800), 0), "the most days, and hours");
    f.hours = 8;
    CHECK(mlocMakeTimeDuration(&f, &d) == mloc_errorInvalid, "a total past 2^53 s");
    CHECK(mlocMakeTimeDuration(nullptr, &d) == mloc_errorInvalid &&
              mlocMakeTimeDuration(&f, nullptr) == mloc_errorInvalid,
          "null pointers");
}

static void TestBalance(void)
{
    mlocTimeDuration d = {-(2 * 86400 + 3 * 3600 + 4 * 60 + 5), -6007008};
    mlocTimeDurationFields f = {0};
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitDay, &f) == mloc_success &&
              FieldsAre(&f, -2, -3, -4, -5, -6, -7, -8),
          "days");
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitHour, &f) == mloc_success &&
              FieldsAre(&f, 0, -51, -4, -5, -6, -7, -8),
          "hours");
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitMinute, &f) == mloc_success &&
              FieldsAre(&f, 0, 0, -3064, -5, -6, -7, -8),
          "minutes");
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitSecond, &f) == mloc_success &&
              FieldsAre(&f, 0, 0, 0, -183845, -6, -7, -8),
          "seconds");
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitMillisecond, &f) == mloc_success &&
              FieldsAre(&f, 0, 0, 0, 0, -183845006, -7, -8),
          "milliseconds");
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitMicrosecond, &f) == mloc_success &&
              FieldsAre(&f, 0, 0, 0, 0, 0, -183845006007, -8),
          "microseconds");
    CHECK(mlocBalanceTimeDuration(&d, mloc_timeUnitNanosecond, &f) == mloc_success &&
              FieldsAre(&f, 0, 0, 0, 0, 0, 0, -183845006007008),
          "nanoseconds");
    mlocTimeDuration largest = {s_maxSeconds - 1, 999999999};
    CHECK(mlocBalanceTimeDuration(&largest, mloc_timeUnitMillisecond, &f) == mloc_success &&
              f.milliseconds == INT64_C(9007199254740991999),
          "the largest duration in milliseconds");
    CHECK(mlocBalanceTimeDuration(&largest, mloc_timeUnitMicrosecond, &f) == mloc_errorInvalid &&
              mlocBalanceTimeDuration(&largest, mloc_timeUnitNanosecond, &f) == mloc_errorInvalid,
          "past int64_t in microseconds and nanoseconds");
    mlocTimeDuration edgeMicro = {INT64_C(9223372036853), 775807000};
    mlocTimeDuration pastMicro = {INT64_C(9223372036854), 775808000};
    mlocTimeDuration edgeNano = {INT64_C(9223372035), 999999999};
    mlocTimeDuration pastNano = {INT64_C(9223372036), 854775808};
    CHECK(mlocBalanceTimeDuration(&edgeMicro, mloc_timeUnitMicrosecond, &f) == mloc_success &&
              mlocBalanceTimeDuration(&pastMicro, mloc_timeUnitMicrosecond, &f) ==
                  mloc_errorInvalid &&
              mlocBalanceTimeDuration(&edgeNano, mloc_timeUnitNanosecond, &f) == mloc_success &&
              mlocBalanceTimeDuration(&pastNano, mloc_timeUnitNanosecond, &f) == mloc_errorInvalid,
          "the int64_t edges");
    mlocTimeDuration mixed = {1, -1};
    mlocTimeDuration wide = {0, 1000000000};
    CHECK(mlocBalanceTimeDuration(&mixed, mloc_timeUnitHour, &f) == mloc_errorInvalid &&
              mlocBalanceTimeDuration(&wide, mloc_timeUnitHour, &f) == mloc_errorInvalid &&
              mlocBalanceTimeDuration(&d, (mlocTimeUnit)7, &f) == mloc_errorInvalid &&
              mlocBalanceTimeDuration(nullptr, mloc_timeUnitHour, &f) == mloc_errorInvalid &&
              mlocBalanceTimeDuration(&d, mloc_timeUnitHour, nullptr) == mloc_errorInvalid,
          "a duration the library would not make, another unit, null pointers");
}

static bool TimeIs(const mlocTime* t, uint8_t hour, uint8_t minute, uint8_t second,
                   uint32_t nanosecond)
{
    return t->hour == hour && t->minute == minute && t->second == second &&
           t->nanosecond == nanosecond && t->reserved == 0;
}

static void TestAdd(void)
{
    mlocTime start = {23, 59, 59, 0, 999999999};
    mlocTime out = {0};
    int64_t days = 0;
    mlocTimeDuration one = {0, 1};
    CHECK(mlocAddToTime(&start, &one, &out, &days) == mloc_success && TimeIs(&out, 0, 0, 0, 0) &&
              days == 1,
          "a nanosecond into the next day");
    mlocTime midnight = {0};
    mlocTimeDuration back = {-86400 * 3 - 1, -500};
    CHECK(mlocAddToTime(&midnight, &back, &out, &days) == mloc_success &&
              TimeIs(&out, 23, 59, 58, 999999500) && days == -4,
          "back across days");
    mlocTimeDuration largest = {s_maxSeconds - 1, 999999999};
    CHECK(mlocAddToTime(&start, &largest, &out, nullptr) == mloc_success,
          "the largest duration, days dropped");
    mlocTime bad = {.hour = 24};
    mlocTimeDuration mixed = {1, -1};
    CHECK(mlocAddToTime(&bad, &one, &out, &days) == mloc_errorInvalid &&
              mlocAddToTime(&start, &mixed, &out, &days) == mloc_errorInvalid &&
              mlocAddToTime(nullptr, &one, &out, &days) == mloc_errorInvalid &&
              mlocAddToTime(&start, nullptr, &out, &days) == mloc_errorInvalid &&
              mlocAddToTime(&start, &one, nullptr, &days) == mloc_errorInvalid,
          "a time or duration the library would not make, null pointers");
}

static void TestUntil(void)
{
    mlocTime one = {12, 34, 56, 0, 987654321};
    mlocTime two = {18, 4, 5, 0, 123456789};
    mlocTimeDurationFields f = {0};
    mlocTimeRounding r = {mloc_timeUnitHour, mloc_timeUnitNanosecond, 1, mloc_roundTrunc};
    CHECK(mlocTimeUntil(&one, &two, &r, &f) == mloc_success &&
              FieldsAre(&f, 0, 5, 29, 8, 135, 802, 468),
          "hours down to nanoseconds");
    r = (mlocTimeRounding){mloc_timeUnitMinute, mloc_timeUnitMinute, 15, mloc_roundHalfExpand};
    CHECK(mlocTimeUntil(&two, &one, &r, &f) == mloc_success &&
              FieldsAre(&f, 0, 0, -330, 0, 0, 0, 0),
          "back, rounded to 15 minutes");
    r = (mlocTimeRounding){mloc_timeUnitHour, mloc_timeUnitHour, 1, mloc_roundFloor};
    CHECK(mlocTimeUntil(&two, &one, &r, &f) == mloc_success && f.hours == -6,
          "floor of a negative difference");
    static const mlocTimeRounding s_refused[] = {
        {mloc_timeUnitDay, mloc_timeUnitHour, 1, mloc_roundTrunc},
        {mloc_timeUnitHour, (mlocTimeUnit)7, 1, mloc_roundTrunc},
        {mloc_timeUnitSecond, mloc_timeUnitMinute, 1, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitHour, 24, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitHour, 5, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitMinute, 0, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitNanosecond, 1000, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitSecond, 1, (mlocRoundingMode)9},
    };
    for (size_t i = 0; i < sizeof s_refused / sizeof s_refused[0]; i++)
    {
        CHECK(mlocTimeUntil(&one, &two, &s_refused[i], &f) == mloc_errorInvalid,
              "a unit, increment or mode refused");
    }
    r = (mlocTimeRounding){mloc_timeUnitHour, mloc_timeUnitNanosecond, 500, mloc_roundTrunc};
    mlocTime bad = {.reserved = 1};
    CHECK(mlocTimeUntil(&one, &two, &r, &f) == mloc_success &&
              mlocTimeUntil(&bad, &two, &r, &f) == mloc_errorInvalid &&
              mlocTimeUntil(&one, &bad, &r, &f) == mloc_errorInvalid &&
              mlocTimeUntil(nullptr, &two, &r, &f) == mloc_errorInvalid &&
              mlocTimeUntil(&one, nullptr, &r, &f) == mloc_errorInvalid &&
              mlocTimeUntil(&one, &two, nullptr, &f) == mloc_errorInvalid &&
              mlocTimeUntil(&one, &two, &r, nullptr) == mloc_errorInvalid,
          "500 divides 1000; a time the library did not make, null pointers");
}

static void TestRound(void)
{
    mlocTime t = {13, 46, 23, 0, 123987500};
    mlocTime out = {0};
    int64_t days = -1;
    CHECK(mlocRoundTime(&t, mloc_timeUnitHour, 1, mloc_roundCeil, &out, &days) == mloc_success &&
              TimeIs(&out, 14, 0, 0, 0) && days == 0,
          "hours");
    CHECK(mlocRoundTime(&t, mloc_timeUnitMicrosecond, 1, mloc_roundHalfEven, &out, nullptr) ==
                  mloc_success &&
              TimeIs(&out, 13, 46, 23, 123988000),
          "a half to even");
    CHECK(mlocRoundTime(&t, mloc_timeUnitMinute, 30, mloc_roundHalfExpand, &out, &days) ==
                  mloc_success &&
              TimeIs(&out, 14, 0, 0, 0) && days == 0,
          "half an hour");
    CHECK(mlocRoundTime(&t, mloc_timeUnitDay, 1, mloc_roundHalfExpand, &out, &days) ==
                  mloc_success &&
              TimeIs(&out, 0, 0, 0, 0) && days == 1,
          "to the next midnight");
    CHECK(mlocRoundTime(&t, mloc_timeUnitDay, 1, mloc_roundFloor, &out, &days) == mloc_success &&
              TimeIs(&out, 0, 0, 0, 0) && days == 0,
          "to this midnight");
    mlocTime late = {23, 59, 59, 0, 999999999};
    CHECK(mlocRoundTime(&late, mloc_timeUnitSecond, 1, mloc_roundExpand, &out, &days) ==
                  mloc_success &&
              TimeIs(&out, 0, 0, 0, 0) && days == 1,
          "a second's rounding passing midnight");
    CHECK(mlocRoundTime(&t, mloc_timeUnitDay, 2, mloc_roundFloor, &out, &days) ==
                  mloc_errorInvalid &&
              mlocRoundTime(&t, mloc_timeUnitHour, 24, mloc_roundFloor, &out, &days) ==
                  mloc_errorInvalid &&
              mlocRoundTime(&t, mloc_timeUnitHour, 7, mloc_roundFloor, &out, &days) ==
                  mloc_errorInvalid &&
              mlocRoundTime(&t, (mlocTimeUnit)7, 1, mloc_roundFloor, &out, &days) ==
                  mloc_errorInvalid &&
              mlocRoundTime(&t, mloc_timeUnitHour, 1, (mlocRoundingMode)9, &out, &days) ==
                  mloc_errorInvalid &&
              mlocRoundTime(nullptr, mloc_timeUnitHour, 1, mloc_roundFloor, &out, &days) ==
                  mloc_errorInvalid &&
              mlocRoundTime(&t, mloc_timeUnitHour, 1, mloc_roundFloor, nullptr, &days) ==
                  mloc_errorInvalid,
          "an increment, unit or mode refused, null pointers");
}

int main(void)
{
    TestMakeDuration();
    TestBalance();
    TestAdd();
    TestUntil();
    TestRound();
    return s_failures == 0 ? 0 : 1;
}
