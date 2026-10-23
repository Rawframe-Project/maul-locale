// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The explicit parent locales of each component as a unit of no locale,
// and CLDR's own inheritance chains by key, which the library's chains are
// checked against.

#ifndef MLOCGEN_PARENTS_GEN_H
#define MLOCGEN_PARENTS_GEN_H

#include "datafile.h"

// Adds the parents unit. Returns 0 and a message on failure.
int GenerateParentUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                       size_t errorCapacity);

// Reads lines "main<TAB>id" or "collation<TAB>id" from standard input
// and writes, for each, the line, a tab and the keys of the locale's
// CLDR parents up to root, each once, separated by spaces. Returns 0 and a message on
// failure.
int PrintChains(const char* commonDirectory, char* errorOut, size_t errorCapacity);

#endif
