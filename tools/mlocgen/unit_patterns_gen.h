// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The duration and coordinate patterns of the unit names data file
// (src/unit_tables.h): per locale key, CLDR's resolved values, filed
// where the key's chain would give others.

#ifndef MLOCGEN_UNIT_PATTERNS_GEN_H
#define MLOCGEN_UNIT_PATTERNS_GEN_H

#include "datafile.h"
#include "unit_names_gen.h"

// Adds the patterns units to the unit names file.
void GenerateUnitPatterns(const UnitNamesInput* input, DataFile* file);

#endif
