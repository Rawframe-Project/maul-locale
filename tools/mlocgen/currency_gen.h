// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's currency units (record mloc-0022,
// src/currency_tables.h): the fractions of supplementalData.xml, and per
// locale key the currency symbols, own patterns and own separators, and
// the names, its file gives, each field kept only where it differs from
// what the parent's chain gives. The pattern sets are the number
// generator's.

#ifndef MLOCGEN_CURRENCY_GEN_H
#define MLOCGEN_CURRENCY_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "keys.h"

#include <stdint.h>

typedef struct CurrencyInput
{
    const char* commonDirectory;
    Cldr* cldr;
    const Keys* keys;
    // The ids of common/main's files, sorted.
    const char* const* ids;
    int count;
    // Compiles a currency's own pattern into the patterns unit, giving its
    // index.
    uint16_t (*compile)(void* context, const char* pattern);
    void* context;
    // Receives the names units, which ship as a data file of their own.
    DataFile* names;
} CurrencyInput;

int GenerateCurrencyUnits(const CurrencyInput* input, DataFile* file, char* errorOut,
                          size_t errorCapacity);

#endif
