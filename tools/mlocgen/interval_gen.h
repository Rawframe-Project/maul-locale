// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A calendar's interval set in a locale (date_tables.h's
// DateIntervalEntry): the interval patterns of every date and time
// option's skeleton, resolved as ICU4C's DateIntervalFormat resolves them
// (interval_match.h), and the fallback; the date unit's builder compiles
// and keeps them.

#ifndef MLOCGEN_INTERVAL_GEN_H
#define MLOCGEN_INTERVAL_GEN_H

#include "cldr.h"
#include "date_tables.h"
#include "interval_match.h"

// A date entry's skeleton with the larger fields it lacks prefixed, as
// ICU prefixes a date-time's for its date differences: the pattern with
// d, with M and d, with y, M and d, with G, y, M and d (null where the
// field is present), and the date-time glue style of each (0 full to 3
// short, by its month's width and weekday).
typedef struct IntervalPrefix
{
    char* patterns[dateIntervalPrefixes];
    int styles[dateIntervalPrefixes];
} IntervalPrefix;

typedef struct IntervalSet
{
    // Each entry's patterns (its own pattern among them).
    IntervalPatterns entries[dateIntervalEntryCount];
    // The date entries' and standard date formats' prefixes.
    IntervalPrefix prefixes[dateIntervalTime];
    // The short date with the full year and with the era (yMd, GyMd).
    char* shortDate;
    char* shortEraDate;
    // The fallback as a compiled-pattern source: its text quoted, its
    // two values as {0} and {1}.
    char* fallback;
} IntervalSet;

// Resolves a locale's interval set in a calendar: g is its generator as
// DateSourceLoad loads it; dates are the classical skeletons of the date
// entries (DateSetEntry by length), as the date set's patterns were
// matched.
void IntervalSetResolve(Cldr* cldr, PatternGenerator* g, const char* locale, const char* calendar,
                        const char* const dates[dateIntervalDateStyle], IntervalSet* out);

void IntervalSetFree(IntervalSet* set);

#endif
