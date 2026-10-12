// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alias rules canonicalization applies (UTS #35 Annex C), as one
// unit of no locale.

#ifndef MLOCGEN_ALIAS_GEN_H
#define MLOCGEN_ALIAS_GEN_H

#include "datafile.h"

// Adds the aliases unit. Returns 0 and a message on failure.
int GenerateAliasUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
