// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a difference relative to where it starts (records mloc-0040,
// mloc-0041): ECMA-262's RoundRelativeDuration without a time zone,
// through NudgeToCalendarUnit, NudgeToDayOrTime and
// BubbleRelativeDuration, for a duration from a date and time of day to
// a later or earlier one. A moment is a day and the nanoseconds past its
// midnight, so the progress between two multiples is compared exactly
// without integers wider than 64 bits. The calendar's date differences
// are the case of midnight at both ends.

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

// A day and the nanoseconds past its midnight (0 to a day less one).
typedef struct Moment
{
    int64_t day;
    int64_t nanosecond;
} Moment;

// How a relative duration is rounded: units as above, an increment of 1
// to 10^9 already checked against the smallest unit, a mode.
typedef struct RelativeRounding
{
    int largestUnit;
    int smallestUnit;
    int64_t increment;
    mlocRoundingMode mode;
} RelativeRounding;

// Rounds a duration that runs from the date one at the time of day
// startNanosecond to the moment dest. False when a date it reaches
// leaves Temporal's range or the time part passes 2^53 seconds.
bool mlocRoundRelative(RelativeDuration* duration, const mlocDate* one, int64_t startNanosecond,
                       Moment dest, const RelativeRounding* rounding);

// RoundTimeDurationToIncrement: a time duration to a multiple of a step
// of stepSeconds whole seconds, or when that is 0 of stepNanoseconds, a
// divisor of a second. False when the result passes 2^53 seconds.
bool mlocRoundTimeDuration(mlocTimeDuration value, int64_t stepSeconds, int64_t stepNanoseconds,
                           mlocRoundingMode mode, mlocTimeDuration* out);

#endif
