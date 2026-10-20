// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation data file's units (records mloc-0064, mloc-0065 and
// mloc-0071): CLDR's root, read from common/uca/FractionalUCA.txt and
// built into the root unit, and each common/collation file's types built
// over it into a unit of the file's locale key. A type kept for imports
// (CLDR's "private-") or an alt is left out.

#ifndef MLOCGEN_COLLATION_GEN_H
#define MLOCGEN_COLLATION_GEN_H

#include "datafile.h"

#include <stddef.h>

// Adds the collation units to a file. Returns 0 and a message on
// failure.
int GenerateCollation(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
