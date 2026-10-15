// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writes the library's generated sources from CLDR: the code tables, the
// data version they make (src/generated/data_version.h) and the
// compiled-in data (src/generated/builtin_data.c). The data version's
// hash covers the pinned releases (tools/cldr-release.txt,
// tools/tzdb-release.txt) and every generated code table, so data from
// any other generation is refused (record mloc-0005).

#ifndef MLOCGEN_GENERATE_H
#define MLOCGEN_GENERATE_H

#include "datafile.h"

// Reads the pins under the repository's root and fills the version the
// generated sources will carry. Returns 0 and a message on failure.
int GenerateVersion(const char* root, DataFileVersion* versionOut, char* errorOut,
                    size_t errorCapacity);

// Writes src/generated/ under root from CLDR's common directory and a
// tzdb source directory. Returns 0 and a message on failure.
int Generate(const char* commonDirectory, const char* root, const char* tzdbDirectory,
             char* errorOut, size_t errorCapacity);

#endif
