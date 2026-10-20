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
//  16  u32 listCount, the context lists
//  20  u32 scriptCount, the scripts and groups below
//  24  u32 startCount, the script starts below
//  28  u32 numericPrimary, the lead byte of numeric order's primaries
//  32  the code point trie of CE32s (record mloc-0063), padded to 8
//      u64 ces[ceCount]
//      u32 ce32s[ce32Count]
//      u32 listStarts[listCount + 1]: where each context list starts in
//      the context words, rising, the last contextCount
//      u32 contexts[contextCount]
//      u32 scripts[scriptCount][2]: a script (Maul Unicode's packed
//      code) or group (collationGroupSpace and on), rising, and the
//      index of its start
//      u32 starts[startCount]: ICU's script starts, the top 16 bits of
//      the first primary of each range in order, rising from 0 and ending
//      with the trailing range's 0xFF00; a range ends where the next
//      starts
//      u8 compressible[32]: a bit a lead byte (bit b % 8 of byte b / 8),
//      set for the bytes sort keys compress: FractionalUCA's COMPRESS
//      top bytes and the unassigned byte FE, as ICU's compressibleBytes
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
//   8  prefix: a context list's number in bits 31..13
//   9  contraction: the same
//  14  offset: index in bits 31..13 of a ce whose bits 63..32 are a
//      three-byte primary and 31..8 its first code point, bit 7 set when
//      the primary compresses and 6..0 the step a code point adds (Han);
//      a code point below the first is read as unassigned
//  15  implicit, the whole word 0xFFFFFFFF: an unassigned code point's
//      primary is computed from the code point
//
// A context list (contexts[listStarts[number]] up to the next start):
//   u32 default, the CE32 when no entry matches (never a context tag)
//   u32 count, then count entries in increasing order of their strings,
//   each u32 length (1 to 8), the code points, u32 CE32; the entries
//   fill the list exactly. For a prefix the strings are the prefixes read
//   backwards from the code point; for a contraction, the code points
//   after the first. The lists are checked once each at open and a trie
//   value only names one, so opening costs the unit's size, however many
//   code points name a list.

// A tailoring (record mloc-0071) is a unit over the root in the same
// encoding:
//
//   0  u32 ce32Count
//   4  u32 ceCount
//   8  u32 contextCount
//  12  u32 listCount
//  16  u32 settings: the rules' defaults, each 0 when the rules give
//      none, else as mlocCollatorOptions numbers them: bits 2..0 the
//      strength, 4..3 alternate, 7..5 max variable, 9..8 case first,
//      11..10 case level, 13..12 numeric, 15..14 backwards; bit 16 set
//      when the rules reorder (an empty [reorder] too); bit 17 set when
//      Han the tailoring leaves to the root takes the radical-stroke
//      table's primaries (the unihan types); the rest 0
//  20  u32 reorderCount, the reorder codes below, at most 256
//  24  u32 mappedCount, the code points the trie maps: 0 when the type
//      gives the root's elements, with its settings alone
//  28  u32 0
//  32  the code point trie of CE32s, padded to 8, then ces, ce32s,
//      listStarts and contexts as the root's, then u32
//      reorder[reorderCount]: the [reorder] codes, each once: Maul
//      Unicode's packed scripts (four letters, the first upper case) and
//      the groups
//
// A trie value may be the fallback word 0xC0 (tag 0), "the root's". A
// context list's default and entries may also be a contraction naming a
// list before their own: a prefix's contractions, and a prefix with
// contractions only falling back to a shorter prefix's.
//
// A locale's types are one unit of its key (kindCollationTailorings):
//
//   0  u32 typeCount
//   4  u32 defaultLength, the length of the name of the locale's default
//      type, which the names begin with; 0 when it names none
//   8  u32 0
//  12  u32 0
//  16  types[typeCount], 16 bytes each: u32 the name's offset in the
//      names, u32 its length, u32 the tailoring's offset from the unit's
//      start, u32 its size; by name, rising
//      the names: the default's, then each type's in order, BCP 47's
//      co values (lowercase letters, digits and hyphens), padded to 8
//      the tailorings, each from a multiple of 8, in order, padded to 8
//
// A unit has a type or a default, or both. Types the locale does not
// list are its chain's.
//
// The radical-stroke Han table (kindCollationHanRadical), a unit of no
// locale, gives Han the primaries ICU4C's unihan root gives it:
//
//   0  u32 rangeCount
//   4  u32 codePointCount, the code points the ranges hold
//   8  u32 0
//  12  u32 0
//  16  ranges[rangeCount], 8 bytes each, by start, rising and disjoint:
//      u32 the start in bits 20..0 and the length less one in 31..21;
//      u32 the start's ordinal in radical-stroke order, the range within
//      codePointCount
//
// A code point of ordinal n has the primary 2n after the first Han
// primary, three-byte steps that do not compress; the first is FA 02 02
// (ICU's initHanRanges: the lead byte FE less the lead bytes that
// 2 codePointCount + 4 primaries take).

