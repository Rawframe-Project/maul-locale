// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root collation unit (record mloc-0065), shared by the library,
// which reads it, and the generator, which writes it; the readers are
// inline. All numbers are little-endian.
//
//   0  u32 the UCA version: major << 16 | minor << 8 | update
//   4  u32 ce32Count, the expansion words below
//   8  u32 ceCount, the 64-bit elements below
//  12  u32 contextCount, the context words below
//  16  the code point trie of CE32s (record mloc-0063), padded to 8
//      u64 ces[ceCount]
//      u32 ce32s[ce32Count]
//      u32 contexts[contextCount]
//
// A CE32 is ICU's compressed element. Its low byte below 0xC0 makes it a
// simple element: the primary's two bytes in bits 31..16, the secondary
// byte in 15..8, the tertiary byte with its case bits in 7..0. From 0xC0
// its low four bits are a tag:
//   1  long primary: a three-byte primary in bits 31..8, the secondary
//      and tertiary common
//   2  long secondary: the primary 0, the secondary in bits 31..16, the
//      tertiary byte in 15..8
//   5  expansion of CE32s: index in bits 31..13, length 1 to 31 in 12..8
//   6  expansion of CEs: the same into ces
//   8  prefix: index in bits 31..13 into contexts
//   9  contraction: the same
//  14  offset: index in bits 31..13 of a ce whose bits 63..32 are a
//      three-byte primary and 31..8 its first code point, bit 7 set when
//      the primary compresses and 6..0 the step a code point adds (Han);
//      a code point below the first is read as unassigned
//  15  implicit, the whole word 0xFFFFFFFF: an unassigned code point's
//      primary is computed from the code point
//
// A context list (contexts[index]):
//   u32 default, the CE32 when no entry matches (never a context tag)
//   u32 count, then count entries in increasing order of their strings,
//   each u32 length (1 to 8), the code points, u32 CE32. For a prefix
//   the strings are the prefixes read backwards from the code point; for
//   a contraction, the code points after the first.

#ifndef MLOC_COLLATION_TABLES_H
#define MLOC_COLLATION_TABLES_H

#include "bytes.h"
#include "code_point_trie.h"
#include "data_view.h"

#include <stdint.h>

enum
{
    collationHead = 16,
    collationSpecialByte = 0xC0,
    collationTagLongPrimary = 1,
    collationTagLongSecondary = 2,
    collationTagExpansion32 = 5,
    collationTagExpansion = 6,
    collationTagPrefix = 8,
    collationTagContraction = 9,
    collationTagOffset = 14,
    collationMaxExpansion = 31,
    collationMaxContext = 8,
};

constexpr uint32_t collationImplicitCe32 = 0xFFFFFFFF;
constexpr uint64_t collationCommonSecondaryTertiary = 0x05000500;

typedef struct CollationRoot
{
    CodePointTrie trie;
    const uint8_t* ces;
    const uint8_t* ce32s;
    const uint8_t* contexts;
    uint32_t version;
    uint32_t ceCount;
    uint32_t ce32Count;
    uint32_t contextCount;
} CollationRoot;

static inline bool mlocCe32IsSpecial(uint32_t ce32)
{
    return (ce32 & 0xFF) >= collationSpecialByte;
}

static inline uint32_t mlocCe32Tag(uint32_t ce32)
{
    return ce32 & 0xF;
}

static inline uint32_t mlocCe32Index(uint32_t ce32)
{
    return ce32 >> 13;
}

static inline uint32_t mlocCe32Length(uint32_t ce32)
{
    return (ce32 >> 8) & 31;
}

// The element of a CE32 that is one: simple, long primary or long
// secondary; 0 for another.
static inline uint64_t mlocCe32Element(uint32_t ce32)
{
    if (!mlocCe32IsSpecial(ce32))
    {
        return (uint64_t)(ce32 & 0xFFFF0000) << 32 | (uint64_t)(ce32 & 0xFF00) << 16 |
               (uint64_t)(ce32 & 0xFF) << 8;
    }
    if (mlocCe32Tag(ce32) == collationTagLongPrimary)
    {
        return (uint64_t)(ce32 & 0xFFFFFF00) << 32 | collationCommonSecondaryTertiary;
    }
    if (mlocCe32Tag(ce32) == collationTagLongSecondary)
    {
        return (uint64_t)(ce32 & 0xFFFF0000) | (ce32 & 0xFF00);
    }
    return 0;
}

// ICU's incThreeBytePrimaryByOffset: a three-byte primary moved on by an
// offset, counting 254 third bytes from 02 and second bytes from 02, or
// 251 from 04 when the lead byte compresses.
static inline uint32_t mlocAddToPrimary(uint32_t primary, bool compressible, uint32_t offset)
{
    offset += ((primary >> 8) & 0xFF) - 2;
    uint32_t third = offset % 254 + 2;
    offset /= 254;
    uint32_t low = compressible ? 4 : 2;
    uint32_t count = compressible ? 251 : 254;
    offset += ((primary >> 16) & 0xFF) - low;
    uint32_t second = offset % count + low;
    offset /= count;
    return ((primary & 0xFF000000) + (offset << 24)) | second << 16 | third << 8;
}

