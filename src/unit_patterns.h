// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A unit formatter's contents (record mloc-0024) and its patterns,
// shared by the simple path (unit_format.c) and the mixed units'
// (unit_mixed_format.c).

#ifndef MLOC_UNIT_PATTERNS_H
#define MLOC_UNIT_PATTERNS_H

#include "list_data.h"
#include "number_format.h"
#include "unit_compound.h"
#include "unit_data.h"
#include "unit_id.h"

#include "maul-locale/unit.h"

enum
{
    // Marks a unit formatter the library made.
    unitStamp = 0x55,
    unitCategories = 6,
    // The bytes the patterns take, each kept once.
    unitPool = 2048,
    // The pattern slots: a unit's, or a mixed unit's parts', of each
    // output unit.
    unitSlots = 8,
    // The most output units a formatter keeps (unit_prefs.h's
    // unitMaxOutputs).
    unitOutputs = 8,
};

// An output unit: its identifier parsed (for a mixed unit or a usage's
// output), the threshold a value must reach in it (threshold /
// 10^places, the last output taking any), the preference's rounding
// increment, and its pattern slots: the first, the count (1, or a mixed
// unit's parts) and the index of a mixed unit's smallest part.
typedef struct UnitOutput
{
    UnitId id;
    uint32_t threshold;
    uint16_t increment;
    uint8_t places;
    uint8_t slot;
    uint8_t parts;
    uint8_t smallest;
    // For the numeric width, the duration pattern a mixed unit of hours,
    // minutes and seconds takes (unitPatternHm and the others, plus 1; 0
    // for none), and the part of each field (hour, minute, second; -1 for
    // none).
    uint8_t numeric;
    int8_t field[3];
} UnitOutput;

// The opaque value's contents: each slot's patterns by plural category,
// copied into the pool with "{0}" as currencyTextNumber.
typedef struct UnitFormat
{
    Formatter formatter;
    uint16_t start[unitSlots][unitCategories];
    uint16_t length[unitSlots][unitCategories];
    uint8_t pool[unitPool];
    mlocPluralRules rules;
    // Written in the percent pattern, with no pattern around it.
    bool percent;
    // Whether the output unit is chosen by the value (a usage).
    bool usage;
    // The output units: 1, or a usage's.
    uint8_t outputs;
    // A usage's input unit, the unit table that converts and splits
    // amounts, and the unit list that joins a mixed unit's parts.
    UnitId input;
    UnitTable table;
    ListPatterns list;
    UnitOutput output[unitOutputs];
    // The numeric width's duration patterns, and a coordinate's direction
    // patterns (east, north, south, west), in the data.
    const char* duration[3];
    uint16_t durationLength[3];
    const char* direction[4];
    uint16_t directionLength[4];
    // The mlocCoordinate the amounts are written as.
    uint8_t coordinate;
    // The mlocGrammaticalCase the patterns were loaded in.
    uint8_t grammaticalCase;
} UnitFormat;

// The names kind of a width.
uint32_t mlocUnitWidthKind(mlocUnitWidth width);

// A unit's normal identifier (mlocNormalizeUnitName); one the unit table
// does not know but the names do ("generic", which converts to nothing)
// as it is.
mlocResult mlocNormalUnitName(const mlocDataSet* data, const char* unit, size_t length, char* out,
                              size_t capacity, size_t* lengthOut);

// A unit's patterns in a width and case, by any identifier of it: its
// normal identifier's (mlocBuildUnitForms), or as it is for one the unit
// table does not know but the names do ("generic"). The errors of
// mlocBuildUnitForms.
mlocResult mlocLoadUnitForms(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                             size_t length, uint32_t kind, uint32_t gcase, UnitForms* out);

// Copies a slot's patterns into the pool, a category without its own
// taking other's: mloc_success, mloc_errorNotFound for forms without
// other, mloc_errorCapacity when the pool is full.
mlocResult mlocAddUnitPatterns(UnitFormat* u, uint32_t slot, size_t* used, const UnitForms* forms);

// The pattern around a slot's number.
void mlocUnitOuter(const UnitFormat* u, uint32_t slot, Outer* out);

#endif
