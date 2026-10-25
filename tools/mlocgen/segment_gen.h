// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file's generator.

#ifndef MLOCGEN_SEGMENT_GEN_H
#define MLOCGEN_SEGMENT_GEN_H

#include "datafile.h"

#include <stddef.h>

// Adds a unit to a file for each locale of common/segments whose file
// gives a segmentation. Returns 0 and a message on failure.
int GenerateSegments(const char* commonDirectory, DataFile* file, char* errorOut,
                     size_t errorCapacity);

#endif
