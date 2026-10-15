// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units (src/date_tables.h): their validators, and a
// locale's compiled patterns.

#ifndef MLOC_DATE_DATA_H
#define MLOC_DATE_DATA_H

#include "data_view.h"
#include "date_tables.h"

#include "maul-locale/locale.h"

#include <stdint.h>

// Whether a date patterns unit's sizes hold, each pattern is well formed
// and every row and set indexes within the unit.
bool mlocValidDatePatterns(const DataView* view, DataSpan payload);

// Whether a locale's dates unit has its size.
bool mlocValidDates(const DataView* view, DataSpan payload);

// Whether a compiled pattern is well formed: literal UTF-8 without bytes
// below 0x20, and field items of the known letters and widths.
bool mlocValidDatePattern(const uint8_t* text, uint32_t length);

// The parts of a valid date patterns unit.
typedef struct DatePatterns
{
    uint32_t patternCount;
    uint32_t rowCount;
    uint32_t setCount;
    const uint8_t* offsets;
    const uint8_t* rows;
    const uint8_t* sets;
    const uint8_t* text;
} DatePatterns;

DatePatterns mlocReadDatePatterns(DataSpan unit);

// A locale's date patterns in a calendar (MlocCalendar's value) along its
// main chain: the set's bytes (dateSetSize) and the unit they index.
// False when the data has no dates, the calendar is unknown or the
// locale's set is past the unit's.
bool mlocFindDateSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                     DatePatterns* patterns, const uint8_t** set);

// A pattern of a valid unit by index; false past its patterns.
bool mlocDatePattern(const DatePatterns* p, uint32_t index, DataSpan* out);

#endif
