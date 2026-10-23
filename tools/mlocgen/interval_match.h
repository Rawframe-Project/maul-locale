// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date interval patterns resolved for a skeleton as ICU4C's
// DateIntervalFormat resolves them when it is made: the locale's
// intervalFormats items, the best of them for the skeleton's date or time
// part, widened to the skeleton, split into the halves the earlier and the
// later value write, and the fallbacks where no item serves. Patterns are
// UTF-8.

#ifndef MLOCGEN_INTERVAL_MATCH_H
#define MLOCGEN_INTERVAL_MATCH_H

#include "skeleton.h"

enum
{
    // The greatest differences an interval pattern answers, in ICU's
    // order: era, year, month, day, am or pm, hour, minute.
    intervalEra = 0,
    intervalYear = 1,
    intervalMonth = 2,
    intervalDay = 3,
    intervalAmPm = 4,
    intervalHour = 5,
    intervalMinute = 6,
    intervalFieldCount = 7,
    intervalMaxItems = 256,
};

// A locale's intervalFormats in one calendar: each item's skeleton and
// its pattern per greatest difference (null for none), in the order ICU
// meets them; the fallback ("{0} – {1}").
typedef struct IntervalItems
{
    char* skeletons[intervalMaxItems];
    char* patterns[intervalMaxItems][intervalFieldCount];
    int count;
    char* fallback;
} IntervalItems;

// A greatest difference's pattern halves: the first written with the
// earlier value (the later where laterFirst), the second with the other.
// A first of null with a second is a fallback: the second is the whole
// pattern, written for both values in the fallback; both null, none.
typedef struct IntervalPair
{
    char* first;
    char* second;
    bool laterFirst;
} IntervalPair;

typedef struct IntervalPatterns
{
    IntervalPair pairs[intervalFieldCount];
    // The skeleton's own pattern, and its date and time parts' where it
    // has both, for the fallback of a time range on one day.
    char* pattern;
    char* datePattern;
    char* timePattern;
} IntervalPatterns;

// Sets an item's pattern for a greatest difference letter (G, y, M, d,
// a, B, h, H, m) unless it has one; other letters are left out, as ICU
// leaves them.
void IntervalItemsPut(IntervalItems* items, const char* skeleton, const char* letter,
                      const char* pattern);

void IntervalItemsFree(IntervalItems* items);

// Resolves a skeleton's interval patterns: g matches patterns as ICU's
// DateFormat::getBestPattern does (the standard formats among the items),
// glue is the Gregorian medium date-time pattern ICU joins a date and a
// time range with.
void IntervalResolve(PatternGenerator* g, const IntervalItems* items, const char* glue,
                     const char* skeleton, IntervalPatterns* out);

void IntervalPatternsFree(IntervalPatterns* p);

#endif
