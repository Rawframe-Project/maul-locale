// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file's generator.

#ifndef MLOCGEN_SEGMENT_GEN_H
#define MLOCGEN_SEGMENT_GEN_H

#include "datafile.h"
#include "xml.h"

#include <stddef.h>
#include <stdint.h>

// The unit of one file of common/segments, which the caller frees, or no
// unit when the file gives no segmentation. Returns nullptr, or the reason
// the generator does not take the file.
const char* SegmentsUnit(const XmlNode* root, uint8_t** unitOut, size_t* sizeOut);

// Adds a unit to a file for each locale of common/segments whose file
// gives a segmentation. Returns 0 and a message on failure.
int GenerateSegments(const char* commonDirectory, DataFile* file, char* errorOut,
                     size_t errorCapacity);

#endif
