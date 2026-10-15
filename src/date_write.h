// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A date formatter's state (maul-locale/datetime.h, record mloc-0042):
// the compiled patterns of its options in its calendar, its names, its
// digits and those a pattern names for its fields; and the writing of a
// date's fields by a pattern.

#ifndef MLOC_DATE_WRITE_H
#define MLOC_DATE_WRITE_H

#include "date_data.h"
#include "number_core.h"
#include "number_tables.h"

#include "maul-locale/datetime.h"
#include "maul-locale/spellout.h"

enum
{
    // The patterns a formatter keeps: a field set's in the three year
    // styles, or a standard format's once.
    dateFormatPatterns = 3,
};

typedef struct DateFormat
{
    const uint8_t* pattern[dateFormatPatterns];
    uint32_t patternLength[dateFormatPatterns];
    DateNameTables names;
    // The calendar's name set, and the Gregorian one's (Japanese dates
    // before Meiji take its eras).
    const uint8_t* nameSet;
    const uint8_t* gregorianSet;
    uint8_t digits[10 * numberDigitSize];
    // The digits of a system the patterns name for their fields, when it
    // has digits ("hanidec").
    uint8_t overrideDigits[10 * numberDigitSize];
    // The locale's minus sign, for a year before the epoch of a calendar
    // with no earlier era (the Persian).
    uint8_t minus[12];
    uint8_t minusLength;
    uint8_t digitLength;
    uint8_t overrideDigitLength;
    // Whether the patterns name an algorithmic system, written by the
    // spell-out formatter.
    uint8_t hasSpellout;
    uint8_t calendar;
    uint8_t firstDay;
    uint8_t minimalDays;
    // An mlocYearStyle, or 3 for a standard format.
    uint8_t yearStyle;
    uint8_t stamp;
    mlocSpelloutFormatter spellout;
} DateFormat;

// Writes a date's fields by a compiled pattern; the date is read in the
// formatter's calendar already. relatedYear is the Gregorian year the
// date's year begins in.
void mlocWriteDateFields(const DateFormat* f, const uint8_t* pattern, uint32_t length,
                         const mlocDateFields* fields, int32_t relatedYear, NumberWriter* w);

#endif
