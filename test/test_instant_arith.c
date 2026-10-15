// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic of instants through the public functions (record
// mloc-0041): adding to the ends of Temporal's range, differences across
// all of it, rounding as if positive before 1970, half-even's parity over
// whole days, and every refusal.

#include "test_harness.h"

#include "maul-locale/zone.h"

static const int64_t s_range = INT64_C(8640000000000);

static bool InstantIs(const mlocInstant* i, int64_t seconds, int32_t nanoseconds)
{
    return i->seconds == seconds && i->nanoseconds == nanoseconds;
}

static void TestAdd(void)
{
    mlocInstant min = {-s_range, 0};
    mlocInstant max = {s_range, 0};
    mlocInstant out = {0};
    mlocTimeDuration back = {0, -1};
    mlocTimeDuration on = {0, 1};
    mlocTimeDuration across = {2 * s_range, 0};
    CHECK(mlocAddToInstant(&max, &back, &out) == mloc_success &&
              InstantIs(&out, s_range - 1, 999999999),
          "a nanosecond before the last");
    CHECK(mlocAddToInstant(&min, &across, &out) == mloc_success && InstantIs(&out, s_range, 0),
          "the first to the last");
    CHECK(mlocAddToInstant(&max, &on, &out) == mloc_errorInvalid &&
              mlocAddToInstant(&min, &back, &out) == mloc_errorInvalid,
          "past either end");
    mlocInstant forged = {0, 1000000000};
    mlocTimeDuration mixed = {1, -1};
    CHECK(mlocAddToInstant(&forged, &on, &out) == mloc_errorInvalid &&
              mlocAddToInstant(&min, &mixed, &out) == mloc_errorInvalid &&
              mlocAddToInstant(nullptr, &on, &out) == mloc_errorInvalid &&
              mlocAddToInstant(&min, nullptr, &out) == mloc_errorInvalid &&
              mlocAddToInstant(&min, &on, nullptr) == mloc_errorInvalid,
          "an instant or duration the library would not make, null pointers");
}

static void TestUntil(void)
{
    mlocInstant min = {-s_range, 0};
    mlocInstant max = {s_range, 0};
    mlocTimeDuration d = {0};
    mlocTimeRounding r = {mloc_timeUnitHour, mloc_timeUnitNanosecond, 1, mloc_roundTrunc};
    CHECK(mlocInstantUntil(&min, &max, &r, &d) == mloc_success && d.seconds == 2 * s_range &&
              d.nanoseconds == 0,
          "all of the range");
    mlocInstant a = {10, 999999999};
    mlocInstant b = {-10, 1};
    CHECK(mlocInstantUntil(&a, &b, &r, &d) == mloc_success && d.seconds == -20 &&
              d.nanoseconds == -999999998,
          "back, of one sign");
    r = (mlocTimeRounding){mloc_timeUnitHour, mloc_timeUnitSecond, 30, mloc_roundHalfExpand};
    CHECK(mlocInstantUntil(&b, &a, &r, &d) == mloc_success && d.seconds == 30 && d.nanoseconds == 0,
          "to 30 seconds");
    static const mlocTimeRounding s_refused[] = {
        {mloc_timeUnitDay, mloc_timeUnitHour, 1, mloc_roundTrunc},
        {mloc_timeUnitMinute, mloc_timeUnitHour, 1, mloc_roundTrunc},
        {mloc_timeUnitHour, (mlocTimeUnit)7, 1, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitHour, 24, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitMinute, 7, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitSecond, 0, mloc_roundTrunc},
        {mloc_timeUnitHour, mloc_timeUnitSecond, 1, (mlocRoundingMode)9},
    };
    for (size_t i = 0; i < sizeof s_refused / sizeof s_refused[0]; i++)
    {
        CHECK(mlocInstantUntil(&a, &b, &s_refused[i], &d) == mloc_errorInvalid,
              "a unit, increment or mode refused");
    }
    mlocInstant forged = {s_range, 1};
    CHECK(mlocInstantUntil(&forged, &b, &r, &d) == mloc_errorInvalid &&
              mlocInstantUntil(nullptr, &b, &r, &d) == mloc_errorInvalid &&
              mlocInstantUntil(&a, nullptr, &r, &d) == mloc_errorInvalid &&
              mlocInstantUntil(&a, &b, nullptr, &d) == mloc_errorInvalid &&
              mlocInstantUntil(&a, &b, &r, nullptr) == mloc_errorInvalid,
          "an instant the library would not make, null pointers");
}

