// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The locale part's units: likely subtags from
// supplemental/likelySubtags.xml, packed as src/locale_tables.h lays
// them out.

#ifndef MLOCGEN_LOCALE_GEN_H
#define MLOCGEN_LOCALE_GEN_H

#include "datafile.h"

// Adds the locale part's units of no locale. Returns 0 and a message on
// failure.
int GenerateLocaleUnits(const char* commonDirectory, DataFile* file, char* errorOut,
                        size_t errorCapacity);

#endif
