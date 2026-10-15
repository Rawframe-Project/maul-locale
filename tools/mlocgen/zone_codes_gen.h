// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone codes unit (record mloc-0041, src/zone_code_tables.h): CLDR's
// BCP 47 time zone ids from bcp47/timezone.xml and the Windows zone names
// from supplemental/windowsZones.xml.

#ifndef MLOCGEN_ZONE_CODES_GEN_H
#define MLOCGEN_ZONE_CODES_GEN_H

#include "datafile.h"

#include <stddef.h>

// Adds the unit to the file. Returns 1, or 0 and a message: a file that
// cannot be read, a deprecated id whose replacement is missing or itself
// deprecated, a name two ids list, a Windows name without a 001 mapping,
// a mapped identifier no id lists, or an id mapped to two Windows names.
int GenerateZoneCodes(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
