// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation data file's units (records mloc-0064 and mloc-0065):
// CLDR's root, read from common/uca/FractionalUCA.txt and built into the
// root unit.

#ifndef MLOCGEN_COLLATION_GEN_H
#define MLOCGEN_COLLATION_GEN_H

#include "datafile.h"

#include <stddef.h>

// Adds the collation units to a file. Returns 0 and a message on
// failure.
int GenerateCollation(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
