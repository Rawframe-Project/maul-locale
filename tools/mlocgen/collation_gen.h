// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation data file's units: CLDR's root, read from
// common/uca/FractionalUCA.txt and built into the root unit, and each
// common/collation file's types built over it into a unit of the file's
// locale key. A type kept for imports (CLDR's "private-") is left out; a
// type the file also gives as alt="short" (zh's) takes that form, as
// ICU4C's data does, and other alts are left out. The unihan types are
// built over the unihan root, and the radical-stroke Han table they read
// is a unit of no locale.

#ifndef MLOCGEN_COLLATION_GEN_H
#define MLOCGEN_COLLATION_GEN_H

#include "datafile.h"

#include <stddef.h>

// Adds the collation units to a file. Returns 0 and a message on
// failure.
int GenerateCollation(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
