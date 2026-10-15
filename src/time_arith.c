// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time durations and the arithmetic of times of day (maul-locale/zone.h,
// record mloc-0041): ECMA-262's TimeDurationFromComponents,
// TemporalDurationFromInternal's balancing, AddTime, DifferenceTime with
// RoundTimeDuration, and RoundTime. A duration is seconds and
// nanoseconds, so its 2^53 seconds need no wider integers.

#include "date_time.h"
#include "rounding.h"

#include "maul-locale/zone.h"

enum
{
    nanosecondsPerSecond = 1000000000,
};

static const int64_t s_maxSeconds = INT64_C(9007199254740992);
static const int64_t s_nanosecondsPerDay = INT64_C(86400000000000);

// Each unit's length in nanoseconds, and the length of the unit above it
// that bounds its increment (a day for days and hours).
static const int64_t s_unitLength[7] = {
    INT64_C(86400000000000),
    INT64_C(3600000000000),
    INT64_C(60000000000),
    1000000000,
    1000000,
    1000,
    1,
};
static const int64_t s_parentLength[7] = {
    INT64_C(86400000000000),
    INT64_C(86400000000000),
    INT64_C(3600000000000),
    INT64_C(60000000000),
    1000000000,
    1000000,
    1000,
};
// MaximumTemporalDurationRoundingIncrement, and 1 for days.
static const uint32_t s_maxIncrement[7] = {1, 24, 60, 60, 1000, 1000, 1000};

static bool ValidDuration(const mlocTimeDuration* d)
{
    return d->seconds > -s_maxSeconds && d->seconds < s_maxSeconds &&
           d->nanoseconds > -nanosecondsPerSecond && d->nanoseconds < nanosecondsPerSecond &&
           !(d->seconds > 0 && d->nanoseconds < 0) && !(d->seconds < 0 && d->nanoseconds > 0);
}

static int64_t Magnitude(int64_t v)
{
    return v < 0 ? -v : v;
}

// ValidateTemporalRoundingIncrement: a day takes 1; another unit an
// increment dividing the unit above it into more than one part.
static bool ValidIncrement(mlocTimeUnit unit, uint32_t increment)
{
    uint32_t maximum = s_maxIncrement[unit];
    return increment >= 1 && (unit == mloc_timeUnitDay ? increment == 1 : increment < maximum) &&
           maximum % increment == 0;
}

// Seconds and nanoseconds of any signs, |nanoseconds| below 10^10, as a
// duration of one sign.
static bool Normalize(int64_t seconds, int64_t nanoseconds, mlocTimeDuration* out)
{
    seconds += nanoseconds / nanosecondsPerSecond;
    nanoseconds %= nanosecondsPerSecond;
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
    *out = (mlocTimeDuration){seconds, (int32_t)nanoseconds};
    return ValidDuration(out);
}

mlocResult mlocMakeTimeDuration(const mlocTimeDurationFields* fields, mlocTimeDuration* durationOut)
{
    if (fields == nullptr || durationOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    const mlocTimeDurationFields* f = fields;
    // Each field alone below 2^53 seconds, so that the sums below stay in
    // int64_t.
    if (Magnitude(f->days) > s_maxSeconds / 86400 || Magnitude(f->hours) > s_maxSeconds / 3600 ||
        Magnitude(f->minutes) > s_maxSeconds / 60 || Magnitude(f->seconds) >= s_maxSeconds ||
        f->milliseconds == INT64_MIN || f->microseconds == INT64_MIN || f->nanoseconds == INT64_MIN)
    {
        return mloc_errorInvalid;
    }
    int64_t seconds = f->days * 86400 + f->hours * 3600 + f->minutes * 60 + f->seconds +
                      f->milliseconds / 1000 + f->microseconds / 1000000 +
                      f->nanoseconds / nanosecondsPerSecond;
    int64_t nanoseconds = f->milliseconds % 1000 * 1000000 + f->microseconds % 1000000 * 1000 +
                          f->nanoseconds % nanosecondsPerSecond;
    mlocTimeDuration d;
    if (!Normalize(seconds, nanoseconds, &d))
    {
        return mloc_errorInvalid;
    }
    *durationOut = d;
    return mloc_success;
}

mlocResult mlocBalanceTimeDuration(const mlocTimeDuration* duration, mlocTimeUnit largestUnit,
                                   mlocTimeDurationFields* fieldsOut)
{
    if (duration == nullptr || fieldsOut == nullptr || !ValidDuration(duration) ||
        (uint32_t)largestUnit > mloc_timeUnitNanosecond)
    {
        return mloc_errorInvalid;
    }
    int64_t s = Magnitude(duration->seconds);
    int64_t n = Magnitude(duration->nanoseconds);
    mlocTimeDurationFields f = {
        .milliseconds = n / 1000000, .microseconds = n / 1000 % 1000, .nanoseconds = n % 1000};
    switch (largestUnit)
    {
    case mloc_timeUnitDay:
        f = (mlocTimeDurationFields){s / 86400,      s % 86400 / 3600, s % 3600 / 60, s % 60,
                                     f.milliseconds, f.microseconds,   f.nanoseconds};
        break;
    case mloc_timeUnitHour:
        f = (mlocTimeDurationFields){
            0, s / 3600, s % 3600 / 60, s % 60, f.milliseconds, f.microseconds, f.nanoseconds};
        break;
    case mloc_timeUnitMinute:
        f.minutes = s / 60;
        f.seconds = s % 60;
        break;
    case mloc_timeUnitSecond:
        f.seconds = s;
        break;
    case mloc_timeUnitMillisecond:
        f.milliseconds += s * 1000;
        break;
    case mloc_timeUnitMicrosecond:
        if (s > (INT64_MAX - 999999) / 1000000)
        {
            return mloc_errorInvalid;
        }
        f = (mlocTimeDurationFields){.microseconds = s * 1000000 + n / 1000,
                                     .nanoseconds = n % 1000};
        break;
    default:
        if (s > (INT64_MAX - (nanosecondsPerSecond - 1)) / nanosecondsPerSecond)
        {
            return mloc_errorInvalid;
        }
        f = (mlocTimeDurationFields){.nanoseconds = s * nanosecondsPerSecond + n};
        break;
    }
    if (duration->seconds < 0 || duration->nanoseconds < 0)
    {
        f = (mlocTimeDurationFields){-f.days,         -f.hours,        -f.minutes,    -f.seconds,
                                     -f.milliseconds, -f.microseconds, -f.nanoseconds};
    }
    *fieldsOut = f;
    return mloc_success;
}

static int64_t SecondOfDay(const mlocTime* t)
{
    return t->hour * 3600 + t->minute * 60 + t->second;
}

static int64_t NanosecondOfDay(const mlocTime* t)
{
    return SecondOfDay(t) * nanosecondsPerSecond + t->nanosecond;
}

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b < 0 ? 1 : 0);
}

