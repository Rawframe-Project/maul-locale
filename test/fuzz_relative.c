// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the relative time data file's validator (record
// mloc-0043). The input is a unit's payload; one the validator accepts
// must keep every read inside its own bytes: each key inside the layout,
// each text inside the text, no name holding "{0}".

#include "bytes.h"
#include "relative_tables.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static void Check(DataSpan span)
{
    uint32_t count = mlocLoad32(span.bytes);
    uint32_t stringCount = mlocLoad32(span.bytes + 4);
    uint32_t textSize = mlocLoad32(span.bytes + 8);
    const uint8_t* keys = span.bytes + relativeHeaderSize;
    const uint8_t* strings = keys + 2 * (size_t)count;
    const uint8_t* offsets = strings + 2 * (size_t)count;
    const uint8_t* text = offsets + 4 * ((size_t)stringCount + 1);
    Require(text + textSize == span.bytes + span.size);
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t key = mlocLoad16(keys + 2 * (size_t)i);
        uint32_t string = mlocLoad16(strings + 2 * (size_t)i);
        Require(key < relativeKeyCount && string < stringCount);
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)string);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)string + 4);
        Require(start <= end && end <= textSize);
        bool found = false;
        for (uint32_t at = start; at + 3 <= end; at++)
        {
            found = found || memcmp(text + at, "{0}", 3) == 0;
        }
        Require(key % relativeSlots >= relativeFuture || !found);
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
    if (mlocValidRelative(&view, span))
    {
        Check(span);
    }
    return 0;
}
