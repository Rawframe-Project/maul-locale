// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Times of day and date-times (maul-locale/zone.h, record mloc-0041):
// their checks, and a date-time as seconds since 1970 read as universal
// time.

#ifndef MLOC_DATE_TIME_H
#define MLOC_DATE_TIME_H

#include "maul-locale/zone.h"

#include <stdint.h>

enum
{
    secondsPerDay = 86400,
    // The day of January 1 of 1970, in mlocDate's count.
    epochDay = 719163,
};

// Whether a time of day is one the library made.
bool mlocTimeValid(const mlocTime* time);

// Whether a date and a time of day are ones the library made; a
// date-time's limits are not checked.
bool mlocDateTimeValid(const mlocDateTime* dateTime);

// A date-time's seconds since 1970 as if in universal time (its
// nanoseconds aside).
int64_t mlocDateTimeSeconds(const mlocDateTime* dateTime);

#endif
