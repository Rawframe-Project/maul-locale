// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names (record mloc-0036, src/names_tables.h): the codes the locale
// files name, and per locale key the names of languages, scripts,
// regions, variants, keys and their values, and the display patterns,
// each value CLDR resolves filed where the key's chain would give
// another. They ship as a data file of their own.

#ifndef MLOCGEN_NAMES_GEN_H
#define MLOCGEN_NAMES_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "keys.h"

#include <stddef.h>

typedef struct NamesInput
{
    const char* commonDirectory;
    Cldr* cldr;
    const Keys* keys;
    // The ids of common/main's files, sorted.
    const char* const* ids;
    int count;
} NamesInput;

// Adds the names units to a file. Returns 0 and a message on failure.
int GenerateNames(const NamesInput* input, DataFile* file, char* errorOut, size_t errorCapacity);

#endif
