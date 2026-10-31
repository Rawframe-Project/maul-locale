// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration's fields as a host keeps them, float64 integers, and its
// internal record (maul-locale/zone.h): ECMA-262's IsValidDuration with
// ToInternalDurationRecord, and TemporalDurationFromInternal with
// CreateTemporalDuration's check. The doubles are read and made from their
// bits; every sum is exact.

#include "duration_values.h"

#include "invariant.h"
#include "time_arith.h"

#include "maul-locale/zone.h"

enum
{
    fieldCount = 10,
    nanosecondsPerSecond = 1000000000,
};

static const uint64_t s_dateLimit = UINT64_C(4294967296);
// Each of days, hours, minutes and seconds alone below 2^53 seconds.
static const uint64_t s_timeLimit[4] = {UINT64_C(104249991374), UINT64_C(2501999792983),
                                        UINT64_C(150119987579016), UINT64_C(9007199254740991)};
static const uint64_t s_unitSeconds[4] = {86400, 3600, 60, 1};

static bool IsZero(Wide v)
{
    return v.high == 0 && v.low == 0;
}

// 2^53 seconds in nanoseconds, which a duration stays below.
static Wide Limit(void)
{
    return mlocWideProduct(UINT64_C(9007199254740992), nanosecondsPerSecond);
}

// The nanoseconds of days to seconds (each checked against its limit)
// with milliseconds, microseconds and nanoseconds, all of one sign; false
// when they reach 2^53 seconds.
static bool Total(const Wide* f, Wide* totalOut)
{
    uint64_t seconds = 0;
    for (int i = 0; i < 4; i += 1)
    {
        if (f[3 + i].high != 0 || f[3 + i].low > s_timeLimit[i])
        {
            return false;
        }
        seconds += f[3 + i].low * s_unitSeconds[i];
    }
    // Below 2^74 microseconds and 2^84 nanoseconds, nothing overflows.
    if (f[7].high != 0 || f[8].high >> 10 != 0 || f[9].high >> 20 != 0)
    {
        return false;
    }
    Wide total = mlocWideProduct(seconds, nanosecondsPerSecond);
    total = mlocWideSum(total, mlocWideProduct(f[7].low, 1000000));
    total = mlocWideSum(total, mlocWideScaled(f[8], 1000));
    total = mlocWideSum(total, f[9]);
    *totalOut = total;
    return mlocWideBelow(total, Limit());
}

// Reads the fields as magnitudes, their sign, and the time's total in
// nanoseconds, as IsValidDuration checks them; false for a duration it
// refuses.
static bool ReadValid(const mlocDurationValues* values, Wide* f, int* signOut, Wide* total)
{
    const double given[fieldCount] = {
        values->years,        values->months,     values->weeks,   values->days,
        values->hours,        values->minutes,    values->seconds, values->milliseconds,
        values->microseconds, values->nanoseconds};
    int sign = 0;
    for (int i = 0; i < fieldCount; i += 1)
    {
        bool negative = false;
        if (!mlocWideOfDouble(given[i], &negative, &f[i]))
        {
            return false;
        }
        int fieldSign = IsZero(f[i]) ? 0 : negative ? -1 : 1;
        if (fieldSign != 0 && sign != 0 && fieldSign != sign)
        {
            return false;
        }
        sign = sign != 0 ? sign : fieldSign;
    }
    for (int i = 0; i < 3; i += 1)
    {
        if (f[i].high != 0 || f[i].low >= s_dateLimit)
        {
            return false;
        }
    }
    *signOut = sign;
    return Total(f, total);
}

bool mlocReadValidDuration(const mlocDurationValues* values, Wide* magnitudes, bool* negativeOut)
{
    int sign = 0;
    Wide total;
    bool valid = values != nullptr && ReadValid(values, magnitudes, &sign, &total);
    *negativeOut = sign < 0;
    return valid;
}

mlocResult mlocMakeDuration(const mlocDurationValues* values, mlocDateTimeDuration* durationOut)
{
    Wide f[fieldCount];
    int sign = 0;
    Wide total;
    if (values == nullptr || durationOut == nullptr || !ReadValid(values, f, &sign, &total))
    {
        return mloc_errorInvalid;
    }
    // TimeDurationFromComponents of hours to nanoseconds: the days stay
    // in the date part.
    Wide days = mlocWideProduct(f[3].low * 86400, nanosecondsPerSecond);
    // The total counts the days, and stays below 2^53 seconds.
    MLOC_ASSERT(!mlocWideBelow(total, days));
    Wide remainder;
    uint64_t seconds = mlocWideQuotient(
        (Wide){total.high - days.high - (total.low < days.low ? 1 : 0), total.low - days.low},
        (Wide){0, nanosecondsPerSecond}, &remainder);
    MLOC_ASSERT(seconds < UINT64_C(9007199254740992) && remainder.low < nanosecondsPerSecond);
    int64_t s = sign < 0 ? -1 : 1;
    *durationOut = (mlocDateTimeDuration){
        .date = {s * (int64_t)f[0].low, s * (int64_t)f[1].low, s * (int64_t)f[2].low,
                 s * (int64_t)f[3].low},
        .time = {s * (int64_t)seconds, (int32_t)(s * (int64_t)remainder.low)}};
    return mloc_success;
}