// A second of a day, and the nanoseconds past it, as a time.
static mlocTime TimeOf(int64_t second, int64_t nanosecond)
{
    return (mlocTime){.hour = (uint8_t)(second / 3600),
                      .minute = (uint8_t)(second / 60 % 60),
                      .second = (uint8_t)(second % 60),
                      .nanosecond = (uint32_t)nanosecond};
}

mlocResult mlocAddToTime(const mlocTime* time, const mlocTimeDuration* duration, mlocTime* timeOut,
                         int64_t* daysOut)
{
    if (time == nullptr || duration == nullptr || timeOut == nullptr || !mlocTimeValid(time) ||
        !ValidDuration(duration))
    {
        return mloc_errorInvalid;
    }
    int64_t nanoseconds = (int64_t)time->nanosecond + duration->nanoseconds;
    int64_t carry = FloorDiv(nanoseconds, nanosecondsPerSecond);
    int64_t seconds = SecondOfDay(time) + duration->seconds + carry;
    int64_t days = FloorDiv(seconds, secondsPerDay);
    *timeOut = TimeOf(seconds - days * secondsPerDay, nanoseconds - carry * nanosecondsPerSecond);
    if (daysOut != nullptr)
    {
        *daysOut = days;
    }
    return mloc_success;
}

mlocResult mlocTimeUntil(const mlocTime* one, const mlocTime* two, const mlocTimeRounding* rounding,
                         mlocTimeDurationFields* fieldsOut)
{
    if (one == nullptr || two == nullptr || rounding == nullptr || fieldsOut == nullptr ||
        !mlocTimeValid(one) || !mlocTimeValid(two) || rounding->largestUnit < mloc_timeUnitHour ||
        (uint32_t)rounding->smallestUnit > mloc_timeUnitNanosecond ||
        rounding->smallestUnit < rounding->largestUnit ||
        !ValidIncrement(rounding->smallestUnit, rounding->increment) ||
        (uint32_t)rounding->mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    // DifferenceTime is under a day, so nanoseconds hold it.
    int64_t difference = NanosecondOfDay(two) - NanosecondOfDay(one);
    int64_t rounded = mlocRoundToIncrement(
        difference, s_unitLength[rounding->smallestUnit] * rounding->increment, rounding->mode);
    mlocTimeDuration d = {rounded / nanosecondsPerSecond,
                          (int32_t)(rounded % nanosecondsPerSecond)};
    return mlocBalanceTimeDuration(&d, rounding->largestUnit, fieldsOut);
}

mlocResult mlocRoundTime(const mlocTime* time, mlocTimeUnit unit, uint32_t increment,
                         mlocRoundingMode mode, mlocTime* timeOut, int64_t* daysOut)
{
    if (time == nullptr || timeOut == nullptr || !mlocTimeValid(time) ||
        (uint32_t)unit > mloc_timeUnitNanosecond || !ValidIncrement(unit, increment) ||
        (uint32_t)mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    // RoundTime rounds the unit and those below it (the quantity), within
    // the unit above, and balances the result back onto the units above.
    int64_t total = NanosecondOfDay(time);
    int64_t quantity = total % s_parentLength[unit];
    int64_t length = s_unitLength[unit];
    int64_t result = mlocRoundToIncrement(quantity, length * increment, mode);
    int64_t rounded = total - quantity + result;
    int64_t days = rounded / s_nanosecondsPerDay;
    int64_t rest = rounded % s_nanosecondsPerDay;
    *timeOut = TimeOf(rest / nanosecondsPerSecond, rest % nanosecondsPerSecond);
    if (daysOut != nullptr)
    {
        *daysOut = days;
    }
    return mloc_success;
}
