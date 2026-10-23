// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A unit's unsafe-backward ranges: the code points after the first of each
// of its contractions, as ICU4C's unsafeBackwardSet holds them. A position
// before one of them, or before a mark (lead combining class not 0), may
// be inside a contraction, so a search moved there backs up to a safe one
// first.
//
// In a unit the ranges are a section after the fast Latin table's: a
// 32-bit count, then that many ranges of two 32-bit words, the first
// code point and the last, rising and apart (each starts past the one
// before ends), all within U+10FFFF. A unit that ends after the table
// has none.

#ifndef MLOC_COLLATION_UNSAFE_H
#define MLOC_COLLATION_UNSAFE_H

#include "bytes.h"

#include <stdint.h>

// Reads the section at an offset up to the unit's end: none when the
// unit ends there.
static inline bool mlocReadCollationUnsafe(const uint8_t* p, uint64_t at, uint64_t size,
                                           const uint8_t** rangesOut, uint32_t* countOut)
{
    *rangesOut = nullptr;
    *countOut = 0;
    if (at == size)
    {
        return true;
    }
    if (at + 4 > size)
    {
        return false;
    }
    uint32_t count = mlocLoad32(p + at);
    if (at + 4 + (uint64_t)count * 8 != size)
    {
        return false;
    }
    const uint8_t* ranges = p + at + 4;
    bool ok = true;
    for (uint32_t i = 0; ok && i < count; i++)
    {
        uint32_t first = mlocLoad32(ranges + (size_t)i * 8);
        uint32_t last = mlocLoad32(ranges + (size_t)i * 8 + 4);
        ok = first <= last && last <= 0x10FFFF &&
             (i == 0 || first > mlocLoad32(ranges + (size_t)i * 8 - 4));
    }
    *rangesOut = ranges;
    *countOut = count;
    return ok;
}

// Whether the ranges hold a code point.
static inline bool mlocCollationInUnsafe(const uint8_t* ranges, uint32_t count, uint32_t codePoint)
{
    uint32_t lo = 0;
    uint32_t hi = count;
    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo) / 2;
        if (codePoint < mlocLoad32(ranges + (size_t)mid * 8))
        {
            hi = mid;
        }
        else if (codePoint > mlocLoad32(ranges + (size_t)mid * 8 + 4))
        {
            lo = mid + 1;
        }
        else
        {
            return true;
        }
    }
    return false;
}

#endif
