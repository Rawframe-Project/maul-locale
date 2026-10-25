// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The segmentation data file's unit (segment_tables.h): its validator. A
// unit is checked alone.

#include "segment_tables.h"

#include "bytes.h"
#include "number_data.h"
#include "segment_overrides.h"

// An override field: none, or a set of the kind's in the code table.
static bool ValidOverrides(uint32_t field, mlocSegmentKind kind)
{
    return field == 0 || (field <= mlocSegmentOverrideSetCount &&
                          mlocSegmentOverrideSets[field - 1].kind == kind);
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

// Whether a's bytes order before b's.
static bool ForwardsBefore(const uint8_t* a, uint32_t aLength, const uint8_t* b, uint32_t bLength)
{
    for (uint32_t i = 0; i < aLength && i < bLength; i++)
    {
        if (a[i] != b[i])
        {
            return a[i] < b[i];
        }
    }
    return aLength < bLength;
}

// The entries: offsets ascending from 0 to the text's end, each text
// well-formed, not empty and at most segmentMaxSuppression bytes; the
// backward entries in strictly ascending order read from the end, each
// value known, and the forward entries in strictly ascending order.
static bool ValidEntries(const uint8_t* values, const uint8_t* offsets, uint32_t backward,
                         uint32_t forward, const uint8_t* text, uint32_t textSize)
{
    uint32_t count = backward + forward;
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * (size_t)count) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        if (end <= start || end > textSize || end - start > segmentMaxSuppression ||
            !mlocValidDataText(text + start, end - start, 0) ||
            (i < backward && mlocLoad32(values + 4 * (size_t)i) > segmentSuppressionPartial))
        {
            return false;
        }
        if (i > 0 && i != backward)
        {
            uint32_t before = mlocLoad32(offsets + 4 * (size_t)i - 4);
            bool ordered =
                i < backward
                    ? BackwardsBefore(text + before, start - before, text + start, end - start)
                    : ForwardsBefore(text + before, start - before, text + start, end - start);
            if (!ordered)
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
    uint32_t backward = mlocLoad32(payload.bytes + 16);
    uint32_t forward = mlocLoad32(payload.bytes + 20);
    uint32_t textSize = mlocLoad32(payload.bytes + 24);
    bool word = (given & segmentGivesWord) != 0;
    bool sentence = (given & segmentGivesSentence) != 0;
    bool line = (given & segmentGivesLine) != 0;
    if (given == 0 || given > (segmentGivesWord | segmentGivesSentence | segmentGivesLine) ||
        flags > (segmentCjIdeographic | segmentJapaneseWords) ||
        ((flags & segmentCjIdeographic) != 0 && !line) ||
        ((flags & segmentJapaneseWords) != 0 && !word) || backward > segmentMaxSuppressions ||
        forward > segmentMaxSuppressions || (words > 0 && !word) ||
        ((sentences > 0 || backward > 0 || forward > 0) && !sentence) ||
        !ValidOverrides(words, mloc_segmentWord) ||
        !ValidOverrides(sentences, mloc_segmentSentence))
    {
        return false;
    }
    uint64_t size = segmentHeaderSize + 4 * (uint64_t)backward +
                    4 * ((uint64_t)backward + forward + 1) + textSize;
    if (size != payload.size)
    {
        return false;
    }
    const uint8_t* values = payload.bytes + segmentHeaderSize;
    const uint8_t* offsets = values + 4 * (size_t)backward;
    const uint8_t* text = offsets + 4 * ((size_t)backward + forward + 1);
    return ValidEntries(values, offsets, backward, forward, text, textSize);
}
