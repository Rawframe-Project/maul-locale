// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The plural rules (record mloc-0020) from supplemental/plurals.xml,
// ordinals.xml and pluralRanges.xml: each distinct rule set compiled to
// a C function, cardinal sets first, and for every locale id the files
// name a unit with its rules and range table, resolved by truncation.

#ifndef MLOCGEN_PLURAL_GEN_H
#define MLOCGEN_PLURAL_GEN_H

#include "datafile.h"

// Adds the plural units and returns the source of
// src/generated/plural_rules.c in *sourceOut, which the caller frees.
// Returns 0 and a message on failure.
int GeneratePluralUnits(const char* commonDirectory, DataFile* file, char** sourceOut,
                        size_t* sourceSizeOut, char* errorOut, size_t errorCapacity);

// Prints every sample of both rule files, one per line: the type
// (cardinal or ordinal), a locale id, the category and the sample, tab
// separated, for each locale of its rule set. Returns 0 and a message on
// failure.
int PrintPluralSamples(const char* commonDirectory, char* errorOut, size_t errorCapacity);

#endif
