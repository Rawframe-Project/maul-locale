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
// and every row, set and time set indexes within the unit.
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
    uint32_t timeSetCount;
    uint32_t intervalSetCount;
    uint32_t datePartCount;
    uint32_t intervalRowCount;
    uint32_t timePartCount;
    uint32_t prefixCount;
    uint32_t pairCount;
    const uint8_t* offsets;
    const uint8_t* rows;
    const uint8_t* sets;
    const uint8_t* timeSets;
    const uint8_t* intervalSets;
    const uint8_t* dateParts;
    const uint8_t* intervalRows;
    const uint8_t* timeParts;
    const uint8_t* prefixes;
    const uint8_t* pairs;
    const uint8_t* text;
} DatePatterns;

DatePatterns mlocReadDatePatterns(DataSpan unit);

// Whether a date names unit's sizes hold, each name is UTF-8 without
// control bytes, each list indexes its names and each set's lists have
// their kinds' sizes and each day period rule gives every hour a period.
// A leap month pattern's "{0}" is the user's to check: the validator
// stays linear in the unit's size.
bool mlocValidDateNames(const DataView* view, DataSpan payload);

// The parts of a valid date names unit.
typedef struct DateNameTables
{
    uint32_t stringCount;
    uint32_t listCount;
    uint32_t setCount;
    uint32_t ruleCount;
    const uint8_t* strings;
    const uint8_t* lists;
    const uint8_t* items;
    const uint8_t* sets;
    const uint8_t* rules;
    const uint8_t* text;
} DateNameTables;

DateNameTables mlocReadDateNames(DataSpan unit);

// A list of a valid names unit: its first item and its length.
typedef struct DateNameList
{
    const uint8_t* items;
    uint32_t count;
} DateNameList;

DateNameList mlocDateNameList(const DateNameTables* t, uint32_t list);

// The name of a list's item; an empty span past the list.
DataSpan mlocDateName(const DateNameTables* t, DateNameList list, uint32_t item);

// A locale's date patterns in a calendar (MlocCalendar's value) along its
// main chain: the set's bytes (dateSetSize), the interval set's
// (dateIntervalSetSize) and the unit they index. False when the data has
// no dates, the calendar is unknown or a set is past the unit's.
bool mlocFindDateSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                     DatePatterns* patterns, const uint8_t** set, const uint8_t** intervals);

// A locale's time patterns in a calendar along its main chain: the time
// set's bytes (dateTimeSetSize), when intervals is not null the interval
// set's, and the unit they index. False when the data has no dates, the
// calendar is unknown or a set is past the unit's.
bool mlocFindTimeSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                     DatePatterns* patterns, const uint8_t** set, const uint8_t** intervals);

// A locale's date names in a calendar along its main chain: the name
// set's bytes (dateNameSetSize) and the unit they index. False when the
// data has no names, the calendar is unknown or the set is past the
// unit's.
bool mlocFindDateNames(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                       DateNameTables* names, const uint8_t** set);

// A locale's day period rules along its main chain (dateRuleSize
// bytes). False when the data has no names or the rules are past the
// unit's.
bool mlocFindDayPeriodRules(const mlocDataSet* data, const mlocLocale* locale,
                            const uint8_t** rules);
// A pattern of a valid unit by index; false past its patterns.
bool mlocDatePattern(const DatePatterns* p, uint32_t index, DataSpan* out);

// A patterns unit's pattern views, which a formatter keeps to resolve
// its interval indexes.
typedef struct PatternTable
{
    const uint8_t* offsets;
    const uint8_t* text;
    uint32_t count;
} PatternTable;

// The items of a pattern from the byte offset first to end whose parts
// are one value's: source (mloc_dateRangeStart or mloc_dateRangeEnd) is
// added to their kinds, the others' 0.
typedef struct PatternSpan
{
    uint32_t first;
    uint32_t end;
    uint32_t source;
} PatternSpan;

// A table's pattern by index; false past its patterns.
bool mlocTablePattern(const PatternTable* table, uint32_t index, DataSpan* out);

#endif
