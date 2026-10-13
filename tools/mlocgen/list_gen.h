// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list patterns (src/list_tables.h): the distinct pattern sets, and
// per locale key the set of each style, CLDR's resolved values filed
// where the key's chain would give others. They are compiled in.

#ifndef MLOCGEN_LIST_GEN_H
#define MLOCGEN_LIST_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "keys.h"

typedef struct ListInput
{
    Cldr* cldr;
    const Keys* keys;
    // The ids of common/main's files, sorted.
    const char* const* ids;
    int count;
} ListInput;

// Adds the list units to a file.
void GenerateListUnits(const ListInput* input, DataFile* file);

#endif
