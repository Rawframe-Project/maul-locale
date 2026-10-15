// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a difference relative to where it starts (duration_round.h).

#include "duration_round.h"

#include "rounding.h"

enum
{
    nanosecondsPerSecond = 1000000000,
    secondsPerDay = 86400,
};

static const int64_t s_nanosecondsPerDay = INT64_C(86400000000000);
static const int64_t s_maxSeconds = INT64_C(9007199254740992);

// Each time unit's length: whole seconds, or nanoseconds below a second.
static const int64_t s_unitSeconds[10] = {0, 0, 0, secondsPerDay, 3600, 60, 1, 0, 0, 0};
static const int64_t s_unitNanoseconds[10] = {0, 0, 0, 0, 0, 0, 0, 1000000, 1000, 1};

static int SignOf(int64_t v)
{
    return (v > 0) - (v < 0);
}

static int64_t Truncated(int64_t value, int64_t increment)
{
    return value / increment * increment;
}

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b < 0 ? 1 : 0);
}

static int Compare(Moment a, Moment b)
{
    return a.day != b.day ? SignOf(a.day - b.day) : SignOf(a.nanosecond - b.nanosecond);
}

// A moment moved by a time duration.
static Moment Plus(Moment m, mlocTimeDuration d)
{
    int64_t nanosecond =
        m.nanosecond + d.seconds % secondsPerDay * nanosecondsPerSecond + d.nanoseconds;
    int64_t carry = FloorDiv(nanosecond, s_nanosecondsPerDay);
    return (Moment){m.day + d.seconds / secondsPerDay + carry,
                    nanosecond - carry * s_nanosecondsPerDay};
}

// A sum of seconds and nanoseconds of any signs, |nanoseconds| below
// 2 * 10^9, as a duration of one sign.
static mlocTimeDuration Normalized(int64_t seconds, int64_t nanoseconds)
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
    return (mlocTimeDuration){seconds, (int32_t)nanoseconds};
}

static int Sign(mlocTimeDuration d)
{
    return d.seconds != 0 ? SignOf(d.seconds) : SignOf(d.nanoseconds);
}

// Whether a magnitude part of a step rounds up: part and whole as
// seconds and nanoseconds compared through twice the part.
static bool UpBySeconds(int64_t partSeconds, int64_t partNanoseconds, int64_t wholeSeconds,
                        RoundingUnsigned mode, bool evenLow)
{
    if (partSeconds == 0 && partNanoseconds == 0)
    {
        return false;
    }
    if (mode == roundingZero || mode == roundingInfinity)
    {
        return mode == roundingInfinity;
    }
    int64_t twiceNanoseconds = 2 * partNanoseconds;
    int64_t twiceSeconds = 2 * partSeconds + twiceNanoseconds / nanosecondsPerSecond;
    int order = twiceSeconds != wholeSeconds ? SignOf(twiceSeconds - wholeSeconds)
                                             : SignOf(twiceNanoseconds % nanosecondsPerSecond);
    if (order != 0)
    {
        return order > 0;
    }
    return mode == roundingHalfInfinity || (mode == roundingHalfEven && !evenLow);
}

