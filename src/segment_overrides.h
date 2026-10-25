// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The class overrides of CLDR's segmentation tailorings, a code table that
// src/generated/segment_overrides.c defines from common/segments. A
// segments unit names its word and sentence overrides by a set's number
// here (segment_tables.h), and Maul Unicode reads them in place.

#ifndef MLOC_SEGMENT_OVERRIDES_H
#define MLOC_SEGMENT_OVERRIDES_H

#include "maul-locale/segment.h"
#include "maul-unicode/segment.h"

#include <stdint.h>

// A set: its overrides from first in mlocSegmentOverrides, strictly
// ascending by code point, and the segmentation they belong to
// (mloc_segmentWord or mloc_segmentSentence).
typedef struct SegmentOverrideSet
{
    uint16_t first;
    uint8_t count;
    uint8_t kind;
} SegmentOverrideSet;

extern const muniBreakClassOverride mlocSegmentOverrides[];
extern const SegmentOverrideSet mlocSegmentOverrideSets[];
extern const uint32_t mlocSegmentOverrideSetCount;

#endif
