// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Ranges of dates and times (maul-locale/datetime.h): what a range's
// patterns show, the greatest difference of two values, and the writing of
// a range as ICU4C's DateIntervalFormat writes it: by its pair's halves,
// by the fallback around one pattern written for each value, or as one
// value.

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

// The greatest date field two dates differ in, rangeEra to rangeDay, or
// rangeNone: the era's code, the year in its era, the month's code (a
// leap month differs from the month it follows), the day.
uint32_t mlocDateDifference(const mlocDateFields* a, const mlocDateFields* b);

// The greatest time field two times of day differ in, rangeAmPm to
// rangeSecond (a fraction's difference shown with the seconds is the
// seconds'), or rangeNone: am or pm, the hour, the minute, the second, the
// fraction to the digits shown.
uint32_t mlocTimeDifference(const mlocTime* a, const mlocTime* b, uint32_t fractionDigits);

// Writes a range by its choice.
void mlocWriteRange(const RangeChoice* c, NumberWriter* w);

// Whether a choice writes two values: a difference with a pair, or one
// its pattern shows.
bool mlocRangeWritesTwo(const RangeChoice* c);

// The prefixed pattern a date-time's date difference writes (a prefix's
// pattern for the field the date lacks), and its glue style (0 full to 3
// short); false where the date shows the field (its own pattern stands).
bool mlocRangePrefix(const PatternTable* table, const uint8_t* prefix, uint32_t difference,
                     DataSpan* pattern, uint32_t* style);

// A value written as a date and a time joined by glue ({1} the date, {0}
// the time), each part by its pattern and writer.
typedef struct RangeJoin
{
    DataSpan glue;
    DataSpan date;
    DataSpan time;
    RangeValueWriter writeDate;
    RangeValueWriter writeTime;
    const void* context;
} RangeJoin;

// Writes a range of two values joined so, each whole, around the
// fallback ({0} the start, {1} the end): a value's parts run from its
// first part's first field to its second part's last, the glue between
// them its own.
void mlocWriteJoinedFallback(const RangeJoin* j, DataSpan fallback, NumberWriter* w);

#endif
