// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the date patterns unit's validator (record
// mloc-0042). The input is a date patterns payload; one the validator
// accepts must give every pattern from inside its text, well formed, and
// every row's and set's pattern by index.

#include "bytes.h"
#include "date_data.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
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
    if (!mlocValidDatePatterns(&view, span))
    {
        return 0;
    }
    DatePatterns p = mlocReadDatePatterns(span);
    for (uint32_t i = 0; i < p.patternCount; i++)
    {
        DataSpan pattern;
        Require(mlocDatePattern(&p, i, &pattern));
        Require(pattern.bytes >= span.bytes && pattern.bytes + pattern.size <= span.bytes + size);
        Require(mlocValidDatePattern(pattern.bytes, pattern.size));
    }
    for (uint32_t r = 0; r < p.rowCount * dateLengthCount; r++)
    {
        DataSpan pattern;
        Require(mlocDatePattern(&p, mlocLoad16(p.rows + 2 * (size_t)r), &pattern));
    }
    for (uint32_t s = 0; s < p.setCount; s++)
    {
        const uint8_t* set = p.sets + (size_t)s * dateSetSize;
        for (uint32_t e = 0; e < dateEntryCount; e++)
        {
            Require(mlocLoad16(set + 2 * (size_t)e) < p.rowCount);
        }
        for (uint32_t i = 0; i < dateStandardCount; i++)
        {
            DataSpan pattern;
            Require(
                mlocDatePattern(&p, mlocLoad16(set + 2 * (dateEntryCount + (size_t)i)), &pattern));
        }
    }
    return 0;
}
