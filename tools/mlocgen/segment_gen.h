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

enum
{
    segmentGenMaxSets = 64,
    segmentGenMaxSetOverrides = 64,
};

// A set of class overrides of one type: code points strictly ascending,
// each with Maul Unicode's name of its class ("muni_sbSTerm").
typedef struct SegmentGenSet
{
    uint32_t given;
    int count;
    uint32_t codePoints[segmentGenMaxSetOverrides];
    const char* classes[segmentGenMaxSetOverrides];
} SegmentGenSet;

// The sets the units name, each once, in the order first given: the code
// table of src/generated/segment_overrides.c (segment_overrides.h).
typedef struct SegmentGenSets
{
    SegmentGenSet sets[segmentGenMaxSets];
    int count;
} SegmentGenSets;

// The unit of one file of common/segments, which the caller frees, or no
// unit when the file gives no segmentation; its overrides are added to
// sets unless a set there is the same. Returns nullptr, or the reason the
// generator does not take the file.
const char* SegmentsUnit(const XmlNode* root, SegmentGenSets* sets, uint8_t** unitOut,
                         size_t* sizeOut);

// The source of src/generated/segment_overrides.c, which the caller frees.
void SegmentOverridesSource(const SegmentGenSets* sets, char** sourceOut, size_t* sourceSizeOut);

// Adds a unit to a file for each locale of common/segments whose file
// gives a segmentation, and gives the source of the code table its units
// name. Returns 0 and a message on failure.
int GenerateSegments(const char* commonDirectory, DataFile* file, char** sourceOut,
                     size_t* sourceSizeOut, char* errorOut, size_t errorCapacity);

#endif
