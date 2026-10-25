// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the segmentation unit's validator. The input is a
// payload; a unit the validator accepts must give every override and
// every entry from inside its own bytes: code points scalar values and
// classes their property's, each entry's text inside the text, not empty
// and at most a segmenter's look-back long, each backward value known.

#include "bytes.h"
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

static void CheckOverrides(const uint8_t* overrides, uint32_t count, uint32_t classCount)
{
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t codePoint = mlocLoad32(overrides + segmentOverrideSize * (size_t)i);
        uint32_t value = mlocLoad32(overrides + segmentOverrideSize * (size_t)i + 4);
        Require(codePoint <= 0x10FFFF && (codePoint < 0xD800 || codePoint > 0xDFFF) &&
                value < classCount);
    }
}

static void Check(DataSpan span)
{
    uint32_t words = mlocLoad32(span.bytes + 8);
    uint32_t sentences = mlocLoad32(span.bytes + 12);
    uint32_t backward = mlocLoad32(span.bytes + 16);
    uint32_t forward = mlocLoad32(span.bytes + 20);
    uint32_t textSize = mlocLoad32(span.bytes + 24);
    Require(words <= segmentMaxOverrides && sentences <= segmentMaxOverrides &&
            backward <= segmentMaxSuppressions && forward <= segmentMaxSuppressions);
    const uint8_t* wordOverrides = span.bytes + segmentHeaderSize;
    const uint8_t* sentenceOverrides = wordOverrides + segmentOverrideSize * (size_t)words;
    const uint8_t* values = sentenceOverrides + segmentOverrideSize * (size_t)sentences;
    const uint8_t* offsets = values + 4 * (size_t)backward;
    const uint8_t* text = offsets + 4 * ((size_t)backward + forward + 1);
    Require(text + textSize == span.bytes + span.size);
    CheckOverrides(wordOverrides, words, segmentWordClassCount);
    CheckOverrides(sentenceOverrides, sentences, segmentSentenceClassCount);
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
