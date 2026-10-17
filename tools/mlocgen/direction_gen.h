// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The scripts written right to left, from properties/scriptMetadata.txt's
// RTL field, as the source of src/generated/script_directions.c (a
// locale's direction, record mloc-0045).

#ifndef MLOCGEN_DIRECTION_GEN_H
#define MLOCGEN_DIRECTION_GEN_H

#include <stddef.h>

// The source of src/generated/script_directions.c in *sourceOut, which
// the caller frees. Returns 0 and a message on failure: an unreadable
// file, a line without the RTL field, a malformed script code, a code
// twice, or no script written right to left.
int GenerateScriptDirections(const char* commonDirectory, char** sourceOut, size_t* sourceSizeOut,
                             char* errorOut, size_t errorCapacity);

// The same from scriptMetadata.txt's text, for the tests.
int ScriptDirectionsSource(const char* metadata, char** sourceOut, size_t* sourceSizeOut,
                           char* errorOut, size_t errorCapacity);

#endif
