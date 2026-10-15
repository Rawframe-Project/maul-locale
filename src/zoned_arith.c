// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic of zoned date-times (maul-locale/zone.h, record
// mloc-0041): ECMA-262's InterpretISODateTimeOffset, AddZonedDateTime,
// DifferenceZonedDateTimeWithRounding through DifferenceZonedDateTime and
// RoundRelativeDuration with the zone, and Temporal.ZonedDateTime's
// round. A zoned date-time is its instant with the zone and calendar
// given beside it.

#include "date_time.h"
#include "duration_round.h"
#include "rounding.h"
#include "time_arith.h"
#include "zone_handle.h"

#include "maul-locale/zone.h"

enum
{
    nanosecondsPerSecond = 1000000000,
};

static const int64_t s_nanosecondsPerDay = INT64_C(86400000000000);
static const int64_t s_nanosecondsPerMinute = INT64_C(60000000000);
// A date duration's days, at most 2^53 seconds of them.
static const int64_t s_maxDays = INT64_C(104249991374);
// CheckISODaysRange: 10^8 days either side of 1970.
static const int64_t s_maxEpochDays = 100000000;

static int SignOf(int64_t v)
{
    return (v > 0) - (v < 0);
}

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b < 0 ? 1 : 0);
}

static int TimeSign(const mlocTimeDuration* d)
{
    return d->seconds != 0 ? SignOf(d->seconds) : SignOf(d->nanoseconds);
}

static int DateSign(const mlocDateDuration* d)
{
    return d->years != 0    ? SignOf(d->years)
           : d->months != 0 ? SignOf(d->months)
           : d->weeks != 0  ? SignOf(d->weeks)
                            : SignOf(d->days);
}

static int64_t NanosecondOfDay(const mlocTime* t)
{
    return ((int64_t)t->hour * 3600 + t->minute * 60 + t->second) * nanosecondsPerSecond +
           t->nanosecond;
}

