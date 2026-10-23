// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A date formatter's state (maul-locale/datetime.h): the compiled patterns
// of its options in its calendar, its names, its digits and those a
// pattern names for its fields; and the writing of a date's fields by a
// pattern.

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

// A formatter's interval patterns: the table their indexes resolve in, the
// pair and prefixes of each pattern the formatter keeps
// (dateIntervalPairSize bytes), a standard format's own pattern (the
// generator's match of its skeleton, which a range of one value writes; a
// field set's are its patterns), and the fallback.
typedef struct DateRange
{
    PatternTable table;
    const uint8_t* pairs[dateFormatPatterns];
    // Each kept pattern's prefixes (dateIntervalPrefixSize bytes): its
    // skeleton with d, M, y and G prefixed where it lacks them, which a
    // date-time's date differences write.
    const uint8_t* prefixes[dateFormatPatterns];
    const uint8_t* own;
    uint32_t ownLength;
    const uint8_t* fallback;
    uint32_t fallbackLength;
} DateRange;

typedef struct DateFormat
{
    const uint8_t* pattern[dateFormatPatterns];
    uint32_t patternLength[dateFormatPatterns];
    DateRange range;
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

// A number in digits (each of digitLength bytes), at least width of
// them (at most 24), into out.
size_t mlocDateDigits(const uint8_t* digits, uint8_t digitLength, uint64_t value, uint32_t width,
                      char* out);

// Writes a date's fields by a compiled pattern; the date is read in the
// formatter's calendar already. relatedYear is the Gregorian year the
// date's year begins in.
void mlocWriteDateFields(const DateFormat* f, const uint8_t* pattern, uint32_t length,
                         const mlocDateFields* fields, int32_t relatedYear, NumberWriter* w);

// As mlocWriteDateFields, the span's parts marked as one value's.
void mlocWriteDateFieldsIn(const DateFormat* f, const uint8_t* pattern, uint32_t length,
                           const mlocDateFields* fields, int32_t relatedYear,
                           const PatternSpan* span, NumberWriter* w);

// Writes a glue's slot: 0 for the time (or zone, or a range's start), 1
// for the date (or a range's end).
typedef void (*GlueSlotWriter)(void* context, uint32_t slot, NumberWriter* w);

// Writes glue: its text, and each {0} and {1} by the writer.
void mlocWriteGlue(const uint8_t* glue, uint32_t length, GlueSlotWriter write, void* context,
                   NumberWriter* w);

// As mlocWriteGlue, the text between the two slots written as between's
// (a range's value whose parts run across the glue).
void mlocWriteGlueBetween(const uint8_t* glue, uint32_t length, GlueSlotWriter write, void* context,
                          uint32_t between, NumberWriter* w);

#endif
