// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The index labels (src/collation_index.h, record mloc-0075): each
// locale's set as ICU4C's AlphabeticIndex takes it from the locale, its
// index exemplars along the main chain, else its standard exemplars made
// into labels. A locale whose set matches that of the nearest locale
// with another key along its chain files nothing; one under its
// parent's key is checked to match.

#ifndef MLOCGEN_INDEX_GEN_H
#define MLOCGEN_INDEX_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "keys.h"

typedef struct IndexInput
{
    Cldr* cldr;
    const Keys* keys;
    // The ids of common/main's files, sorted.
    const char* const* ids;
    int count;
} IndexInput;

// Adds the index label units to a file (the collation data file).
void GenerateIndexUnits(const IndexInput* input, DataFile* file);

#endif
