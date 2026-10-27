// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a difference relative to where it starts (duration_round.h).

#include "duration_round.h"

#include "binary64.h"
#include "date_time.h"
#include "rounding.h"

enum
{
    nanosecondsPerSecond = 1000000000,
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

bool mlocRoundTimeStep(mlocTimeDuration value, int64_t stepSeconds, int64_t stepNanoseconds,
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

Moment mlocMomentOf(const mlocInstant* instant)
{
    int64_t day = FloorDiv(instant->seconds, secondsPerDay);
    return (Moment){day + epochDay,
                    (instant->seconds - day * secondsPerDay) * nanosecondsPerSecond +
                        instant->nanoseconds};
}

// The moment a day reaches at the start's time of day: without a zone
// the wall clock, with one its compatible instant (GetEpochNanosecondsFor).
static bool At(const RelativeStart* s, int64_t day, Moment* out)
{
    if (s->zone == nullptr)
    {
        *out = (Moment){day, s->nanosecond};
        return true;
    }
    int64_t second = s->nanosecond / nanosecondsPerSecond;
    mlocTime time = {(uint8_t)(second / 3600), (uint8_t)(second / 60 % 60), (uint8_t)(second % 60),
                     (uint32_t)(s->nanosecond % nanosecondsPerSecond)};
    mlocDateTime dateTime = {{0}, time};
    mlocInstant instant;
    if (day < INT32_MIN || day > INT32_MAX ||
        mlocMakeDateFromDay(mloc_calendarGregorian, (int32_t)day, &dateTime.date) != mloc_success ||
        mlocGetInstantFor(s->zone, &dateTime, mloc_disambiguationCompatible, &instant) !=
            mloc_success)
    {
        return false;
    }
    *out = mlocMomentOf(&instant);
    return true;
}

// The time duration from one moment to another.
static mlocTimeDuration Between(Moment from, Moment to)
{
    return Normalized((to.day - from.day) * secondsPerDay +
                          (to.nanosecond - from.nanosecond) / nanosecondsPerSecond,
                      (to.nanosecond - from.nanosecond) % nanosecondsPerSecond);
}

// ComputeNudgeWindow: the multiples r1 and r2 of the unit around the
// duration (r1 shifted one increment on with shift), the durations with
// them in place, and the moments they reach.
typedef struct Window
{
    int64_t r1;
    int64_t r2;
    mlocDateDuration start;
    mlocDateDuration end;
    Moment startMoment;
    Moment endMoment;
} Window;

static bool ComputeWindow(int sign, const mlocDateDuration* d, const RelativeStart* s, int unit,
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
    else if (unit == unitWeek)
    {
        // Weeks: the weeks the days hold from where the years and months
        // reach, which every calendar counts as seven days (that date lies
        // between the two, so reaching it cannot fail).
        int64_t weeks = d->weeks + d->days / 7;
        w->r1 = Truncated(weeks, increment);
        w->r2 = w->r1 + step;
        w->start = (mlocDateDuration){d->years, d->months, w->r1, 0};
        w->end = (mlocDateDuration){d->years, d->months, w->r2, 0};
    }
    else
    {
        // Days, which only a zone makes irregular.
        w->r1 = Truncated(d->days, increment);
        w->r2 = w->r1 + step;
        w->start = (mlocDateDuration){d->years, d->months, d->weeks, w->r1};
        w->end = (mlocDateDuration){d->years, d->months, d->weeks, w->r2};
    }
    // The first moment itself when the start duration is zero (ECMA-262's
    // Temporal text; the proposal's r1 = 0 left the larger units out).
    w->startMoment = s->origin;
    bool zero =
        w->start.years == 0 && w->start.months == 0 && w->start.weeks == 0 && w->start.days == 0;
    int64_t day = 0;
    if (!zero && !(Reach(s->date, &w->start, &day) && At(s, day, &w->startMoment)))
    {
        return false;
    }
    return Reach(s->date, &w->end, &day) && At(s, day, &w->endMoment);
}

// Whether a part of a window rounds up: part and whole are days and
// nanoseconds (each under a day, of any sign), 0 <= part < whole. Twice
// the part is compared with the whole by days first, exactly.
static bool UpByDays(int64_t partDays, int64_t partNanoseconds, int64_t wholeDays,
                     int64_t wholeNanoseconds, RoundingUnsigned mode, bool evenLow)
{
    if (partDays == 0 && partNanoseconds == 0)
    {
        return false;
    }
    if (mode == roundingZero || mode == roundingInfinity)
    {
        return mode == roundingInfinity;
    }
    // The nanoseconds of the difference lie within three days.
    int64_t k = 2 * partDays - wholeDays;
    int order = k >= 3 ? 1
                : k <= -3
                    ? -1
                    : SignOf(k * s_nanosecondsPerDay + 2 * partNanoseconds - wholeNanoseconds);
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
// The window holding the destination: the first, or the next when the
// destination lies past it (expanded).
static bool FindWindow(int sign, const RelativeDuration* d, const RelativeStart* s, Moment dest,
                       int unit, int64_t increment, Window* w, bool* expandedOut)
{
    if (!ComputeWindow(sign, &d->date, s, unit, increment, false, w))
    {
        return false;
    }
    *expandedOut = false;
    Moment low = sign > 0 ? w->startMoment : w->endMoment;
    Moment high = sign > 0 ? w->endMoment : w->startMoment;
    if (Compare(low, dest) > 0 || Compare(dest, high) > 0)
    {
        *expandedOut = true;
        return ComputeWindow(sign, &d->date, s, unit, increment, true, w);
    }
    return true;
}

static bool NudgeToCalendarUnit(int sign, const RelativeDuration* d, const RelativeStart* s,
                                Moment dest, const RelativeRounding* r, Nudge* out)
{
    Window w;
    bool expanded = false;
    if (!FindWindow(sign, d, s, dest, r->smallestUnit, r->increment, &w, &expanded))
    {
        return false;
    }
    int64_t partDays = sign * (dest.day - w.startMoment.day);
    int64_t partNanoseconds = sign * (dest.nanosecond - w.startMoment.nanosecond);
    int64_t wholeDays = sign * (w.endMoment.day - w.startMoment.day);
    int64_t wholeNanoseconds = sign * (w.endMoment.nanosecond - w.startMoment.nanosecond);
    bool atEnd = Compare(dest, w.endMoment) == 0;
    bool evenLow = (w.r1 / r->increment) % 2 == 0;
    bool up = atEnd || UpByDays(partDays, partNanoseconds, wholeDays, wholeNanoseconds,
                                mlocUnsignedRoundingMode(r->mode, sign < 0), evenLow);
    *out = (Nudge){.duration = {.date = up ? w.end : w.start},
                   .nudged = up ? w.endMoment : w.startMoment,
                   .expanded = expanded || up};
    return true;
}

// NudgeToZonedTime: the time part rounded within the day the date part
// reaches, and past that day's end into the next when it rounds beyond
// it; a day's length is the zone's.
static bool NudgeToZonedTime(int sign, const RelativeDuration* d, const RelativeStart* s,
                             const RelativeRounding* r, Nudge* out)
{
    int64_t startDay = 0;
    Moment start;
    Moment end;
    if (!Reach(s->date, &d->date, &startDay) || !At(s, startDay, &start) ||
        !At(s, startDay + sign, &end))
    {
        return false;
    }
    mlocTimeDuration daySpan = Between(start, end);
    int64_t stepSeconds = s_unitSeconds[r->smallestUnit] * r->increment;
    int64_t stepNanoseconds = s_unitNanoseconds[r->smallestUnit] * r->increment;
    mlocTimeDuration rounded;
    if (!mlocRoundTimeStep(d->time, stepSeconds, stepNanoseconds, r->mode, &rounded))
    {
        return false;
    }
    mlocTimeDuration beyond =
        Normalized(rounded.seconds - daySpan.seconds, rounded.nanoseconds - daySpan.nanoseconds);
    bool beyondDay = Sign(beyond) != -sign;
    Moment nudged = start;
    if (beyondDay)
    {
        if (!mlocRoundTimeStep(beyond, stepSeconds, stepNanoseconds, r->mode, &rounded))
        {
            return false;
        }
        nudged = end;
    }
    RelativeDuration result = {.date = d->date, .time = rounded};
    result.date.days += beyondDay ? sign : 0;
    *out = (Nudge){.duration = result, .nudged = Plus(nudged, rounded), .expanded = beyondDay};
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
    if (!mlocRoundTimeStep(total, s_unitSeconds[r->smallestUnit] * r->increment,
                           s_unitNanoseconds[r->smallestUnit] * r->increment, r->mode, &rounded))
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
static bool Bubble(int sign, RelativeDuration* d, Moment nudged, const RelativeStart* s,
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
        Moment endMoment;
        if (!Reach(s->date, &end, &endDay) || !At(s, endDay, &endMoment))
        {
            return false;
        }
        if (Compare(nudged, endMoment) == -sign)
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

bool mlocRoundRelative(RelativeDuration* duration, const RelativeStart* start, Moment dest,
                       const RelativeRounding* rounding)
{
    int sign = DurationSign(duration) < 0 ? -1 : 1;
    Nudge nudge;
    bool irregular = rounding->smallestUnit < unitDay ||
                     (start->zone != nullptr && rounding->smallestUnit == unitDay);
    bool done = irregular ? NudgeToCalendarUnit(sign, duration, start, dest, rounding, &nudge)
                : start->zone != nullptr ? NudgeToZonedTime(sign, duration, start, rounding, &nudge)
                                         : NudgeToDayOrTime(duration, dest, rounding, &nudge);
    if (!done)
    {
        return false;
    }
    *duration = nudge.duration;
    if (nudge.expanded && rounding->smallestUnit != unitWeek)
    {
        int startUnit = rounding->smallestUnit < unitDay ? rounding->smallestUnit : unitDay;
        return Bubble(sign, duration, nudge.nudged, start, rounding->largestUnit, startUnit);
    }
    return true;
}

// The nanoseconds from one moment to another, which lie within a few
// hundred days here.
static int64_t NanosecondsBetween(Moment from, Moment to)
{
    return (to.day - from.day) * s_nanosecondsPerDay + to.nanosecond - from.nanosecond;
}

bool mlocTotalRelative(const RelativeDuration* duration, const RelativeStart* start, Moment dest,
                       int unit, double* totalOut)
{
    if (unit < unitDay || (start->zone != nullptr && unit == unitDay))
    {
        // NudgeToCalendarUnit's total with an increment of 1: the
        // multiple before, and the destination's place in the window.
        int sign = DurationSign(duration) < 0 ? -1 : 1;
        Window w;
        bool expanded = false;
        if (!FindWindow(sign, duration, start, dest, unit, 1, &w, &expanded))
        {
            return false;
        }
        uint64_t part = (uint64_t)(sign * NanosecondsBetween(w.startMoment, dest));
        uint64_t whole = (uint64_t)(sign * NanosecondsBetween(w.startMoment, w.endMoment));
        uint64_t below = (uint64_t)(w.r1 < 0 ? -w.r1 : w.r1);
        Wide numerator = mlocWideSum(mlocWideProduct(below, whole), (Wide){0, part});
        *totalOut = mlocBinary64Of(sign < 0, numerator, (Wide){0, whole});
        return true;
    }
    // A time unit, or days without a zone: the days as 24 hours.
    mlocTimeDuration time = Normalized(duration->date.days * secondsPerDay + duration->time.seconds,
                                       duration->time.nanoseconds);
    *totalOut = mlocTotalOfTime(&time, s_unitSeconds[unit] * nanosecondsPerSecond +
                                           s_unitNanoseconds[unit]);
    return true;
}

double mlocTotalOfTime(const mlocTimeDuration* time, int64_t unitNanoseconds)
{
    bool negative = time->seconds < 0 || time->nanoseconds < 0;
    uint64_t seconds = (uint64_t)(negative ? -time->seconds : time->seconds);
    uint64_t nanoseconds = (uint64_t)(negative ? -time->nanoseconds : time->nanoseconds);
    Wide numerator =
        mlocWideSum(mlocWideProduct(seconds, nanosecondsPerSecond), (Wide){0, nanoseconds});
    return mlocBinary64Of(negative, numerator, (Wide){0, (uint64_t)unitNanoseconds});
}
