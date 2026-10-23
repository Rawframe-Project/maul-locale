// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The relative time data file's generator.

#ifndef MLOCGEN_RELATIVE_GEN_H
#define MLOCGEN_RELATIVE_GEN_H

#include "datafile.h"
#include "names_gen.h"

#include <stddef.h>

// Adds each locale key's relative time unit to a file. Returns 0 and a
// message on failure.
int GenerateRelative(const NamesInput* input, DataFile* file, char* errorOut, size_t errorCapacity);

#endif
