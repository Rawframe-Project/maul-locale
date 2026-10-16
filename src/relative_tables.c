// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The relative time data file's units (relative_tables.h): their
// validator. A unit is checked alone.

#include "relative_tables.h"

#include "bytes.h"
#include "number_data.h"

#include <string.h>

// How many times "{0}" occurs in text, at most two counted.
static uint32_t Placeholders(const uint8_t* text, uint32_t length)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i + 3 <= length && count < 2; i++)
    {
        count += memcmp(text + i, "{0}", 3) == 0;
    }
    return count;
}

// The strings: offsets ascending from 0 to the text's end, each text
// well-formed.
static bool ValidStrings(const uint8_t* offsets, uint32_t stringCount, const uint8_t* text,
                         uint32_t textSize)
{
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * (size_t)stringCount) != textSize)
    {
        return false;
    }
    for (uint32_t i = 0; i < stringCount; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        if (end < start || end > textSize || !mlocValidDataText(text + start, end - start, 0))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidRelative(const DataView* view, DataSpan payload)
{
    (void)view;
    // The header and the one offset of a unit without strings.
    if (payload.size < relativeHeaderSize + 4)
    {
        return false;
    }
    uint32_t count = mlocLoad32(payload.bytes);
    uint32_t stringCount = mlocLoad32(payload.bytes + 4);
    uint32_t textSize = mlocLoad32(payload.bytes + 8);
    uint64_t size =
        relativeHeaderSize + 4 * (uint64_t)count + 4 * ((uint64_t)stringCount + 1) + textSize;
    if (count > relativeKeyCount || stringCount > relativeKeyCount || size != payload.size)
    {
        return false;
    }
    const uint8_t* keys = payload.bytes + relativeHeaderSize;
    const uint8_t* strings = keys + 2 * (size_t)count;
    const uint8_t* offsets = strings + 2 * (size_t)count;
    const uint8_t* text = offsets + 4 * ((size_t)stringCount + 1);
    if (!ValidStrings(offsets, stringCount, text, textSize))
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t key = mlocLoad16(keys + 2 * (size_t)i);
        uint32_t string = mlocLoad16(strings + 2 * (size_t)i);
        if (key >= relativeKeyCount || (i > 0 && key <= mlocLoad16(keys + 2 * (size_t)i - 2)) ||
            string >= stringCount)
        {
            return false;
        }
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)string);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)string + 4);
        bool pattern = key % relativeSlots >= relativeFuture;
        if (Placeholders(text + start, end - start) > (pattern ? 1U : 0U))
        {
            return false;
        }
    }
    return true;
}
