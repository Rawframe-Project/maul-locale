// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The grammatical derivations (src/unit_tables.h) from CLDR's
// supplemental/grammaticalFeatures.xml, a unit of no locale in the unit
// names' file.

#ifndef MLOCGEN_UNIT_DERIVATIONS_GEN_H
#define MLOCGEN_UNIT_DERIVATIONS_GEN_H

#include "datafile.h"

#include <stddef.h>

// A case's index (unitCaseNominative and the others), or -1.
int UnitCaseIndex(const char* name);

// The name of a case's index.
const char* UnitCaseName(int index);

// The name of a gender's index ("" for 0).
const char* UnitGenderName(int index);

// A gender's index (unitGenderAnimate and the others), or -1.
int UnitGenderIndex(const char* name);

// Adds the derivations' unit to a file. Returns 0 and a message on
// failure.
int GenerateUnitDerivations(const char* commonDirectory, DataFile* file, char* errorOut,
                            size_t errorCapacity);

#endif