#ifndef MLOC_COLLATION_TABLES_H
#define MLOC_COLLATION_TABLES_H

#include "bytes.h"
#include "code_point_trie.h"
#include "data_view.h"

#include <stdint.h>
#include <string.h>

enum
{
    collationHead = 32,
    collationTypesHead = 16,
    collationTypeSize = 16,
    collationMaxTypeName = 32,
    // Codes in a reorder list, more than the root has ranges.
    collationMaxReorderCodes = 256,
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
    collationUnassignedByte = 0xFE,
    // The groups among the scripts, as ICU's UCOL_REORDER_CODE_SPACE and
    // on, and its two reserved ranges either side of Latin.
    collationGroupSpace = 0x1000,
    collationGroupPunctuation = 0x1001,
    collationGroupSymbol = 0x1002,
    collationGroupCurrency = 0x1003,
    collationGroupDigit = 0x1004,
    collationReservedBeforeLatin = 0x100E,
    collationReservedAfterLatin = 0x100F,
    collationMergeSeparator = 0x02000000,
};

constexpr uint32_t collationImplicitCe32 = 0xFFFFFFFF;
// A tailoring's trie value for a code point the root gives.
constexpr uint32_t collationFallbackCe32 = 0xC0;
// The settings word's fields.
constexpr uint32_t collationReorderGiven = 0x10000;
constexpr uint32_t collationHanRadical = 0x20000;
constexpr uint64_t collationCommonSecondaryTertiary = 0x05000500;
// U+FFFD and U+FFFF: fixed primaries FFFD and FFFF, as simple CE32s.
constexpr uint32_t collationFffdCe32 = 0xFFFD0505;
constexpr uint32_t collationFfffCe32 = 0xFFFF0505;

