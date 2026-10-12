// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's units (record mloc-0021, src/number_tables.h): the
// numeric numbering systems, the distinct symbol sets and compiled
// patterns, and each locale's numbers, filed under its key where its
// resolved data differs from the next key's along its chain.

#ifndef MLOCGEN_NUMBER_GEN_H
#define MLOCGEN_NUMBER_GEN_H

#include "datafile.h"

// The currency names go into names, a data file of their own (record
// mloc-0022).
int GenerateNumberUnits(const char* commonDirectory, DataFile* file, DataFile* names,
                        char* errorOut, size_t errorCapacity);

#endif