static bool RoundedIs(int64_t seconds, int32_t nanoseconds, mlocTimeUnit unit, uint32_t increment,
                      mlocRoundingMode mode, int64_t wantSeconds, int32_t wantNanoseconds)
{
    mlocInstant i = {seconds, nanoseconds};
    mlocInstant out = {0};
    return mlocRoundInstant(&i, unit, increment, mode, &out) == mloc_success &&
           InstantIs(&out, wantSeconds, wantNanoseconds);
}

static void TestRound(void)
{
    CHECK(RoundedIs(-2, 500000000, mloc_timeUnitSecond, 1, mloc_roundFloor, -2, 0) &&
              RoundedIs(-2, 500000000, mloc_timeUnitSecond, 1, mloc_roundTrunc, -2, 0) &&
              RoundedIs(-2, 500000000, mloc_timeUnitSecond, 1, mloc_roundCeil, -1, 0) &&
              RoundedIs(-2, 500000000, mloc_timeUnitSecond, 1, mloc_roundHalfTrunc, -2, 0) &&
              RoundedIs(-2, 500000000, mloc_timeUnitSecond, 1, mloc_roundHalfExpand, -1, 0),
          "before 1970, rounded as if positive");
    CHECK(
        RoundedIs(86400 + 43200, 0, mloc_timeUnitHour, 24, mloc_roundHalfEven, 2 * 86400, 0) &&
            RoundedIs(43200, 0, mloc_timeUnitHour, 24, mloc_roundHalfEven, 0, 0) &&
            RoundedIs(-43200, 0, mloc_timeUnitHour, 24, mloc_roundHalfEven, 0, 0) &&
            RoundedIs(-86400 - 43200, 0, mloc_timeUnitHour, 24, mloc_roundHalfEven, -2 * 86400, 0),
        "a whole day's step: half to the even day");
    CHECK(RoundedIs(3, 500000000, mloc_timeUnitMillisecond, 6, mloc_roundHalfEven, 3, 498000000) &&
              RoundedIs(s_range - 1, 999999999, mloc_timeUnitHour, 1, mloc_roundCeil, s_range, 0),
          "6 ms steps, which divide a day but not a second; the last instant");
    mlocInstant i = {0, 0};
    mlocInstant out = {0};
    CHECK(
        mlocRoundInstant(&i, mloc_timeUnitHour, 25, mloc_roundFloor, &out) == mloc_errorInvalid &&
            mlocRoundInstant(&i, mloc_timeUnitMinute, 7, mloc_roundFloor, &out) ==
                mloc_errorInvalid &&
            mlocRoundInstant(&i, mloc_timeUnitHour, 0, mloc_roundFloor, &out) ==
                mloc_errorInvalid &&
            mlocRoundInstant(&i, mloc_timeUnitDay, 1, mloc_roundFloor, &out) == mloc_errorInvalid &&
            mlocRoundInstant(&i, (mlocTimeUnit)7, 1, mloc_roundFloor, &out) == mloc_errorInvalid &&
            mlocRoundInstant(&i, mloc_timeUnitHour, 1, (mlocRoundingMode)9, &out) ==
                mloc_errorInvalid &&
            mlocRoundInstant(nullptr, mloc_timeUnitHour, 1, mloc_roundFloor, &out) ==
                mloc_errorInvalid &&
            mlocRoundInstant(&i, mloc_timeUnitHour, 1, mloc_roundFloor, nullptr) ==
                mloc_errorInvalid,
        "an increment, unit or mode refused, null pointers");
    CHECK(mlocRoundInstant(&i, mloc_timeUnitHour, 24, mloc_roundFloor, &out) == mloc_success,
          "a whole day of hours");
}

int main(void)
{
    TestAdd();
    TestUntil();
    TestRound();
    return s_failures == 0 ? 0 : 1;
}