static uint64_t Magnitude(int64_t v)
{
    return v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
}

static int SignOf(int64_t v)
{
    return (v > 0) - (v < 0);
}

// A field as Temporal keeps it: the float64 nearest it, read back exactly.
static Wide Rounded(Wide v)
{
    bool negative = false;
    Wide out = {0, 0};
    // A field balanced from a valid time (below 2^53 seconds) is below
    // 2^84, and its double is an integer the reading takes.
    bool read = mlocWideOfDouble(mlocBinary64Of(false, v, (Wide){0, 1}), &negative, &out);
    MLOC_ASSERT(read);
    (void)read;
    return out;
}

// The record's one sign, or false for two.
static bool RecordSign(const mlocDateTimeDuration* duration, int* signOut)
{
    const mlocDateDuration* date = &duration->date;
    int64_t fields[5] = {date->years, date->months, date->weeks, date->days,
                         duration->time.seconds != 0 ? duration->time.seconds
                                                     : duration->time.nanoseconds};
    int sign = 0;
    for (int i = 0; i < 5; i += 1)
    {
        int fieldSign = SignOf(fields[i]);
        if (fieldSign != 0 && sign != 0 && fieldSign != sign)
        {
            return false;
        }
        sign = sign != 0 ? sign : fieldSign;
    }
    *signOut = sign;
    return true;
}

// The time part's magnitude, s seconds and n nanoseconds, balanced up to
// the largest unit into the fields of hours on, and into the days for a
// date unit.
static void Balance(uint64_t s, uint64_t n, mlocUnit largestUnit, Wide* f)
{
    f[7] = (Wide){0, n / 1000000};
    f[8] = (Wide){0, n / 1000 % 1000};
    f[9] = (Wide){0, n % 1000};
    if (largestUnit <= mloc_unitDay)
    {
        f[3].low += s / 86400;
        f[4].low = s / 3600 % 24;
        f[5].low = s / 60 % 60;
        f[6].low = s % 60;
    }
    else if (largestUnit == mloc_unitHour)
    {
        f[4].low = s / 3600;
        f[5].low = s / 60 % 60;
        f[6].low = s % 60;
    }
    else if (largestUnit == mloc_unitMinute)
    {
        f[5].low = s / 60;
        f[6].low = s % 60;
    }
    else if (largestUnit == mloc_unitSecond)
    {
        f[6].low = s;
    }
    else if (largestUnit == mloc_unitMillisecond)
    {
        f[7].low = s * 1000 + n / 1000000;
    }
    else if (largestUnit == mloc_unitMicrosecond)
    {
        f[7].low = 0;
        f[8] = mlocWideSum(mlocWideProduct(s, 1000000), (Wide){0, n / 1000});
    }
    else
    {
        f[7].low = 0;
        f[8].low = 0;
        f[9] = mlocWideSum(mlocWideProduct(s, nanosecondsPerSecond), (Wide){0, n});
    }
}

mlocResult mlocGetDurationValues(const mlocDateTimeDuration* duration, mlocUnit largestUnit,
                                 mlocDurationValues* valuesOut)
{
    int sign = 0;
    if (duration == nullptr || valuesOut == nullptr || !mlocTimeDurationValid(&duration->time) ||
        (uint32_t)largestUnit > mloc_unitNanosecond || !RecordSign(duration, &sign))
    {
        return mloc_errorInvalid;
    }
    const mlocDateDuration* date = &duration->date;
    Wide f[fieldCount] = {{0, Magnitude(date->years)},
                          {0, Magnitude(date->months)},
                          {0, Magnitude(date->weeks)},
                          {0, Magnitude(date->days)}};
    Balance(Magnitude(duration->time.seconds), Magnitude(duration->time.nanoseconds), largestUnit,
            f);
    // CreateTemporalDuration's IsValidDuration, on the float64 values of
    // the sub-second fields.
    Wide checked[fieldCount];
    for (int i = 0; i < fieldCount; i += 1)
    {
        checked[i] = i >= 7 ? Rounded(f[i]) : f[i];
    }
    Wide total;
    if (f[0].low >= s_dateLimit || f[1].low >= s_dateLimit || f[2].low >= s_dateLimit ||
        !Total(checked, &total))
    {
        return mloc_errorInvalid;
    }
    double out[fieldCount];
    for (int i = 0; i < fieldCount; i += 1)
    {
        out[i] = mlocBinary64Of(sign < 0, f[i], (Wide){0, 1});
    }
    *valuesOut = (mlocDurationValues){out[0], out[1], out[2], out[3], out[4],
                                      out[5], out[6], out[7], out[8], out[9]};
    return mloc_success;
}
