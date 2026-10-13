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
    unitPool = 1024,
};

// The opaque value's contents: a part's patterns by plural category
// (one part, or a mixed unit's), copied into the pool with "{0}" as
// currencyTextNumber.
typedef struct UnitFormat
{
    Formatter formatter;
    uint16_t start[unitMaxParts][unitCategories];
    uint16_t length[unitMaxParts][unitCategories];
    uint8_t pool[unitPool];
    mlocPluralRules rules;
    // Written in the percent pattern, with no pattern around it.
    bool percent;
    // The parts: 1, or a mixed unit's 2 to unitMaxParts, and the index of
    // a mixed unit's smallest part.
    uint8_t parts;
    uint8_t smallest;
    // A mixed unit's identifier, the unit table that splits amounts
    // across its parts and the unit list that joins them.
    UnitId mixed;
    UnitTable table;
    ListPatterns list;
} UnitFormat;

// The names kind of a width.
uint32_t mlocUnitWidthKind(mlocUnitWidth width);

// A unit's normal identifier (mlocNormalizeUnitName); one the unit table
// does not know but the names do ("generic", which converts to nothing)
// as it is.
mlocResult mlocNormalUnitName(const mlocDataSet* data, const char* unit, size_t length, char* out,
                              size_t capacity, size_t* lengthOut);

// A unit's patterns in a width, by any identifier of it: its normal
// identifier's (mlocBuildUnitForms), or as it is for one the unit table
// does not know but the names do ("generic"). The errors of
// mlocBuildUnitForms.
mlocResult mlocLoadUnitForms(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                             size_t length, uint32_t kind, UnitForms* out);

// Copies a part's patterns into the pool, a category without its own
// taking other's: mloc_success, mloc_errorNotFound for forms without
// other, mloc_errorCapacity when the pool is full.
mlocResult mlocAddUnitPatterns(UnitFormat* u, uint32_t part, size_t* used, const UnitForms* forms);

// The pattern around a part's number.
void mlocUnitOuter(const UnitFormat* u, uint32_t part, Outer* out);

#endif
