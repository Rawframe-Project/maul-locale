// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a difference relative to where it starts (records mloc-0040,
// mloc-0041): ECMA-262's RoundRelativeDuration, through
// NudgeToCalendarUnit, NudgeToZonedTime, NudgeToDayOrTime and
// BubbleRelativeDuration, for a duration from a date and time of day to
// a later or earlier one, with or without a time zone. A moment is a day
// and the nanoseconds past its midnight in universal time, so the
// progress between two multiples is compared exactly without integers
// wider than 64 bits. Without a zone a moment reads the wall clock as
// universal time. The calendar's date differences are the case of
// midnight at both ends.

#ifndef MLOC_DURATION_ROUND_H
#define MLOC_DURATION_ROUND_H

#include "maul-locale/zone.h"

#include <stdint.h>

// Temporal's units in its table's order: year (0) to nanosecond (9); a
// date unit is day or larger.
enum
{
    unitYear = 0,
    unitMonth = 1,
    unitWeek = 2,
    unitDay = 3,
    unitHour = 4,
    unitNanosecond = 9,
};

// A difference to round: its date part and its time part.
typedef struct RelativeDuration
{
    mlocDateDuration date;
    mlocTimeDuration time;
} RelativeDuration;

// A day in mlocDate's count and the nanoseconds past its midnight (0 to
// a day less one).
typedef struct Moment
{
    int64_t day;
    int64_t nanosecond;
} Moment;

// Where a relative duration starts: a date and its time of day, the
// zone its dates are read in (null for none), and the first moment. With
// a zone that is the first instant, which in a fold may not be the
// compatible one.
typedef struct RelativeStart
{
    const mlocDate* date;
    int64_t nanosecond;
    const mlocTimeZone* zone;
    Moment origin;
} RelativeStart;

// How a relative duration is rounded: units as above, an increment of 1
// to 10^9 already checked against the smallest unit, a mode.
typedef struct RelativeRounding
{
    int largestUnit;
    int smallestUnit;
    int64_t increment;
    mlocRoundingMode mode;
} RelativeRounding;

// Rounds a duration that runs from start to the moment dest. False when
// a date or instant it reaches leaves Temporal's range or the time part
// passes 2^53 seconds.
bool mlocRoundRelative(RelativeDuration* duration, const RelativeStart* start, Moment dest,
                       const RelativeRounding* rounding);

// An instant as a moment.
Moment mlocMomentOf(const mlocInstant* instant);

// TotalRelativeDuration: the total of a unit in a duration that runs from
// start to the moment dest, as the binary64 nearest it. False when a date
// or instant it reaches leaves Temporal's range.
bool mlocTotalRelative(const RelativeDuration* duration, const RelativeStart* start, Moment dest,
                       int unit, double* totalOut);

// TotalTimeDuration: a time duration over a unit's length in
// nanoseconds, as the binary64 nearest it.
double mlocTotalOfTime(const mlocTimeDuration* time, int64_t unitNanoseconds);

// RoundTimeDurationToIncrement: a time duration to a multiple of a step
// of stepSeconds whole seconds, or when that is 0 of stepNanoseconds, a
// divisor of a second. False when the result passes 2^53 seconds.
bool mlocRoundTimeStep(mlocTimeDuration value, int64_t stepSeconds, int64_t stepNanoseconds,
                       mlocRoundingMode mode, mlocTimeDuration* out);

#endif
