// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table (unit_tables.h): its validator, for the kind list, and
// the reads the unit parser and converter make of a valid table.

#ifndef MLOC_UNIT_DATA_H
#define MLOC_UNIT_DATA_H

#include "data_view.h"
#include "wide.h"

#include <stddef.h>
#include <stdint.h>

bool mlocValidUnits(const DataView* view, DataSpan payload);

// The parts of a valid unit table.
typedef struct UnitTable
{
    const uint8_t* simples;
    const uint8_t* prefixes;
    const uint8_t* aliases;
    const uint8_t* bases;
    const uint8_t* beaufort;
    const uint8_t* text;
    const uint8_t* numbers;
    uint32_t simpleCount;
    uint32_t prefixCount;
    uint32_t aliasCount;
    uint32_t baseCount;
    uint32_t beaufortCount;
    uint32_t dimensionCount;
} UnitTable;

UnitTable mlocReadUnitTable(DataSpan unit);

// The index of the simple unit with an identifier, or -1.
int32_t mlocFindSimpleUnit(const UnitTable* t, const char* id, size_t length);

// The prefix an identifier's token begins with, the longest, or -1.
int32_t mlocFindUnitPrefix(const UnitTable* t, const char* token, size_t length);

// The replacement of a deprecated identifier, its length or 0 when the
// identifier is none.
size_t mlocFindUnitAlias(const UnitTable* t, const char* id, size_t length,
                         const char** replacementOut);

// A simple unit's record and a prefix's.
const uint8_t* mlocSimpleUnit(const UnitTable* t, uint32_t index);
const uint8_t* mlocUnitPrefix(const UnitTable* t, uint32_t index);

// A base unit's exponent of each dimension.
const int8_t* mlocBaseDimensions(const UnitTable* t, uint32_t base);

// A simple unit's size in its base unit, and its offset (0 without
// one); the offset's numerator sign is never negative in CLDR, so it is
// unsigned.
typedef struct UnitFactor
{
    Wide numerator;
    Wide denominator;
    Wide offsetNumerator;
    Wide offsetDenominator;
} UnitFactor;

void mlocReadUnitFactor(const UnitTable* t, uint32_t simple, UnitFactor* out);

#endif
