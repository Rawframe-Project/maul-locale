// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file's unit (segment_tables.h): its validator. A
// unit is checked alone.

#include "segment_tables.h"

#include "bytes.h"
#include "number_data.h"

// Overrides: code points strictly ascending, none a surrogate or past
// U+10FFFF, each class below count.
static bool ValidOverrides(const uint8_t* overrides, uint32_t n, uint32_t classCount)
{
    uint32_t previous = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t codePoint = mlocLoad32(overrides + segmentOverrideSize * (size_t)i);
        uint32_t value = mlocLoad32(overrides + segmentOverrideSize * (size_t)i + 4);
        if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint < 0xE000) ||
            (i > 0 && codePoint <= previous) || value >= classCount)
        {
            return false;
        }
        previous = codePoint;
    }
    return true;
}

// Whether a's bytes read from the end order before b's.
static bool BackwardsBefore(const uint8_t* a, uint32_t aLength, const uint8_t* b, uint32_t bLength)
{
    for (uint32_t i = 1; i <= aLength && i <= bLength; i++)
    {
        if (a[aLength - i] != b[bLength - i])
        {
            return a[aLength - i] < b[bLength - i];
        }
    }
    return aLength < bLength;
}

// The suppressions: offsets ascending from 0 to the text's end, each text
// well-formed, not empty and at most segmentMaxSuppression bytes, in
// strictly ascending order read from the end.
static bool ValidSuppressions(const uint8_t* offsets, uint32_t count, const uint8_t* text,
                              uint32_t textSize)
{
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * (size_t)count) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        if (end <= start || end > textSize || end - start > segmentMaxSuppression ||
            !mlocValidDataText(text + start, end - start, 0))
        {
            return false;
        }
        if (i > 0)
        {
            uint32_t before = mlocLoad32(offsets + 4 * (size_t)i - 4);
            if (!BackwardsBefore(text + before, start - before, text + start, end - start))
            {
                return false;
            }
        }
    }
    return true;
}

bool mlocValidSegments(const DataView* view, DataSpan payload)
{
    (void)view;
    // The header and the one offset of a unit without suppressions.
    if (payload.size < segmentHeaderSize + 4)
    {
        return false;
    }
    uint32_t given = mlocLoad32(payload.bytes);
    uint32_t flags = mlocLoad32(payload.bytes + 4);
    uint32_t words = mlocLoad32(payload.bytes + 8);
    uint32_t sentences = mlocLoad32(payload.bytes + 12);
    uint32_t count = mlocLoad32(payload.bytes + 16);
    uint32_t textSize = mlocLoad32(payload.bytes + 20);
    bool word = (given & segmentGivesWord) != 0;
    bool sentence = (given & segmentGivesSentence) != 0;
    bool line = (given & segmentGivesLine) != 0;
    if (given == 0 || given > (segmentGivesWord | segmentGivesSentence | segmentGivesLine) ||
        flags > (segmentCjIdeographic | segmentJapaneseWords) ||
        ((flags & segmentCjIdeographic) != 0 && !line) ||
        ((flags & segmentJapaneseWords) != 0 && !word) || words > segmentMaxOverrides ||
        sentences > segmentMaxOverrides || count > segmentMaxSuppressions || (words > 0 && !word) ||
        ((sentences > 0 || count > 0) && !sentence))
    {
        return false;
    }
    uint64_t size = segmentHeaderSize + segmentOverrideSize * ((uint64_t)words + sentences) +
                    4 * ((uint64_t)count + 1) + textSize;
    if (size != payload.size)
    {
        return false;
    }
    const uint8_t* wordOverrides = payload.bytes + segmentHeaderSize;
    const uint8_t* sentenceOverrides = wordOverrides + segmentOverrideSize * (size_t)words;
    const uint8_t* offsets = sentenceOverrides + segmentOverrideSize * (size_t)sentences;
    const uint8_t* text = offsets + 4 * ((size_t)count + 1);
    return ValidOverrides(wordOverrides, words, segmentWordClassCount) &&
           ValidOverrides(sentenceOverrides, sentences, segmentSentenceClassCount) &&
           ValidSuppressions(offsets, count, text, textSize);
}
