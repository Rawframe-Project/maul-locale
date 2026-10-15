// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic of date-times (maul-locale/zone.h, record mloc-0041):
// ECMA-262's AddDurationToDateTime, DifferencePlainDateTimeWithRounding
// through DifferenceISODateTime and RoundRelativeDuration, and
// RoundISODateTime.

#include "date_time.h"
#include "duration_round.h"
#include "time_arith.h"

#include "maul-locale/zone.h"

enum
{
    nanosecondsPerSecond = 1000000000,
};

static const int64_t s_nanosecondsPerDay = INT64_C(86400000000000);
static const uint32_t s_maxIncrement = 1000000000;
// A date duration's days, at most 2^53 seconds of them.
static const int64_t s_maxDays = INT64_C(104249991374);
// MaximumTemporalDurationRoundingIncrement of the time units, hour on.
static const uint32_t s_timeMaximum[6] = {24, 60, 60, 1000, 1000, 1000};

static int SignOf(int64_t v)
{
    return (v > 0) - (v < 0);
}

static int64_t NanosecondOfDay(const mlocTime* t)
{
    return ((int64_t)t->hour * 3600 + t->minute * 60 + t->second) * nanosecondsPerSecond +
           t->nanosecond;
}

static bool Usable(const mlocDateTime* d)
{
    return d != nullptr && mlocDateTimeValid(d) && mlocDateTimeWithinLimits(d);
}

static int DateSign(const mlocDateDuration* d)
{
    return d->years != 0    ? SignOf(d->years)
           : d->months != 0 ? SignOf(d->months)
           : d->weeks != 0  ? SignOf(d->weeks)
                            : SignOf(d->days);
}

mlocResult mlocAddToDateTime(const mlocDateTime* dateTime, const mlocDateTimeDuration* duration,
                             mlocOverflow overflow, mlocDateTime* dateTimeOut)
{
    if (!Usable(dateTime) || duration == nullptr || dateTimeOut == nullptr ||
        !mlocTimeDurationValid(&duration->time))
    {
        return mloc_errorInvalid;
    }
    int timeSign = duration->time.seconds != 0 ? SignOf(duration->time.seconds)
                                               : SignOf(duration->time.nanoseconds);
    if (timeSign * DateSign(&duration->date) < 0)
    {
        return mloc_errorInvalid;
    }
    mlocDateTime result;
    int64_t days = 0;
    mlocDateDuration date = duration->date;
    if (date.days > s_maxDays || date.days < -s_maxDays ||
        mlocAddToTime(&dateTime->time, &duration->time, &result.time, &days) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    // AdjustDateDurationRecord: the days passed join the duration's days.
    date.days += days;
    if (mlocAddToDate(&dateTime->date, &date, overflow, &result.date) != mloc_success ||
        !mlocDateTimeWithinLimits(&result))
    {
        return mloc_errorInvalid;
    }
    *dateTimeOut = result;
    return mloc_success;
}

static bool ValidRounding(const mlocDateTimeRounding* r)
{
    if ((uint32_t)r->largestUnit > mloc_unitNanosecond ||
        (uint32_t)r->smallestUnit > mloc_unitNanosecond || r->smallestUnit < r->largestUnit ||
        r->increment == 0 || r->increment > s_maxIncrement || (uint32_t)r->mode > mloc_roundTrunc)
    {
        return false;
    }
    if (r->smallestUnit < mloc_unitHour)
    {
        return true;
    }
    uint32_t maximum = s_timeMaximum[r->smallestUnit - mloc_unitHour];
    return r->increment < maximum && maximum % r->increment == 0;
}

// DifferenceISODateTime: the date difference to the second date, moved a
// day toward the first when the times run against the dates, and the
// time difference; a largest time unit takes the days into the time.
static bool Difference(const mlocDateTime* one, const mlocDateTime* two, mlocUnit largest,
                       RelativeDuration* out)
{
    int64_t time = NanosecondOfDay(&two->time) - NanosecondOfDay(&one->time);
    int timeSign = SignOf(time);
    int dateSign = SignOf((int64_t)one->date.day - two->date.day);
    mlocDate adjusted = two->date;
    if (timeSign == dateSign && timeSign != 0)
    {
        adjusted.day += timeSign;
        time -= timeSign * s_nanosecondsPerDay;
    }
    mlocDateUnit dateLargest = largest < mloc_unitDay ? (mlocDateUnit)largest : mloc_dateUnitDay;
    *out = (RelativeDuration){0};
    if (mlocDateUntil(&one->date, &adjusted, dateLargest, &out->date) != mloc_success)
    {
        return false;
    }
    int64_t seconds = time / nanosecondsPerSecond;
    if (largest > mloc_unitDay)
    {
        seconds += out->date.days * 86400;
        out->date.days = 0;
    }
    out->time = (mlocTimeDuration){seconds, (int32_t)(time % nanosecondsPerSecond)};
    return true;
}

mlocResult mlocDateTimeUntil(const mlocDateTime* one, const mlocDateTime* two,
                             const mlocDateTimeRounding* rounding,
                             mlocDateTimeDuration* durationOut)
{
    if (!Usable(one) || !Usable(two) || rounding == nullptr || durationOut == nullptr ||
        one->date.calendar != two->date.calendar || !ValidRounding(rounding))
    {
        return mloc_errorInvalid;
    }
    RelativeDuration d = {0};
    if (one->date.day == two->date.day &&
        NanosecondOfDay(&one->time) == NanosecondOfDay(&two->time))
    {
        *durationOut = (mlocDateTimeDuration){0};
        return mloc_success;
    }
    if (!Difference(one, two, rounding->largestUnit, &d))
    {
        return mloc_errorInvalid;
    }
    if (rounding->smallestUnit != mloc_unitNanosecond || rounding->increment != 1)
    {
        RelativeRounding relative = {(int)rounding->largestUnit, (int)rounding->smallestUnit,
                                     rounding->increment, rounding->mode};
        Moment dest = {two->date.day, NanosecondOfDay(&two->time)};
        if (!mlocRoundRelative(&d, &one->date, NanosecondOfDay(&one->time), dest, &relative))
        {
            return mloc_errorInvalid;
        }
    }
    *durationOut = (mlocDateTimeDuration){.date = d.date, .time = d.time};
    return mloc_success;
}

mlocResult mlocRoundDateTime(const mlocDateTime* dateTime, mlocTimeUnit unit, uint32_t increment,
                             mlocRoundingMode mode, mlocDateTime* dateTimeOut)
{
    mlocDateTime result;
    int64_t days = 0;
    if (!Usable(dateTime) || dateTimeOut == nullptr ||
        mlocRoundTime(&dateTime->time, unit, increment, mode, &result.time, &days) !=
            mloc_success ||
        mlocMakeDateFromDay((mlocCalendar)dateTime->date.calendar,
                            (int32_t)(dateTime->date.day + days), &result.date) != mloc_success ||
        !mlocDateTimeWithinLimits(&result))
    {
        return mloc_errorInvalid;
    }
    *dateTimeOut = result;
    return mloc_success;
}
