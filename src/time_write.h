// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A time formatter's state (maul-locale/datetime.h): the compiled patterns
// of its options, its hour letter, digits and decimal separator, its day
// period names and rules; and the writing of a time's fields by a pattern.

#ifndef MLOC_TIME_WRITE_H
#define MLOC_TIME_WRITE_H

#include "date_data.h"
#include "number_core.h"
#include "number_tables.h"

#include "maul-locale/datetime.h"

enum
{
    // The patterns a time formatter keeps: the one it writes, and the
    // hour's for a minute of zero under minute-optional precision.
    timeFormatPatterns = 2,
};

// A time formatter's interval patterns and its relative glue: the table
// their indexes resolve in, the pair of each pattern the formatter keeps,
// a standard format's own pattern (null for the others, whose own are
// their patterns), the fallback, and for a time in a zone whose dates
// differ the short date with the full year and with the era (yMd, GyMd)
// and the short atTime glue that joins either to the time.
typedef struct TimeRange
{
    PatternTable table;
    const uint8_t* pairs[timeFormatPatterns];
    const uint8_t* own;
    uint32_t ownLength;
    const uint8_t* fallback;
    uint32_t fallbackLength;
    uint16_t shortDate;
    uint16_t shortEraDate;
    uint16_t glue;
    // The relative date-time glue of each standard length, full to short,
    // which a relative day and the time take ("tomorrow at 3:00 PM").
    uint16_t relativeGlue[dateStandardCount];
} TimeRange;

typedef struct TimeFormat
{
    const uint8_t* pattern[timeFormatPatterns];
    uint32_t patternLength[timeFormatPatterns];
    TimeRange range;
    DateNameTables names;
    // The day periods of the calendar's names, by context, width and
    // DateDayPeriod.
    DateNameList dayPeriods;
    uint8_t rules[dateRuleSize];
    uint8_t digits[10 * numberDigitSize];
    uint8_t decimal[12];
    uint8_t decimalLength;
    uint8_t digitLength;
    // The letter every hour field takes (K, h, H or k), or 0 for the
    // pattern's own.
    uint8_t hourLetter;
    // The fraction digits written after the seconds, 0 to 9.
    uint8_t fractionDigits;
    // Whether a pattern holds a zone field, which only a zoned writing
    // fills.
    uint8_t zoned;
    uint8_t stamp;
} TimeFormat;

// Writes a field a time pattern holds beside its own (a weekday's E).
typedef void (*TimeOtherField)(void* context, const uint8_t* item, NumberWriter* w);

// Writes a time's fields by a compiled pattern.
void mlocWriteTimeFields(const TimeFormat* f, const uint8_t* pattern, uint32_t length,
                         const mlocTime* time, NumberWriter* w);

// Writes a time's fields by a compiled pattern, any field but a time's
// and a day period's by other.
void mlocWriteTimeFieldsWith(const TimeFormat* f, const uint8_t* pattern, uint32_t length,
                             const mlocTime* time, TimeOtherField other, void* context,
                             NumberWriter* w);

// As mlocWriteTimeFieldsWith, the span's parts marked as one value's.
void mlocWriteTimeFieldsIn(const TimeFormat* f, const uint8_t* pattern, uint32_t length,
                           const mlocTime* time, TimeOtherField other, void* context,
                           const PatternSpan* span, NumberWriter* w);

#endif
