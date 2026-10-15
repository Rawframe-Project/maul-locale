// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units (src/date_tables.h): each locale's semantic date
// patterns in every calendar, matched by skeleton.c from UTS #35's
// mapping (semantic.h), and its standard date formats, compiled and
// shared; per locale key the set of each calendar, filed where the key's
// chain would give others (record mloc-0042).

#ifndef MLOCGEN_DATE_GEN_H
#define MLOCGEN_DATE_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "keys.h"

#include <stddef.h>

typedef struct DateInput
{
    const char* commonDirectory;
    Cldr* cldr;
    const Keys* keys;
    // The ids of common/main's files, sorted.
    const char* const* ids;
    int count;
} DateInput;

// Adds the date pattern units to a file.
int GenerateDateUnits(const DateInput* input, DataFile* file, char* errorOut, size_t errorCapacity);

// Compiles a pattern (src/date_tables.h) into out, its numeric fields
// written by a numbering override ("hanidec", "d=hanidays") where it
// applies; returns its length, or 0 for a pattern that cannot be
// compiled.
size_t DateCompilePattern(const char* pattern, const char* numbers, char* out, size_t capacity);

#endif