// The element an offset gives a code point: its range's first primary
// moved on by the code point's distance times the step, the secondary
// and tertiary common; 0 for a code point before the range.
static inline uint64_t mlocCollationOffsetElement(uint64_t data, uint32_t codePoint)
{
    uint32_t first = (uint32_t)data >> 8;
    if (codePoint < first)
    {
        return 0;
    }
    uint32_t primary = mlocAddToPrimary((uint32_t)(data >> 32), (data & 0x80) != 0,
                                        (codePoint - first) * (uint32_t)(data & 0x7F));
    return (uint64_t)primary << 32 | collationCommonSecondaryTertiary;
}

static inline uint64_t mlocCollationCe(const CollationRoot* r, uint32_t index)
{
    return mlocLoad64(r->ces + (size_t)index * 8);
}

static inline uint32_t mlocCollationCe32(const CollationRoot* r, uint32_t index)
{
    return mlocLoad32(r->ce32s + (size_t)index * 4);
}

static inline uint32_t mlocCollationContext(const CollationRoot* r, uint32_t index)
{
    return mlocLoad32(r->contexts + (size_t)index * 4);
}

// Whether a CE32 is valid where it is found: one element anywhere; with
// mayRefer (a trie value, a context list's default or entry) also an
// expansion inside its array, an offset whose element exists, or the
// implicit word. Every special CE32's low byte is 0xC0 and its tag.
static inline bool mlocCollationCe32Valid(const CollationRoot* r, uint32_t ce32, bool mayRefer)
{
    if (!mlocCe32IsSpecial(ce32))
    {
        return true;
    }
    if (ce32 == collationImplicitCe32)
    {
        return mayRefer;
    }
    if ((ce32 & 0xF0) != collationSpecialByte)
    {
        return false;
    }
    uint32_t index = mlocCe32Index(ce32);
    uint32_t length = mlocCe32Length(ce32);
    switch (mlocCe32Tag(ce32))
    {
    case collationTagLongPrimary:
    case collationTagLongSecondary:
        return true;
    case collationTagExpansion32:
        return mayRefer && length > 0 && (uint64_t)index + length <= r->ce32Count;
    case collationTagExpansion:
        return mayRefer && length > 0 && (uint64_t)index + length <= r->ceCount;
    case collationTagOffset:
        return mayRefer && index < r->ceCount;
    default:
        return false;
    }
}

// A context list's words from index: true when its default, count and
// entries fit the contexts and every CE32 in it is valid.
static inline bool mlocCollationContextValid(const CollationRoot* r, uint32_t index)
{
    if ((uint64_t)index + 2 > r->contextCount ||
        !mlocCollationCe32Valid(r, mlocCollationContext(r, index), true))
    {
        return false;
    }
    uint32_t count = mlocCollationContext(r, index + 1);
    uint64_t at = (uint64_t)index + 2;
    for (uint32_t e = 0; e < count; e++)
    {
        if (at >= r->contextCount)
        {
            return false;
        }
        uint32_t length = mlocCollationContext(r, (uint32_t)at);
        if (length == 0 || length > collationMaxContext || at + 1 + length >= r->contextCount ||
            !mlocCollationCe32Valid(r, mlocCollationContext(r, (uint32_t)(at + 1 + length)), true))
        {
            return false;
        }
        at += 2 + length;
    }
    return true;
}

// A trie value: anything a context list or an expansion may hold, or a
// context whose list is valid.
static inline bool mlocCollationTrieValueValid(const CollationRoot* r, uint32_t ce32)
{
    if (mlocCe32IsSpecial(ce32) &&
        (mlocCe32Tag(ce32) == collationTagPrefix || mlocCe32Tag(ce32) == collationTagContraction))
    {
        return mlocCollationContextValid(r, mlocCe32Index(ce32));
    }
    return mlocCollationCe32Valid(r, ce32, true);
}

// Reads and checks a root unit: its trie, its arrays inside the payload,
// every trie value and every expansion word.
static inline bool mlocReadCollationRoot(DataSpan span, CollationRoot* out)
{
    if (span.size < collationHead)
    {
        return false;
    }
    const uint8_t* p = span.bytes;
    CollationRoot r = {
        .version = mlocLoad32(p),
        .ce32Count = mlocLoad32(p + 4),
        .ceCount = mlocLoad32(p + 8),
        .contextCount = mlocLoad32(p + 12),
    };
    if (!mlocReadCodePointTrie((DataSpan){p + collationHead, span.size - collationHead}, &r.trie))
    {
        return false;
    }
    uint64_t ces = collationHead + ((uint64_t)r.trie.size + 7) / 8 * 8;
    uint64_t ce32s = ces + (uint64_t)r.ceCount * 8;
    uint64_t contexts = ce32s + (uint64_t)r.ce32Count * 4;
    if (contexts + (uint64_t)r.contextCount * 4 > span.size)
    {
        return false;
    }
    r.ces = p + ces;
    r.ce32s = p + ce32s;
    r.contexts = p + contexts;
    bool ok = mlocCollationTrieValueValid(&r, r.trie.highValue);
    for (uint32_t i = 0; ok && i < r.trie.dataLength; i++)
    {
        ok = mlocCollationTrieValueValid(&r, mlocLoad32(r.trie.data + (size_t)i * 4));
    }
    for (uint32_t i = 0; ok && i < r.ce32Count; i++)
    {
        uint32_t ce32 = mlocCollationCe32(&r, i);
        ok = !mlocCe32IsSpecial(ce32) || mlocCollationCe32Valid(&r, ce32, false);
    }
    *out = r;
    return ok;
}

#endif
