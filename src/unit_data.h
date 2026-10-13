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
    const uint8_t* dimensions;
    const uint8_t* quantities;
    const uint8_t* beaufort;
    const uint8_t* text;
    const uint8_t* numbers;
    uint32_t simpleCount;
    uint32_t prefixCount;
    uint32_t aliasCount;
    uint32_t baseCount;
    uint32_t beaufortCount;
    uint32_t dimensionCount;
    uint32_t quantityCount;
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

// A base unit's record: its numerator's and denominator's exponents at
// baseNumerator and baseDenominator (unit_tables.h).
const uint8_t* mlocBaseUnit(const UnitTable* t, uint32_t base);

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

bool mlocValidUnitPreferences(const DataView* view, DataSpan payload);

// The parts of a valid unit preferences unit.
typedef struct UnitPrefsTable
{
    const uint8_t* sets;
    const uint8_t* runs;
    const uint8_t* entries;
    const uint8_t* text;
    uint32_t setCount;
    uint32_t runCount;
    uint32_t entryCount;
} UnitPrefsTable;

UnitPrefsTable mlocReadUnitPrefs(DataSpan unit);

// The set of a category and usage, or -1; with a null usage, the first set
// of the category, or -1 when it has none.
int32_t mlocFindUnitPrefSet(const UnitPrefsTable* t, const char* category, size_t categoryLength,
                            const char* usage, size_t usageLength);

// The run of a set for a region, else its run for 001.
uint32_t mlocFindUnitPrefRun(const UnitPrefsTable* t, uint32_t set, uint32_t region);

// A run's first entry and its entry count.
uint32_t mlocUnitPrefEntries(const UnitPrefsTable* t, uint32_t run, const uint8_t** firstOut);

#endif
