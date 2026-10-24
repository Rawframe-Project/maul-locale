// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin table, ICU4C's CollationFastLatin format version 2:
// 16-bit words, built by the generator for each unit.
//
//   word 0  the version (2) in bits 15..8 and the header's length (5) in
//           7..0
//   1..4    for the max variable groups space, punct, symbol and
//           currency, the highest long mini primary each covers
//   5..452  a mini CE for each fast character: U+0000 to U+017F, then
//           U+2000 to U+203F
//   then    expansions (two mini CEs) and contraction lists, which the
//           character words name by an index after the 448
//
// A mini CE: below 0x400 a special value (0 ignorable, 1 bail out, 2 end
// of string, 3 merge) or a secondary CE (bits 9..5, case 4..3, tertiary
// 2..0); 0x400 a contraction (index in bits 9..0); 0x800 an expansion
// (index in 9..0); 0xC00 to 0xFF8 a long primary (bits 15..3) with its
// tertiary; from 0x1000 a short primary (bits 15..10), secondary, case
// and tertiary. A contraction list's words: the next character's index
// (bits 8..0, 0x1FF for the default and for the list's end) and the
// result's length in 10..9 (1 bail out, 2 one mini CE, 3 two), then the
// result's words; the list starts with its default.
//
// In a unit the table is a section after its arrays: a 32-bit count and
// that many words, then the unsafe-backward ranges (collation_unsafe.h). A unit that ends after its
// arrays has none, nor has one whose count is 0 (the general compare always runs); a tailoring's
// count fastLatinRootTable says that its table is the root's.
//
// A data layout: tools/check_data_format.py watches this header.

#ifndef MLOC_COLLATION_FAST_LATIN_H
#define MLOC_COLLATION_FAST_LATIN_H

#include "bytes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    fastLatinVersion = 2,
    fastLatinHeader = 5,
    fastLatinMax = 0x17F,
    fastLatinLimit = 0x180,
    fastPunctStart = 0x2000,
    fastPunctLimit = 0x2040,
    fastLatinChars = 448,
    fastShortPrimaryMask = 0xFC00,
    fastIndexMask = 0x3FF,
    fastSecondaryMask = 0x3E0,
    fastCaseMask = 0x18,
    fastLongPrimaryMask = 0xFFF8,
    fastTertiaryMask = 7,
    fastContraction = 0x400,
    fastExpansion = 0x800,
    fastMinLong = 0xC00,
    fastLongInc = 8,
    fastMaxLong = 0xFF8,
    fastMinShort = 0x1000,
    fastShortInc = 0x400,
    fastMaxShort = 0xFC00,
    fastMinSecBefore = 0,
    fastSecInc = 0x20,
    fastMaxSecBefore = 0x80,
    fastCommonSec = 0xA0,
    fastMinSecAfter = 0xC0,
    fastMaxSecAfter = 0x160,
    fastMinSecHigh = 0x180,
    fastMaxSecHigh = 0x3E0,
    fastSecOffset = 0x20,
    fastLowerCase = 8,
    fastCommonTer = 0,
    fastMaxTerAfter = 7,
    fastTerOffset = 0x20,
    fastMergeWeight = 3,
    fastEndOfString = 2,
    fastBailOut = 1,
    fastContractionCharMask = 0x1FF,
    fastContractionLengthShift = 9,
    fastLatinRootTable = 1,
};

// The word at an index of a table's bytes.
static inline uint32_t mlocFastLatinWord(const uint8_t* words, uint32_t at)
{
    return mlocLoad16(words + (size_t)at * 2);
}

// Whether words a compare reads as mini CEs are none of them the end of
// string mark, which would end a text early and leave the rest of it
// unchecked for the later levels.
static inline bool mlocFastLatinMinisValid(const uint8_t* words, uint32_t at, uint32_t length)
{
    bool ok = true;
    for (uint32_t i = 0; ok && i < length; i++)
    {
        ok = mlocFastLatinWord(words, at + i) != fastEndOfString;
    }
    return ok;
}

// Whether a contraction list stays inside the table: from its default,
// entries by their lengths up to the next one of character 0x1FF, the
// word the compare stops on; no result the end of string mark.
static inline bool mlocFastLatinListValid(const uint8_t* words, uint32_t count, uint32_t at)
{
    bool ok = at < count &&
              (mlocFastLatinWord(words, at) & fastContractionCharMask) == fastContractionCharMask;
    bool first = true;
    while (ok)
    {
        uint32_t head = mlocFastLatinWord(words, at);
        uint32_t length = head >> fastContractionLengthShift;
        if (!first && (head & fastContractionCharMask) == fastContractionCharMask)
        {
            return true;
        }
        ok = length >= 1 && length <= 3 && (uint64_t)at + length < count &&
             mlocFastLatinMinisValid(words, at + 1, length - 1);
        at += length;
        first = false;
    }
    return false;
}

// Whether a table's words hold together: its head word, every
// character's expansion and contraction list inside it, and no mini CE
// the end of string mark.
static inline bool mlocFastLatinValid(const uint8_t* words, uint32_t count)
{
    if (count < fastLatinHeader + fastLatinChars ||
        mlocFastLatinWord(words, 0) != (fastLatinVersion << 8 | fastLatinHeader))
    {
        return false;
    }
    bool ok = true;
    for (uint32_t i = 2; ok && i < fastLatinHeader; i++)
    {
        ok = mlocFastLatinWord(words, i - 1) <= mlocFastLatinWord(words, i);
    }
    uint32_t base = fastLatinHeader + fastLatinChars;
    for (uint32_t i = 0; ok && i < fastLatinChars; i++)
    {
        uint32_t word = mlocFastLatinWord(words, fastLatinHeader + i);
        uint32_t at = base + (word & fastIndexMask);
        if (word >= fastExpansion && word < fastMinLong)
        {
            ok = at + 1 < count && mlocFastLatinMinisValid(words, at, 2);
        }
        else if (word >= fastContraction && word < fastExpansion)
        {
            ok = mlocFastLatinListValid(words, count, at);
        }
        else
        {
            ok = word != fastEndOfString;
        }
    }
    return ok;
}

// Reads a unit's table section at an offset: none when the unit ends
// there; a count, which a tailoring may give as fastLatinRootTable, and
// its words; where the section ends.
static inline bool mlocReadFastLatin(const uint8_t* p, uint64_t at, uint64_t size, bool tailoring,
                                     const uint8_t** wordsOut, uint32_t* countOut, uint64_t* endOut)
{
    *wordsOut = nullptr;
    *countOut = 0;
    *endOut = at;
    if (at == size)
    {
        return true;
    }
    if (at + 4 > size)
    {
        return false;
    }
    uint32_t count = mlocLoad32(p + at);
    if (count == fastLatinRootTable || count == 0)
    {
        *countOut = count;
        *endOut = at + 4;
        return tailoring || count == 0;
    }
    if (at + 4 + (uint64_t)count * 2 > size || !mlocFastLatinValid(p + at + 4, count))
    {
        return false;
    }
    *wordsOut = p + at + 4;
    *countOut = count;
    *endOut = at + 4 + (uint64_t)count * 2;
    return true;
}

#endif