typedef struct CollationRoot
{
    CodePointTrie trie;
    const uint8_t* ces;
    const uint8_t* ce32s;
    const uint8_t* listStarts;
    const uint8_t* contexts;
    const uint8_t* scripts;
    const uint8_t* starts;
    uint32_t version;
    uint32_t ceCount;
    uint32_t ce32Count;
    uint32_t contextCount;
    uint32_t listCount;
    uint32_t scriptCount;
    uint32_t startCount;
    uint32_t numericPrimary;
    const uint8_t* compressible;
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

// ICU's unassignedPrimaryFromCodePoint: the four-byte primary, lead byte
// FE, of a code point the trie gives the implicit word. A gap before
// U+0000; 18 fourth bytes 14 apart, 254 third bytes, 251 second bytes
// from 04.
static inline uint32_t mlocCollationUnassignedPrimary(uint32_t codePoint)
{
    uint32_t c = codePoint + 1;
    uint32_t primary = 2 + (c % 18) * 14;
    c /= 18;
    primary |= (2 + c % 254) << 8;
    c /= 254;
    primary |= (4 + c % 251) << 16;
    return primary | (uint32_t)collationUnassignedByte << 24;
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

// Where a context list starts in the context words; number at most
// listCount, which gives the end of the last.
static inline uint32_t mlocCollationListStart(const CollationRoot* r, uint32_t number)
{
    return mlocLoad32(r->listStarts + (size_t)number * 4);
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

// A context list's CE32: valid with mayRefer, or a contraction naming a
// list before the given one (never, for the root: below is 0).
static inline bool mlocCollationListCe32Valid(const CollationRoot* r, uint32_t ce32, uint32_t below)
{
    return mlocCollationCe32Valid(r, ce32, true) ||
           ((ce32 & 0xFF) == (collationSpecialByte | collationTagContraction) &&
            mlocCe32Index(ce32) < below);
}

// The context list from start to end: a valid default, then entries
// that fill it exactly, each with a length from 1 to 8 and a valid CE32,
// contractions naming lists below the given number allowed.
static inline bool mlocCollationListValid(const CollationRoot* r, uint32_t start, uint32_t end,
                                          uint32_t below)
{
    if (end - start < 2 || !mlocCollationListCe32Valid(r, mlocCollationContext(r, start), below))
    {
        return false;
    }
    uint32_t count = mlocCollationContext(r, start + 1);
    uint32_t at = start + 2;
    for (uint32_t e = 0; e < count; e++)
    {
        if (end - at < 3)
        {
            return false;
        }
        uint32_t length = mlocCollationContext(r, at);
        if (length == 0 || length > collationMaxContext || end - at < 2 + length ||
            !mlocCollationListCe32Valid(r, mlocCollationContext(r, at + 1 + length), below))
        {
            return false;
        }
        at += 2 + length;
    }
    return at == end;
}

// A trie value: anything a context list or an expansion may hold, or a
// context naming a list that exists.
static inline bool mlocCollationTrieValueValid(const CollationRoot* r, uint32_t ce32)
{
    if (mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32 &&
        (ce32 & 0xF0) == collationSpecialByte &&
        (mlocCe32Tag(ce32) == collationTagPrefix || mlocCe32Tag(ce32) == collationTagContraction))
    {
        return mlocCe32Index(ce32) < r->listCount;
    }
    return mlocCollationCe32Valid(r, ce32, true);
}

// A script's or group's code at an index of the scripts, and its start's
// index.
static inline uint32_t mlocCollationScriptCode(const CollationRoot* r, uint32_t index)
{
    return mlocLoad32(r->scripts + (size_t)index * 8);
}

static inline uint32_t mlocCollationScriptStart(const CollationRoot* r, uint32_t index)
{
    return mlocLoad32(r->scripts + (size_t)index * 8 + 4);
}

static inline uint32_t mlocCollationStart(const CollationRoot* r, uint32_t index)
{
    return mlocLoad32(r->starts + (size_t)index * 4);
}

// Whether sort keys compress primaries of the lead byte.
static inline bool mlocCollationCompressible(const CollationRoot* r, uint32_t leadByte)
{
    return (r->compressible[leadByte / 8] >> (leadByte % 8) & 1) != 0;
}

// The scripts rising by code, each naming a start that has one after it;
// the starts rising from 0, 16-bit, the last 0xFF00 (the trailing range),
// at most 256 (reordering's tables are indexed by start).
static inline bool mlocCollationScriptsValid(const CollationRoot* r)
{
    bool ok = r->startCount >= 2 && r->startCount <= 256 && mlocCollationStart(r, 0) == 0 &&
              mlocCollationStart(r, r->startCount - 1) == 0xFF00;
    for (uint32_t i = 1; ok && i < r->startCount; i++)
    {
        ok = mlocCollationStart(r, i - 1) < mlocCollationStart(r, i);
    }
    for (uint32_t i = 0; ok && i < r->scriptCount; i++)
    {
        uint32_t start = mlocCollationScriptStart(r, i);
        ok = start > 0 && start + 1 < r->startCount &&
             (i == 0 || mlocCollationScriptCode(r, i - 1) < mlocCollationScriptCode(r, i));
    }
    return ok;
}

// The index of a script's or group's start, or 0 when the root has none
// (ICU's getScriptIndex).
static inline uint32_t mlocCollationScriptIndex(const CollationRoot* r, uint32_t code)
{
    uint32_t low = 0;
    uint32_t high = r->scriptCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint32_t found = mlocCollationScriptCode(r, middle);
        if (found == code)
        {
            return mlocCollationScriptStart(r, middle);
        }
        if (found < code)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return 0;
}

// The last primary of a group or script, ICU's getLastPrimaryForGroup: the
// variable top for maxVariable's group; 0 when the root has none.
static inline uint32_t mlocCollationLastPrimary(const CollationRoot* r, uint32_t code)
{
    uint32_t index = mlocCollationScriptIndex(r, code);
    return index == 0 ? 0 : (mlocCollationStart(r, index + 1) << 16) - 1;
}

// Reads and checks a root unit: its trie, its arrays inside the payload,
// every context list once, every trie value and every expansion word.
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
        .listCount = mlocLoad32(p + 16),
        .scriptCount = mlocLoad32(p + 20),
        .startCount = mlocLoad32(p + 24),
        .numericPrimary = mlocLoad32(p + 28),
    };
    if (!mlocReadCodePointTrie((DataSpan){p + collationHead, span.size - collationHead}, &r.trie))
    {
        return false;
    }
    uint64_t ces = collationHead + ((uint64_t)r.trie.size + 7) / 8 * 8;
    uint64_t ce32s = ces + (uint64_t)r.ceCount * 8;
    uint64_t starts = ce32s + (uint64_t)r.ce32Count * 4;
    uint64_t contexts = starts + ((uint64_t)r.listCount + 1) * 4;
    uint64_t scripts = contexts + (uint64_t)r.contextCount * 4;
    uint64_t scriptStarts = scripts + (uint64_t)r.scriptCount * 8;
    uint64_t compressible = scriptStarts + (uint64_t)r.startCount * 4;
    if (compressible + 32 > span.size)
    {
        return false;
    }
    r.compressible = p + compressible;
    r.ces = p + ces;
    r.ce32s = p + ce32s;
    r.listStarts = p + starts;
    r.contexts = p + contexts;
    r.scripts = p + scripts;
    r.starts = p + scriptStarts;
    bool ok = mlocCollationScriptsValid(&r) && mlocCollationListStart(&r, 0) == 0 &&
              mlocCollationListStart(&r, r.listCount) == r.contextCount;
    for (uint32_t i = 0; ok && i < r.listCount; i++)
    {
        uint32_t start = mlocCollationListStart(&r, i);
        uint32_t end = mlocCollationListStart(&r, i + 1);
        ok = start < end && end <= r.contextCount && mlocCollationListValid(&r, start, end, 0);
    }
    ok = ok && mlocCollationTrieValueValid(&r, r.trie.highValue);
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

// The root unit's validator, as data files are opened.
bool mlocValidCollationRoot(const DataView* view, DataSpan payload);

// A tailoring: its elements in the root's form (no scripts), its
// settings word and its reorder codes.
typedef struct CollationTailoring
{
    CollationRoot view;
    uint32_t settings;
    uint32_t reorderCount;
    const uint8_t* reorder;
    uint32_t mappedCount;
} CollationTailoring;

// Whether a settings word holds only the values its fields may.
static inline bool mlocCollationSettingsValid(uint32_t s)
{
    return (s & 7) <= 5 && (s >> 3 & 3) <= 2 && (s >> 5 & 7) <= 4 && (s >> 10 & 3) <= 2 &&
           (s >> 12 & 3) <= 2 && (s >> 14 & 3) <= 2 && (s & ~0x3FFFFu) == 0;
}

// Whether reorder codes are scripts' or groups', each once.
static inline bool mlocCollationReorderValid(const uint8_t* codes, uint32_t count)
{
    bool ok = count <= collationMaxReorderCodes;
    for (uint32_t i = 0; ok && i < count; i++)
    {
        uint32_t code = mlocLoad32(codes + (size_t)i * 4);
        bool letters = (code >> 24) - 'A' < 26;
        for (int k = 0; letters && k < 3; k++)
        {
            letters = (code >> (8 * k) & 0xFF) - 'a' < 26;
        }
        ok = letters || code - collationGroupSpace <= collationGroupDigit - collationGroupSpace;
        for (uint32_t j = 0; ok && j < i; j++)
        {
            ok = mlocLoad32(codes + (size_t)j * 4) != code;
        }
    }
    return ok;
}

// Reads and checks a tailoring as mlocReadCollationRoot reads the root,
// the fallback word allowed in the trie.
static inline bool mlocReadCollationTailoring(DataSpan span, CollationTailoring* out)
{
    if (span.size < collationHead)
    {
        return false;
    }
    const uint8_t* p = span.bytes;
    CollationTailoring t = {
        .view = {.ce32Count = mlocLoad32(p),
                 .ceCount = mlocLoad32(p + 4),
                 .contextCount = mlocLoad32(p + 8),
                 .listCount = mlocLoad32(p + 12)},
        .settings = mlocLoad32(p + 16),
        .reorderCount = mlocLoad32(p + 20),
        .mappedCount = mlocLoad32(p + 24),
    };
    CollationRoot* r = &t.view;
    if (!mlocCollationSettingsValid(t.settings) || t.mappedCount > 0x110000 ||
        mlocLoad32(p + 28) != 0 ||
        !mlocReadCodePointTrie((DataSpan){p + collationHead, span.size - collationHead}, &r->trie))
    {
        return false;
    }
    uint64_t ces = collationHead + ((uint64_t)r->trie.size + 7) / 8 * 8;
    uint64_t ce32s = ces + (uint64_t)r->ceCount * 8;
    uint64_t starts = ce32s + (uint64_t)r->ce32Count * 4;
    uint64_t contexts = starts + ((uint64_t)r->listCount + 1) * 4;
    uint64_t reorder = contexts + (uint64_t)r->contextCount * 4;
    if (reorder + (uint64_t)t.reorderCount * 4 > span.size)
    {
        return false;
    }
    r->ces = p + ces;
    r->ce32s = p + ce32s;
    r->listStarts = p + starts;
    r->contexts = p + contexts;
    t.reorder = p + reorder;
    bool ok = mlocCollationReorderValid(t.reorder, t.reorderCount) &&
              mlocCollationListStart(r, 0) == 0 &&
              mlocCollationListStart(r, r->listCount) == r->contextCount;
    for (uint32_t i = 0; ok && i < r->listCount; i++)
    {
        uint32_t start = mlocCollationListStart(r, i);
        uint32_t end = mlocCollationListStart(r, i + 1);
        ok = start < end && end <= r->contextCount && mlocCollationListValid(r, start, end, i);
    }
    ok = ok && (r->trie.highValue == collationFallbackCe32 ||
                mlocCollationTrieValueValid(r, r->trie.highValue));
    for (uint32_t i = 0; ok && i < r->trie.dataLength; i++)
    {
        uint32_t ce32 = mlocLoad32(r->trie.data + (size_t)i * 4);
        ok = ce32 == collationFallbackCe32 || mlocCollationTrieValueValid(r, ce32);
    }
    for (uint32_t i = 0; ok && i < r->ce32Count; i++)
    {
        uint32_t ce32 = mlocCollationCe32(r, i);
        ok = !mlocCe32IsSpecial(ce32) || mlocCollationCe32Valid(r, ce32, false);
    }
    *out = t;
    return ok;
}

// A locale's types, read from a unit the validator passed.
typedef struct CollationTypes
{
    const uint8_t* bytes;
    uint32_t count;
    // The default type's name, defaultLength 0 for none.
    const uint8_t* defaultName;
    uint32_t defaultLength;
    const uint8_t* names;
} CollationTypes;

static inline CollationTypes mlocReadCollationTypes(DataSpan unit)
{
    uint32_t count = mlocLoad32(unit.bytes);
    const uint8_t* names = unit.bytes + collationTypesHead + (size_t)count * collationTypeSize;
    return (CollationTypes){unit.bytes, count, names, mlocLoad32(unit.bytes + 4), names};
}

// A type's tailoring by its name, or false when the locale lists none.
static inline bool mlocFindCollationType(const CollationTypes* t, const char* name, size_t length,
                                         DataSpan* tailoringOut)
{
    for (uint32_t i = 0; i < t->count; i++)
    {
        const uint8_t* entry = t->bytes + collationTypesHead + (size_t)i * collationTypeSize;
        if (mlocLoad32(entry + 4) == length &&
            memcmp(t->names + mlocLoad32(entry), name, length) == 0)
        {
            *tailoringOut = (DataSpan){t->bytes + mlocLoad32(entry + 8), mlocLoad32(entry + 12)};
            return true;
        }
    }
    return false;
}

bool mlocValidCollationTypes(const DataView* view, DataSpan payload);

// The radical-stroke Han table, read from a unit the validator passed.
typedef struct CollationHan
{
    const uint8_t* ranges;
    uint32_t count;
    uint32_t firstPrimary;
} CollationHan;

// ICU's first Han primary for a count of Han code points.
static inline uint32_t mlocCollationFirstHanPrimary(uint32_t codePoints)
{
    uint32_t primaries = codePoints * 2 + 4;
    uint32_t leadBytes = (primaries + 254 * 254 - 1) / (254 * 254);
    return (collationUnassignedByte - leadBytes) << 24 | 0x20200;
}

static inline CollationHan mlocReadCollationHan(DataSpan unit)
{
    return (CollationHan){unit.bytes + 16, mlocLoad32(unit.bytes),
                          mlocCollationFirstHanPrimary(mlocLoad32(unit.bytes + 4))};
}

// A code point's radical-stroke primary, or false when the table has
// none for it.
static inline bool mlocCollationHanPrimary(const CollationHan* han, uint32_t codePoint,
                                           uint32_t* primaryOut)
{
    uint32_t lo = 0;
    uint32_t hi = han->count;
    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo) / 2;
        uint32_t word = mlocLoad32(han->ranges + (size_t)mid * 8);
        uint32_t start = word & 0x1FFFFF;
        if (codePoint < start)
        {
            hi = mid;
        }
        else if (codePoint - start > word >> 21)
        {
            lo = mid + 1;
        }
        else
        {
            uint32_t ordinal = mlocLoad32(han->ranges + (size_t)mid * 8 + 4) + codePoint - start;
            *primaryOut = mlocAddToPrimary(han->firstPrimary, false, ordinal * 2);
            return true;
        }
    }
    return false;
}

bool mlocValidCollationHan(const DataView* view, DataSpan payload);

#endif
