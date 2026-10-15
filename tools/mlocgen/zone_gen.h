// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone tables (record mloc-0041) from the pinned tzdb source.

#ifndef MLOCGEN_ZONE_GEN_H
#define MLOCGEN_ZONE_GEN_H

#include <stddef.h>
#include <stdint.h>

// Prints each zone's offsets up to the start of a year, for the checks
// against zic and zdump: "zone NAME STANDARD SAVE" with the offset
// before the first transition, then "at SECONDS STANDARD SAVE" for each
// transition, the closing rule's included. Returns 0 and a message on
// failure.
int PrintZones(const char* tzdbDirectory, int32_t untilYear, char* errorOut, size_t errorCapacity);

// The source of src/generated/zone_data.c from a tzdb source directory
// holding the pinned release, in *sourceOut, which the caller frees.
// Returns 0 and a message on failure: another release, a zone the
// compiler refuses, a zone whose rearguard form gives other offsets than
// its main form, or a count past the table's fields.
int GenerateZones(const char* tzdbDirectory, const char* release, char** sourceOut,
                  size_t* sourceSizeOut, char* errorOut, size_t errorCapacity);

#endif
