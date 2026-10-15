// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a difference of dates (maul-locale/calendar.h, record
// mloc-0040): Temporal's RoundRelativeDuration for two dates at midnight
// and no time zone, through NudgeToCalendarUnit (years, months, weeks),
// NudgeToDayOrTime (days) and BubbleRelativeDuration. Moments are days,
// and the progress between two multiples is compared as an exact
// fraction of days.

#include "calendar_year.h"
#include "rounding.h"

#include "maul-locale/calendar.h"

static const uint32_t s_maxIncrement = 1000000000;

static int SignOf(int64_t v)
{
    return (v > 0) - (v < 0);
}

// RoundNumberToIncrement with trunc: toward zero to a multiple.
static int64_t Truncated(int64_t value, int64_t increment)
{
    return value / increment * increment;
}

// The day a date duration reaches from a date, constrained.
static bool Reach(const mlocDate* date, const mlocDateDuration* d, int64_t* dayOut)
{
    mlocDate end = {0};
    if (mlocAddToDate(date, d, mloc_overflowConstrain, &end) != mloc_success)
    {
        return false;
    }
    *dayOut = end.day;
    return true;
}

// ComputeNudgeWindow: the multiples r1 and r2 of the unit around the
// duration (r1 shifted one increment on with shift), the durations with
// them in place, and the days they reach.
typedef struct Window
{
    int64_t r1;
    int64_t r2;
    mlocDateDuration start;
    mlocDateDuration end;
    int64_t startDay;
    int64_t endDay;
} Window;

static bool ComputeWindow(int sign, const mlocDateDuration* d, const mlocDate* one,
                          mlocDateUnit unit, int64_t increment, bool shift, Window* w)
{
    int64_t step = increment * sign;
    *w = (Window){0};
    if (unit == mloc_dateUnitYear)
    {
        w->r1 = Truncated(d->years, increment) + (shift ? step : 0);
        w->r2 = w->r1 + step;
        w->start = (mlocDateDuration){w->r1, 0, 0, 0};
        w->end = (mlocDateDuration){w->r2, 0, 0, 0};
    }
    else if (unit == mloc_dateUnitMonth)
    {
        w->r1 = Truncated(d->months, increment) + (shift ? step : 0);
        w->r2 = w->r1 + step;
        w->start = (mlocDateDuration){d->years, w->r1, 0, 0};
        w->end = (mlocDateDuration){d->years, w->r2, 0, 0};
    }
    else
    {
        // Weeks: the weeks the days hold from where the years and months
        // reach, counted on the calendar.
        mlocDateDuration yearsMonths = {d->years, d->months, 0, 0};
        int64_t weeksStart = 0;
        if (!Reach(one, &yearsMonths, &weeksStart))
        {
            return false;
        }
        int64_t weeks = d->weeks + d->days / 7;
        w->r1 = Truncated(weeks, increment);
        w->r2 = w->r1 + step;
        w->start = (mlocDateDuration){d->years, d->months, w->r1, 0};
        w->end = (mlocDateDuration){d->years, d->months, w->r2, 0};
    }
    // The first date itself when the start duration is zero (ECMA-262's
    // Temporal text; the proposal's r1 = 0 left the larger units out).
    w->startDay = one->day;
    bool zero = w->start.years == 0 && w->start.months == 0 && w->start.weeks == 0;
    if (!zero && !Reach(one, &w->start, &w->startDay))
    {
        return false;
    }
    return Reach(one, &w->end, &w->endDay);
}

static bool Between(int sign, int64_t start, int64_t value, int64_t end)
{
    return sign > 0 ? start <= value && value <= end : end <= value && value <= start;
}

// BubbleRelativeDuration: carries a rounding that reached the next value
// of a larger unit into it, up to the largest unit.
static bool Bubble(int sign, mlocDateDuration* d, int64_t nudged, const mlocDate* one,
                   mlocDateUnit largest, mlocDateUnit smallest)
{
    for (int unit = (int)smallest - 1; unit >= (int)largest; unit--)
    {
        if (unit == mloc_dateUnitWeek && largest != mloc_dateUnitWeek)
        {
            continue;
        }
        mlocDateDuration end = unit == mloc_dateUnitYear
                                   ? (mlocDateDuration){d->years + sign, 0, 0, 0}
                               : unit == mloc_dateUnitMonth
                                   ? (mlocDateDuration){d->years, d->months + sign, 0, 0}
                                   : (mlocDateDuration){d->years, d->months, d->weeks + sign, 0};
        int64_t endDay = 0;
        if (!Reach(one, &end, &endDay))
        {
            return false;
        }
        if (SignOf(nudged - endDay) == -sign)
        {
            break;
        }
        *d = end;
    }
    return true;
}

mlocResult mlocDateUntilRounded(const mlocDate* one, const mlocDate* two,
                                const mlocDateRounding* rounding, mlocDateDuration* durationOut)
{
    if (rounding == nullptr || durationOut == nullptr ||
        (uint32_t)rounding->largestUnit > mloc_dateUnitDay ||
        (uint32_t)rounding->smallestUnit > mloc_dateUnitDay ||
        rounding->smallestUnit < rounding->largestUnit || rounding->increment == 0 ||
        rounding->increment > s_maxIncrement || (uint32_t)rounding->mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    mlocDateDuration d = {0};
    mlocResult r = mlocDateUntil(one, two, rounding->largestUnit, &d);
    if (r != mloc_success || one->day == two->day ||
        (rounding->smallestUnit == mloc_dateUnitDay && rounding->increment == 1))
    {
        if (r == mloc_success)
        {
            *durationOut = d;
        }
        return r;
    }
    int sign = SignOf((int64_t)two->day - one->day);
    int64_t increment = rounding->increment;
    int64_t nudged = 0;
    bool expanded = false;
    if (rounding->smallestUnit == mloc_dateUnitDay)
    {
        // NudgeToDayOrTime: the days to a multiple of the increment.
        int64_t days = mlocRoundToIncrement(d.days, increment, rounding->mode);
        expanded = SignOf(days - d.days) == SignOf(d.days);
        nudged = two->day + (days - d.days);
        d.days = days;
    }
    else
    {
        // NudgeToCalendarUnit: the multiples around the duration, shifted
        // once when the second date lies past them.
        Window w;
        if (!ComputeWindow(sign, &d, one, rounding->smallestUnit, increment, false, &w))
        {
            return mloc_errorInvalid;
        }
        if (!Between(sign, w.startDay, two->day, w.endDay))
        {
            if (!ComputeWindow(sign, &d, one, rounding->smallestUnit, increment, true, &w))
            {
                return mloc_errorInvalid;
            }
            expanded = true;
        }
        int64_t part = sign * ((int64_t)two->day - w.startDay);
        int64_t whole = sign * (w.endDay - w.startDay);
        bool negative = sign < 0;
        bool evenLow = (w.r1 / increment) % 2 == 0;
        bool up =
            part == whole ||
            mlocRoundsUp(part, whole, mlocUnsignedRoundingMode(rounding->mode, negative), evenLow);
        expanded = expanded || up;
        d = up ? w.end : w.start;
        nudged = up ? w.endDay : w.startDay;
    }
    if (expanded && rounding->smallestUnit != mloc_dateUnitWeek &&
        !Bubble(sign, &d, nudged, one, rounding->largestUnit, rounding->smallestUnit))
    {
        return mloc_errorInvalid;
    }
    *durationOut = d;
    return mloc_success;
}
