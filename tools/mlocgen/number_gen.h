// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's units (src/number_tables.h): the numeric numbering
// systems, the distinct symbol sets and compiled patterns, and each
// locale's numbers, filed under its key where its resolved data differs
// from the next key's along its chain.

#ifndef MLOCGEN_NUMBER_GEN_H
#define MLOCGEN_NUMBER_GEN_H

#include "datafile.h"

// The data files of names, each of its own: the currency names, the unit
// names, the names of languages and the rest and the subdivision names.
typedef struct NameFiles
{
    DataFile currencies;
    DataFile units;
    DataFile locales;
    DataFile subdivisions;
    DataFile dates;
    DataFile zones;
    DataFile relative;
} NameFiles;

int GenerateNumberUnits(const char* commonDirectory, DataFile* file, NameFiles* names,
                        char* errorOut, size_t errorCapacity);

#endif
