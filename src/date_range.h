// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Ranges of dates and times (maul-locale/datetime.h, research 40): what a
// range's patterns show, the greatest difference of two values, and the
// writing of a range as ICU4C's DateIntervalFormat writes it: by its
// pair's halves, by the fallback around one pattern written for each
// value, or as one value.

#ifndef MLOC_DATE_RANGE_H
#define MLOC_DATE_RANGE_H

#include "date_data.h"
#include "date_write.h"
#include "number_core.h"

#include "maul-locale/calendar.h"

// The greatest field two values differ in, in ICU's order; the first
// dateIntervalFields have pairs.
typedef enum RangeDifference
{
    rangeEra = 0,
    rangeYear = 1,
    rangeMonth = 2,
    rangeDay = 3,
    rangeAmPm = 4,
    rangeHour = 5,
    rangeMinute = 6,
    rangeSecond = 7,
    rangeFraction = 8,
    rangeNone = 9,
} RangeDifference;

// Writes a range's value (0 the start, 1 the end) by a pattern, the
// span's parts marked.
typedef void (*RangeValueWriter)(const void* context, uint32_t value, const uint8_t* pattern,
                                 uint32_t length, const PatternSpan* span, NumberWriter* w);

// What a range writes: its difference, the pair for it (null for a
// difference without pairs), the pattern of one value, which the
// fallback also writes when the pair has none, and the fallback ({0}
// and {1} in literal text).
typedef struct RangeChoice
{
    RangeValueWriter write;
    const void* context;
    const PatternTable* table;
    const uint8_t* pair;
    uint32_t difference;
    DataSpan own;
    DataSpan fallback;
} RangeChoice;

// Whether every half of a pair is a pattern of the table that the
// writer takes.
bool mlocValidRangePair(const PatternTable* table, const uint8_t* pair,
                        bool (*writable)(const uint8_t* pattern, uint32_t length));

// The greatest date field two dates differ in, rangeEra to rangeDay, or
// rangeNone: the era's code, the year in its era, the month's code (a
// leap month differs from the month it follows), the day.
uint32_t mlocDateDifference(const mlocDateFields* a, const mlocDateFields* b);

// Writes a range by its choice.
void mlocWriteRange(const RangeChoice* c, NumberWriter* w);

#endif
