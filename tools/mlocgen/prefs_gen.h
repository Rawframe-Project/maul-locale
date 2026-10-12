// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The regional preferences from supplemental/supplementalData.xml: the
// hours of timeData (by region, and by language and region), weekData's
// first day, minimal days and weekend, and measurementData's system and
// paper size, each region's fields resolved against 001's. Alternative
// values (alt="variant") are left out. The layout is
// src/locale_tables.h's.

#ifndef MLOCGEN_PREFS_GEN_H
#define MLOCGEN_PREFS_GEN_H

#include "datafile.h"

// Adds the preferences unit. Returns 0 and a message on failure.
int GeneratePreferenceUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                           size_t errorCapacity);

#endif