// The time duration from one instant to another, of one sign.
static mlocTimeDuration Between(const mlocInstant* from, const mlocInstant* to)
{
    int64_t seconds = to->seconds - from->seconds;
    int64_t nanoseconds = to->nanoseconds - from->nanoseconds;
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

static int CompareInstants(const mlocInstant* a, const mlocInstant* b)
{
    return a->seconds != b->seconds ? SignOf(a->seconds - b->seconds)
                                    : SignOf(a->nanoseconds - b->nanoseconds);
}

// The offset of a candidate instant for a local date-time, in
// nanoseconds: the local time read as universal time less the instant.
static int64_t CandidateOffset(const mlocDateTime* dateTime, const mlocInstant* candidate)
{
    return (mlocDateTimeSeconds(dateTime) - candidate->seconds) * nanosecondsPerSecond +
           (int64_t)dateTime->time.nanosecond - candidate->nanoseconds;
}

mlocResult mlocGetInstantForOffset(const mlocTimeZone* zone, const mlocDateTime* dateTime,
                                   int64_t offsetNanoseconds, mlocOffsetOption option,
                                   mlocDisambiguation disambiguation, bool matchMinutes,
                                   mlocInstant* instantOut)
{
    if (dateTime == nullptr || instantOut == nullptr || !mlocDateTimeValid(dateTime) ||
        offsetNanoseconds <= -s_nanosecondsPerDay || offsetNanoseconds >= s_nanosecondsPerDay ||
        (uint32_t)option > mloc_offsetReject ||
        (uint32_t)disambiguation > mloc_disambiguationReject)
    {
        return mloc_errorInvalid;
    }
    if (option == mloc_offsetIgnore)
    {
        return mlocGetInstantFor(zone, dateTime, disambiguation, instantOut);
    }
    ZoneTables t;
    bool named = false;
    if (!mlocReadTimeZone(zone, &t, &named))
    {
        return mloc_errorInvalid;
    }
    if (option == mloc_offsetUse)
    {
        // BalanceISODateTime with the offset taken off, CheckISODaysRange
        // on the balanced date, then IsValidEpochNanoseconds.
        int64_t nanoseconds = (int64_t)dateTime->time.nanosecond - offsetNanoseconds;
        int64_t carry = FloorDiv(nanoseconds, nanosecondsPerSecond);
        mlocInstant instant = {mlocDateTimeSeconds(dateTime) + carry,
                               (int32_t)(nanoseconds - carry * nanosecondsPerSecond)};
        int64_t day = FloorDiv(instant.seconds, secondsPerDay);
        if (day < -s_maxEpochDays || day > s_maxEpochDays || !mlocValidInstant(&instant))
        {
            return mloc_errorInvalid;
        }
        *instantOut = instant;
        return mloc_success;
    }
    int64_t day = (int64_t)dateTime->date.day - epochDay;
    mlocPossibleInstants possible;
    if (day < -s_maxEpochDays || day > s_maxEpochDays ||
        mlocGetPossibleInstants(zone, dateTime, &possible) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    for (uint32_t i = 0; i < possible.count; i += 1)
    {
        const mlocInstant* candidate = i == 0 ? &possible.earliest : &possible.latest;
        int64_t offset = CandidateOffset(dateTime, candidate);
        if (offset == offsetNanoseconds ||
            (matchMinutes && mlocRoundToIncrement(offset, s_nanosecondsPerMinute,
                                                  mloc_roundHalfExpand) == offsetNanoseconds))
        {
            *instantOut = *candidate;
            return mloc_success;
        }
    }
    if (option == mloc_offsetReject)
    {
        return mloc_errorInvalid;
    }
    return mlocGetInstantFor(zone, dateTime, disambiguation, instantOut);
}

mlocResult mlocAddToZonedDateTime(const mlocTimeZone* zone, mlocCalendar calendar,
                                  const mlocInstant* instant, const mlocDateTimeDuration* duration,
                                  mlocOverflow overflow, mlocInstant* instantOut)
{
    if (instant == nullptr || duration == nullptr || instantOut == nullptr ||
        !mlocTimeDurationValid(&duration->time))
    {
        return mloc_errorInvalid;
    }
    int dateSign = DateSign(&duration->date);
    if (dateSign * TimeSign(&duration->time) < 0 || duration->date.days > s_maxDays ||
        duration->date.days < -s_maxDays)
    {
        return mloc_errorInvalid;
    }
    mlocDateTime local;
    if (mlocGetDateTimeFor(zone, instant, calendar, &local) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    mlocInstant intermediate = *instant;
    if (dateSign != 0)
    {
        // The date part in the calendar from the local date, the time of
        // day kept, then that date-time's compatible instant.
        mlocDateTime added = {.time = local.time};
        if (mlocAddToDate(&local.date, &duration->date, overflow, &added.date) != mloc_success ||
            !mlocDateTimeWithinLimits(&added) ||
            mlocGetInstantFor(zone, &added, mloc_disambiguationCompatible, &intermediate) !=
                mloc_success)
        {
            return mloc_errorInvalid;
        }
    }
    return mlocAddToInstant(&intermediate, &duration->time, instantOut);
}

// DifferenceZonedDateTime for different instants: the date difference of
// the local dates, the second's date moved toward the first until the
// first's time of day on it lies between them (a day, or two backward
// across a fold), and the exact time left.
static bool Difference(const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* one,
                       const mlocInstant* two, mlocUnit largest, mlocDateTime* startOut,
                       RelativeDuration* out)
{
    mlocDateTime end;
    if (mlocGetDateTimeFor(zone, one, calendar, startOut) != mloc_success ||
        mlocGetDateTimeFor(zone, two, calendar, &end) != mloc_success)
    {
        return false;
    }
    *out = (RelativeDuration){0};
    if (startOut->date.day == end.date.day)
    {
        out->time = Between(one, two);
        return true;
    }
    // ECMA-262's sign runs against the difference.
    int sign = CompareInstants(two, one) < 0 ? 1 : -1;
    int maxCorrection = sign == -1 ? 2 : 1;
    int64_t timeOfDay = NanosecondOfDay(&end.time) - NanosecondOfDay(&startOut->time);
    int correction = SignOf(timeOfDay) == sign ? 1 : 0;
    mlocDateTime intermediate = {.time = startOut->time};
    for (;; correction += 1)
    {
        mlocInstant at;
        if (correction > maxCorrection ||
            mlocMakeDateFromDay(calendar, (int32_t)(end.date.day + correction * sign),
                                &intermediate.date) != mloc_success ||
            mlocGetInstantFor(zone, &intermediate, mloc_disambiguationCompatible, &at) !=
                mloc_success)
        {
            return false;
        }
        out->time = Between(&at, two);
        if (TimeSign(&out->time) != sign)
        {
            break;
        }
    }
    mlocDateUnit dateLargest = largest < mloc_unitDay ? (mlocDateUnit)largest : mloc_dateUnitDay;
    return mlocDateUntil(&startOut->date, &intermediate.date, dateLargest, &out->date) ==
           mloc_success;
}

mlocResult mlocZonedDateTimeUntil(const mlocTimeZone* zone, mlocCalendar calendar,
                                  const mlocInstant* one, const mlocInstant* two,
                                  const mlocDateTimeRounding* rounding,
                                  mlocDateTimeDuration* durationOut)
{
    if (one == nullptr || two == nullptr || rounding == nullptr || durationOut == nullptr ||
        !mlocDateTimeRoundingValid(rounding))
    {
        return mloc_errorInvalid;
    }
    if (rounding->largestUnit >= mloc_unitHour)
    {
        // DifferenceEpochNanoseconds: the zone and calendar play no part
        // but must be ones the library made.
        mlocDateTime unused;
        mlocTimeRounding time = {(mlocTimeUnit)(rounding->largestUnit - mloc_unitDay),
                                 (mlocTimeUnit)(rounding->smallestUnit - mloc_unitDay),
                                 rounding->increment, rounding->mode};
        mlocTimeDuration d;
        if (mlocGetDateTimeFor(zone, one, calendar, &unused) != mloc_success ||
            mlocInstantUntil(one, two, &time, &d) != mloc_success)
        {
            return mloc_errorInvalid;
        }
        *durationOut = (mlocDateTimeDuration){.time = d};
        return mloc_success;
    }
    mlocDateTime start;
    RelativeDuration d = {0};
    if (!Difference(zone, calendar, one, two, rounding->largestUnit, &start, &d))
    {
        return mloc_errorInvalid;
    }
    bool equal = CompareInstants(one, two) == 0;
    if (!equal && (rounding->smallestUnit != mloc_unitNanosecond || rounding->increment != 1))
    {
        RelativeRounding relative = {(int)rounding->largestUnit, (int)rounding->smallestUnit,
                                     rounding->increment, rounding->mode};
        RelativeStart from = {&start.date, NanosecondOfDay(&start.time), zone, mlocMomentOf(one)};
        if (!mlocRoundRelative(&d, &from, mlocMomentOf(two), &relative))
        {
            return mloc_errorInvalid;
        }
    }
    *durationOut = (mlocDateTimeDuration){.date = d.date, .time = d.time};
    return mloc_success;
}

// Rounding to a day: the day's progress from its start to the next day's
// start, which the zone may make longer or shorter than 24 hours.
static mlocResult RoundToDay(const mlocTimeZone* zone, const mlocInstant* instant,
                             mlocRoundingMode mode, mlocInstant* instantOut)
{
    mlocDateTime local;
    mlocDate next;
    mlocInstant start;
    mlocInstant end;
    if (mlocGetDateTimeFor(zone, instant, mloc_calendarGregorian, &local) != mloc_success ||
        mlocMakeDateFromDay(mloc_calendarGregorian, (int32_t)(local.date.day + 1), &next) !=
            mloc_success ||
        mlocGetStartOfDay(zone, &local.date, &start) != mloc_success ||
        mlocGetStartOfDay(zone, &next, &end) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    // ECMA-262 takes the instant to a nanosecond before the next day's
    // start, where a change runs a day's clock back past midnight.
    mlocTimeDuration progress = Between(&start, instant);
    mlocTimeDuration length = Between(&start, &end);
    int64_t lengthNanoseconds = length.seconds * nanosecondsPerSecond + length.nanoseconds;
    int64_t part = progress.seconds * nanosecondsPerSecond + progress.nanoseconds;
    if (part >= lengthNanoseconds)
    {
        part = lengthNanoseconds - 1;
    }
    bool up = mlocRoundsUp(part, lengthNanoseconds, mlocUnsignedRoundingMode(mode, false), true);
    *instantOut = up ? end : start;
    return mloc_success;
}

mlocResult mlocRoundZonedDateTime(const mlocTimeZone* zone, const mlocInstant* instant,
                                  mlocTimeUnit unit, uint32_t increment, mlocRoundingMode mode,
                                  mlocInstant* instantOut)
{
    if (instant == nullptr || instantOut == nullptr || (uint32_t)mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    if (unit == mloc_timeUnitDay)
    {
        return increment == 1 ? RoundToDay(zone, instant, mode, instantOut) : mloc_errorInvalid;
    }
    // The local date-time rounded, then found again with the offset it
    // had preferred.
    mlocDateTime local;
    mlocDateTime rounded;
    if (mlocGetDateTimeFor(zone, instant, mloc_calendarGregorian, &local) != mloc_success ||
        mlocRoundDateTime(&local, unit, increment, mode, &rounded) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    int64_t offset = CandidateOffset(&local, instant);
    return mlocGetInstantForOffset(zone, &rounded, offset, mloc_offsetPrefer,
                                   mloc_disambiguationCompatible, false, instantOut);
}
