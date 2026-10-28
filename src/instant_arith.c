// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic of instants (maul-locale/zone.h): ECMA-262's
// AddDurationToInstant, DifferenceEpochNanoseconds and
// RoundEpochNanoseconds. An instant's seconds and nanoseconds, and a day
// with the nanoseconds past its midnight for rounding, keep every value
// within 64 bits.

#include "duration_round.h"
#include "invariant.h"
#include "rounding.h"
#include "time_arith.h"
#include "zone_handle.h"

#include "maul-locale/zone.h"

enum
{
    nanosecondsPerSecond = 1000000000,
    secondsPerDay = 86400,
};

static const int64_t s_nanosecondsPerDay = INT64_C(86400000000000);
// Each time unit's length in nanoseconds, hour on, and in whole seconds
// where it has them.
static const int64_t s_unitLength[6] = {
    INT64_C(3600000000000), INT64_C(60000000000), 1000000000, 1000000, 1000, 1};
static const int64_t s_unitSeconds[6] = {3600, 60, 1, 0, 0, 0};
static const uint32_t s_maxIncrement[6] = {24, 60, 60, 1000, 1000, 1000};

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b < 0 ? 1 : 0);
}

mlocResult mlocAddToInstant(const mlocInstant* instant, const mlocTimeDuration* duration,
                            mlocInstant* instantOut)
{
    if (instant == nullptr || duration == nullptr || instantOut == nullptr ||
        !mlocValidInstant(instant) || !mlocTimeDurationValid(duration))
    {
        return mloc_errorInvalid;
    }
    int64_t nanoseconds = (int64_t)instant->nanoseconds + duration->nanoseconds;
    int64_t carry = FloorDiv(nanoseconds, nanosecondsPerSecond);
    mlocInstant result = {instant->seconds + duration->seconds + carry,
                          (int32_t)(nanoseconds - carry * nanosecondsPerSecond)};
    MLOC_ASSERT(result.nanoseconds >= 0 && result.nanoseconds < nanosecondsPerSecond);
    if (!mlocValidInstant(&result))
    {
        return mloc_errorInvalid;
    }
    *instantOut = result;
    return mloc_success;
}

mlocResult mlocInstantUntil(const mlocInstant* one, const mlocInstant* two,
                            const mlocTimeRounding* rounding, mlocTimeDuration* durationOut)
{
    if (one == nullptr || two == nullptr || rounding == nullptr || durationOut == nullptr ||
        !mlocValidInstant(one) || !mlocValidInstant(two) ||
        rounding->largestUnit < mloc_timeUnitHour ||
        (uint32_t)rounding->smallestUnit > mloc_timeUnitNanosecond ||
        rounding->smallestUnit < rounding->largestUnit || rounding->increment == 0 ||
        rounding->increment >= s_maxIncrement[rounding->smallestUnit - 1] ||
        s_maxIncrement[rounding->smallestUnit - 1] % rounding->increment != 0 ||
        (uint32_t)rounding->mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    // Two instants lie within 2 * 10^8 days, far below 2^53 seconds.
    int64_t seconds = two->seconds - one->seconds;
    int64_t nanoseconds = (int64_t)two->nanoseconds - one->nanoseconds;
    if (seconds > 0 && nanoseconds < 0)
    {
        seconds -= 1;
        nanoseconds += nanosecondsPerSecond;
    }
    else if (seconds < 0 && nanoseconds > 0)
    {
        seconds += 1;
        nanoseconds -= nanosecondsPerSecond;
    }
    int unit = rounding->smallestUnit - 1;
    mlocTimeDuration d = {seconds, (int32_t)nanoseconds};
    mlocTimeDuration rounded;
    if (!mlocRoundTimeStep(d, s_unitSeconds[unit] * rounding->increment,
                           s_unitSeconds[unit] > 0 ? 0 : s_unitLength[unit] * rounding->increment,
                           rounding->mode, &rounded))
    {
        return mloc_errorInvalid;
    }
    *durationOut = rounded;
    return mloc_success;
}

mlocResult mlocRoundInstant(const mlocInstant* instant, mlocTimeUnit unit, uint32_t increment,
                            mlocRoundingMode mode, mlocInstant* instantOut)
{
    if (instant == nullptr || instantOut == nullptr || !mlocValidInstant(instant) ||
        unit < mloc_timeUnitHour || (uint32_t)unit > mloc_timeUnitNanosecond ||
        (uint32_t)mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    int64_t length = s_unitLength[unit - 1];
    int64_t perDay = s_nanosecondsPerDay / length;
    if (increment == 0 || increment > perDay || perDay % increment != 0)
    {
        return mloc_errorInvalid;
    }
    // The step divides a day, so the day's start is a multiple and the
    // nanoseconds past it round alone, as a positive value. Half-even's
    // parity counts the days when a day holds an odd number of steps.
    int64_t step = length * increment;
    int64_t day = FloorDiv(instant->seconds, secondsPerDay);
    int64_t past =
        (instant->seconds - day * secondsPerDay) * nanosecondsPerSecond + instant->nanoseconds;
    // The step divides the day, and the nanoseconds lie within it.
    MLOC_ASSERT(past >= 0 && past < s_nanosecondsPerDay && s_nanosecondsPerDay % step == 0);
    int64_t low = past / step;
    int64_t stepsPerDay = s_nanosecondsPerDay / step;
    bool evenLow = ((day & 1) * (stepsPerDay % 2) + low) % 2 == 0;
    bool up = mlocRoundsUp(past % step, step, mlocUnsignedRoundingMode(mode, false), evenLow);
    int64_t rounded = (low + (up ? 1 : 0)) * step;
    *instantOut = (mlocInstant){day * secondsPerDay + rounded / nanosecondsPerSecond,
                                (int32_t)(rounded % nanosecondsPerSecond)};
    return mloc_success;
}
