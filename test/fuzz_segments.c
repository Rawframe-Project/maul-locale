// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the segmentation unit's validator. The input is a
// payload; a unit the validator accepts must name override sets of the
// code table of their types, and give every entry from inside its own
// bytes: each entry's text inside the text, not empty and at most a
// segmenter's look-back long, each backward value known.

#include "bytes.h"
#include "segment_overrides.h"
#include "segment_tables.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void CheckOverrides(uint32_t field, mlocSegmentKind kind)
{
    Require(field == 0 || (field <= mlocSegmentOverrideSetCount &&
                           mlocSegmentOverrideSets[field - 1].kind == kind));
}

static void Check(DataSpan span)
{
    uint32_t words = mlocLoad32(span.bytes + 8);
    uint32_t sentences = mlocLoad32(span.bytes + 12);
    uint32_t backward = mlocLoad32(span.bytes + 16);
    uint32_t forward = mlocLoad32(span.bytes + 20);
    uint32_t textSize = mlocLoad32(span.bytes + 24);
    Require(backward <= segmentMaxSuppressions && forward <= segmentMaxSuppressions);
    const uint8_t* values = span.bytes + segmentHeaderSize;
    const uint8_t* offsets = values + 4 * (size_t)backward;
    const uint8_t* text = offsets + 4 * ((size_t)backward + forward + 1);
    Require(text + textSize == span.bytes + span.size);
    CheckOverrides(words, mloc_segmentWord);
    CheckOverrides(sentences, mloc_segmentSentence);
    for (uint32_t i = 0; i < backward + forward; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        Require(start < end && end <= textSize && end - start <= segmentMaxSuppression);
        Require(i >= backward || mlocLoad32(values + 4 * (size_t)i) <= segmentSuppressionPartial);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Aligned, as a payload in a data file is.
    static uint64_t s_bytes[1024];
    if (size > sizeof(s_bytes))
    {
        return 0;
    }
    memcpy(s_bytes, data, size);
    DataSpan span = {(const uint8_t*)s_bytes, (uint32_t)size};
    DataView view = {0};
    if (mlocValidSegments(&view, span))
    {
        Check(span);
    }
    return 0;
}