bool mlocRoundTimeDuration(mlocTimeDuration value, int64_t stepSeconds, int64_t stepNanoseconds,
                           mlocRoundingMode mode, mlocTimeDuration* out)
{
    bool negative = value.seconds < 0 || value.nanoseconds < 0;
    int64_t s = negative ? -value.seconds : value.seconds;
    int64_t n = negative ? -value.nanoseconds : value.nanoseconds;
    RoundingUnsigned unsignedMode = mlocUnsignedRoundingMode(mode, negative);
    if (stepSeconds > 0)
    {
        int64_t low = s / stepSeconds;
        bool up = UpBySeconds(s % stepSeconds, n, stepSeconds, unsignedMode, low % 2 == 0);
        s = (low + (up ? 1 : 0)) * stepSeconds;
        n = 0;
    }
    else
    {
        // The step divides a second, so the seconds are whole steps: the
        // parity of the count of steps takes theirs when a second holds an
        // odd number of steps.
        int64_t low = n / stepNanoseconds;
        int64_t perSecond = nanosecondsPerSecond / stepNanoseconds;
        bool evenLow = ((s % 2) * (perSecond % 2) + low) % 2 == 0;
        bool up = mlocRoundsUp(n % stepNanoseconds, stepNanoseconds, unsignedMode, evenLow);
        n = (low + (up ? 1 : 0)) * stepNanoseconds;
        s += n / nanosecondsPerSecond;
        n %= nanosecondsPerSecond;
    }
    if (s >= s_maxSeconds)
    {
        return false;
    }
    *out = negative ? (mlocTimeDuration){-s, (int32_t)-n} : (mlocTimeDuration){s, (int32_t)n};
    return true;
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

static bool ComputeWindow(int sign, const mlocDateDuration* d, const mlocDate* one, int unit,
                          int64_t increment, bool shift, Window* w)
{
    int64_t step = increment * sign;
    *w = (Window){0};
    if (unit == unitYear)
    {
        w->r1 = Truncated(d->years, increment) + (shift ? step : 0);
        w->r2 = w->r1 + step;
        w->start = (mlocDateDuration){w->r1, 0, 0, 0};
        w->end = (mlocDateDuration){w->r2, 0, 0, 0};
    }
    else if (unit == unitMonth)
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

// Whether a part of a window rounds up: part is partDays days and
// partNanoseconds (|partNanoseconds| under a day, of any sign), whole is
// wholeDays days, 0 <= part < whole. Twice the part is compared with the
// whole by days first, exactly.
static bool UpByDays(int64_t partDays, int64_t partNanoseconds, int64_t wholeDays,
                     RoundingUnsigned mode, bool evenLow)
{
    if (partDays == 0 && partNanoseconds == 0)
    {
        return false;
    }
    if (mode == roundingZero || mode == roundingInfinity)
    {
        return mode == roundingInfinity;
    }
    int64_t k = 2 * partDays - wholeDays;
    int order = k >= 2 ? 1 : k <= -2 ? -1 : SignOf(k * s_nanosecondsPerDay + 2 * partNanoseconds);
    if (order != 0)
    {
        return order > 0;
    }
    return mode == roundingHalfInfinity || (mode == roundingHalfEven && !evenLow);
}

// The result of a nudge: the duration, the moment it reaches, and
// whether it reached the next value of its unit.
typedef struct Nudge
{
    RelativeDuration duration;
    Moment nudged;
    bool expanded;
} Nudge;

// NudgeToCalendarUnit: the multiples around the duration, shifted once
// when the destination lies past them, and the destination's place
// between them deciding by mode.
static bool NudgeToCalendarUnit(int sign, const RelativeDuration* d, const mlocDate* one,
                                int64_t start, Moment dest, const RelativeRounding* r, Nudge* out)
{
    Window w;
    if (!ComputeWindow(sign, &d->date, one, r->smallestUnit, r->increment, false, &w))
    {
        return false;
    }
    bool expanded = false;
    Moment low = {sign > 0 ? w.startDay : w.endDay, start};
    Moment high = {sign > 0 ? w.endDay : w.startDay, start};
    if (Compare(low, dest) > 0 || Compare(dest, high) > 0)
    {
        if (!ComputeWindow(sign, &d->date, one, r->smallestUnit, r->increment, true, &w))
        {
            return false;
        }
        expanded = true;
    }
    int64_t partDays = sign * (dest.day - w.startDay);
    int64_t partNanoseconds = sign * (dest.nanosecond - start);
    int64_t wholeDays = sign * (w.endDay - w.startDay);
    bool atEnd = dest.day == w.endDay && dest.nanosecond == start;
    bool evenLow = (w.r1 / r->increment) % 2 == 0;
    bool up = atEnd || UpByDays(partDays, partNanoseconds, wholeDays,
                                mlocUnsignedRoundingMode(r->mode, sign < 0), evenLow);
    *out = (Nudge){.duration = {.date = up ? w.end : w.start},
                   .nudged = {up ? w.endDay : w.startDay, start},
                   .expanded = expanded || up};
    return true;
}

// NudgeToDayOrTime: the days and time together rounded as a time
// duration, the days split off again when the largest unit is a date
// unit.
static bool NudgeToDayOrTime(const RelativeDuration* d, Moment dest, const RelativeRounding* r,
                             Nudge* out)
{
    mlocTimeDuration total =
        Normalized(d->date.days * secondsPerDay + d->time.seconds, d->time.nanoseconds);
    mlocTimeDuration rounded;
    if (!mlocRoundTimeDuration(total, s_unitSeconds[r->smallestUnit] * r->increment,
                               s_unitNanoseconds[r->smallestUnit] * r->increment, r->mode,
                               &rounded))
    {
        return false;
    }
    mlocTimeDuration difference =
        Normalized(rounded.seconds - total.seconds, rounded.nanoseconds - total.nanoseconds);
    int64_t wholeDays = total.seconds / secondsPerDay;
    int64_t roundedDays = rounded.seconds / secondsPerDay;
    RelativeDuration result = {.date = d->date, .time = rounded};
    result.date.days = 0;
    if (r->largestUnit <= unitDay)
    {
        result.date.days = roundedDays;
        result.time =
            Normalized(rounded.seconds - roundedDays * secondsPerDay, rounded.nanoseconds);
    }
    *out = (Nudge){.duration = result,
                   .nudged = Plus(dest, difference),
                   .expanded = SignOf(roundedDays - wholeDays) == Sign(total)};
    return true;
}

// BubbleRelativeDuration: carries a rounding that reached the next value
// of a larger unit into it, up to the largest unit.
static bool Bubble(int sign, RelativeDuration* d, Moment nudged, const mlocDate* one, int64_t start,
                   int largest, int smallest)
{
    for (int unit = smallest - 1; unit >= largest; unit--)
    {
        if (unit == unitWeek && largest != unitWeek)
        {
            continue;
        }
        const mlocDateDuration* t = &d->date;
        mlocDateDuration end = unit == unitYear ? (mlocDateDuration){t->years + sign, 0, 0, 0}
                               : unit == unitMonth
                                   ? (mlocDateDuration){t->years, t->months + sign, 0, 0}
                                   : (mlocDateDuration){t->years, t->months, t->weeks + sign, 0};
        int64_t endDay = 0;
        if (!Reach(one, &end, &endDay))
        {
            return false;
        }
        if (Compare(nudged, (Moment){endDay, start}) == -sign)
        {
            break;
        }
        *d = (RelativeDuration){.date = end};
    }
    return true;
}

static int DurationSign(const RelativeDuration* d)
{
    const mlocDateDuration* t = &d->date;
    int date = t->years != 0    ? SignOf(t->years)
               : t->months != 0 ? SignOf(t->months)
               : t->weeks != 0  ? SignOf(t->weeks)
                                : SignOf(t->days);
    return date != 0 ? date : Sign(d->time);
}

bool mlocRoundRelative(RelativeDuration* duration, const mlocDate* one, int64_t startNanosecond,
                       Moment dest, const RelativeRounding* rounding)
{
    int sign = DurationSign(duration) < 0 ? -1 : 1;
    Nudge nudge;
    bool done =
        rounding->smallestUnit < unitDay
            ? NudgeToCalendarUnit(sign, duration, one, startNanosecond, dest, rounding, &nudge)
            : NudgeToDayOrTime(duration, dest, rounding, &nudge);
    if (!done)
    {
        return false;
    }
    *duration = nudge.duration;
    if (nudge.expanded && rounding->smallestUnit != unitWeek)
    {
        int startUnit = rounding->smallestUnit < unitDay ? rounding->smallestUnit : unitDay;
        return Bubble(sign, duration, nudge.nudged, one, startNanosecond, rounding->largestUnit,
                      startUnit);
    }
    return true;
}
