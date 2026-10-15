// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a difference of dates (maul-locale/calendar.h, record
// mloc-0040): Temporal's RoundRelativeDuration for two dates at midnight
// and no time zone, which duration_round.h gives for date-times.

#include "duration_round.h"

#include "maul-locale/calendar.h"

static const uint32_t s_maxIncrement = 1000000000;

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
    RelativeDuration d = {0};
    mlocResult r = mlocDateUntil(one, two, rounding->largestUnit, &d.date);
    if (r != mloc_success || one->day == two->day ||
        (rounding->smallestUnit == mloc_dateUnitDay && rounding->increment == 1))
    {
        if (r == mloc_success)
        {
            *durationOut = d.date;
        }
        return r;
    }
    // Both ends at midnight: the date units' values are the date-time
    // rounding's (duration_round.h).
    RelativeRounding relative = {(int)rounding->largestUnit, (int)rounding->smallestUnit,
                                 rounding->increment, rounding->mode};
    if (!mlocRoundRelative(&d, one, 0, (Moment){two->day, 0}, &relative))
    {
        return mloc_errorInvalid;
    }
    *durationOut = d.date;
    return mloc_success;
}
