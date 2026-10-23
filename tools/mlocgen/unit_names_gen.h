// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names (src/unit_tables.h): the identifiers the locale files
// name, and per locale key each width's names, the long width's extras and
// its patterns of the grammatical cases, the values ICU's data gives each
// filed where the key's chain would give another. They ship as a data file
// of their own.

#ifndef MLOCGEN_UNIT_NAMES_GEN_H
#define MLOCGEN_UNIT_NAMES_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "keys.h"

#include <stddef.h>

typedef struct UnitNamesInput
{
    const char* commonDirectory;
    Cldr* cldr;
    const Keys* keys;
    // The ids of common/main's files, sorted.
    const char* const* ids;
    int count;
} UnitNamesInput;

// Adds the unit names units to a file. Returns 0 and a message on
// failure.
int GenerateUnitNames(const UnitNamesInput* input, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
