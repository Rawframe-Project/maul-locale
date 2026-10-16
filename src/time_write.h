// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A time formatter's state (maul-locale/datetime.h, record mloc-0042): the
// compiled patterns of its options, its hour letter, digits and decimal
// separator, its day period names and rules; and the writing of a time's
// fields by a pattern.

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

typedef struct TimeFormat
{
    const uint8_t* pattern[timeFormatPatterns];
    uint32_t patternLength[timeFormatPatterns];
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

#endif
