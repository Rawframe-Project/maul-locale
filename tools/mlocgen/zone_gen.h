// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone tables (record mloc-0041) from the pinned tzdb source.

#ifndef MLOCGEN_ZONE_GEN_H
#define MLOCGEN_ZONE_GEN_H

#include "datafile.h"

#include <stddef.h>
#include <stdint.h>

// Prints each zone's offsets up to the start of a year, for the checks
// against zic and zdump: "zone NAME STANDARD SAVE" with the offset
// before the first transition, then "at SECONDS STANDARD SAVE" for each
// transition, the closing rule's included. Returns 0 and a message on
// failure.
int PrintZones(const char* tzdbDirectory, int32_t untilYear, char* errorOut, size_t errorCapacity);

// Adds the zones unit (src/zone_tables.h) to a data file, from a tzdb
// source directory holding the pinned release and CLDR's common
// directory (whose bcp47/timezone.xml gives Links' countries). Returns 0
// and a message on failure: another release, a zone the compiler refuses,
// a zone whose rearguard form gives other offsets than its main form,
// identifiers zone_ids.h refuses, or a count past the unit's fields.
int GenerateZones(const char* tzdbDirectory, const char* commonDirectory, const char* release,
                  DataFile* file, char* errorOut, size_t errorCapacity);

// Writes a zones file (record mloc-0041): a data file holding the zones
// unit alone, compiled from a tzdb source directory of any release, with
// the version of the library this generator links but for that release.
// A primary other than CLDR's is reported on standard error, as a newer
// release may differ from CLDR. Returns 0 and a message on failure.
int WriteZonesFile(const char* tzdbDirectory, const char* commonDirectory, const char* path,
                   char* errorOut, size_t errorCapacity);

#endif
