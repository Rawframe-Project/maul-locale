// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The aliases of extension values canonicalization replaces, as one
// unit of no locale.

#ifndef MLOCGEN_EXTENSION_GEN_H
#define MLOCGEN_EXTENSION_GEN_H

#include "datafile.h"

// Adds the extension aliases unit. Returns 0 and a message on failure.
int GenerateExtensionUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                          size_t errorCapacity);

#endif
