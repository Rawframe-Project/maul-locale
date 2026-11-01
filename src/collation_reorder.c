// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_reorder.h"

#include "invariant.h"

#include "maul-locale/collate.h"
#include "maul-unicode/properties.h"

#include <assert.h>
#include <string.h>

static_assert(collationMaxReorderRanges == MLOC_MAX_REORDER_RANGES,
              "the reordering keeps the ranges the public bound names");

enum
{
    groupCount = collationGroupDigit - collationGroupSpace + 1,
};

// Zzzz: "others", and the unassigned range's script.
static const uint32_t scriptUnknown = MUNI_MAKE_SCRIPT('Z', 'z', 'z', 'z');
static const uint32_t scriptLatin = MUNI_MAKE_SCRIPT('L', 'a', 't', 'n');

// A code's start index, as ICU's getScriptIndex: the reserved ranges
// either side of Latin are never named.
static uint32_t IndexOf(const CollationRoot* root, uint32_t code)
{
    bool reserved = code == collationReservedBeforeLatin || code == collationReservedAfterLatin;
    return reserved ? 0 : mlocCollationScriptIndex(root, code);
}

// The builder's state: the table by start index, and the next low and
// high lead bytes (with the low byte of the start or limit they are at).
typedef struct Builder
{
    const CollationRoot* root;
    uint8_t table[256];
    uint32_t lowStart;
    uint32_t highLimit;
    // The end ranges met the front ones.
    bool overflow;
} Builder;

// Puts the range at the index after the ranges at the front, as ICU's
// addLowScriptRange: a range starting at a lower second byte than the
// last one ended at takes the next lead byte.
static void AddLow(Builder* b, uint32_t index)
{
    // A script's start has a start after it (mlocCollationScriptsValid).
    MLOC_ASSERT(index > 0 && index + 1 < b->root->startCount);
    uint32_t start = mlocCollationStart(b->root, index);
    if ((start & 0xFF) < (b->lowStart & 0xFF))
    {
        b->lowStart += 0x100;
    }
    b->table[index] = (uint8_t)(b->lowStart >> 8);
    uint32_t limit = mlocCollationStart(b->root, index + 1);
    // ICU masks with 0xFF00 here, so a front that passes the last lead
    // byte wraps to the first and the check after misses it; the bits
    // above are kept, so the check sees the overflow.
    b->lowStart = ((b->lowStart & ~0xFFU) + ((limit & 0xFF00) - (start & 0xFF00))) | (limit & 0xFF);
}

// Puts the range before the ranges at the end (ICU's addHighScriptRange).
// An end that would pass below the first start is an overflow, where
// ICU's int arithmetic goes negative.
static void AddHigh(Builder* b, uint32_t index)
{
    MLOC_ASSERT(index > 0 && index + 1 < b->root->startCount);
    uint32_t limit = mlocCollationStart(b->root, index + 1);
    uint32_t start = mlocCollationStart(b->root, index);
    uint32_t high = b->highLimit - ((limit & 0xFF) > (b->highLimit & 0xFF) ? 0x100 : 0);
    uint32_t span = (limit & 0xFF00) - (start & 0xFF00);
    b->table[index] = 1;
    if (b->overflow || (high & 0xFF00) < span + mlocCollationStart(b->root, 1))
    {
        b->overflow = true;
        return;
    }
    b->highLimit = ((high & 0xFF00) - span) | (start & 0xFF);
    b->table[index] = (uint8_t)(b->highLimit >> 8);
}

// A code's range at the end or the front; false for a duplicate or a
// code sharing a range with one before.
static bool Place(Builder* b, uint32_t code, bool high)
{
    uint32_t index = IndexOf(b->root, code);
    if (index == 0)
    {
        return true;
    }
    if (b->table[index] != 0)
    {
        return false;
    }
    if (high)
    {
        AddHigh(b, index);
    }
    else
    {
        AddLow(b, index);
    }
    return true;
}

// The groups not named, at the front in their order.
static void PlaceGroups(Builder* b, const uint32_t* codes, size_t count)
{
    uint32_t named = 0;
    for (size_t i = 0; i < count; i++)
    {
        uint32_t group = codes[i] - collationGroupSpace;
        named |= group < groupCount ? 1U << group : 0;
    }
    for (uint32_t g = 0; g < groupCount; g++)
    {
        uint32_t index = mlocCollationScriptIndex(b->root, collationGroupSpace + g);
        if (index != 0 && (named & 1U << g) == 0)
        {
            AddLow(b, index);
        }
    }
}

// The named codes in order; those after Zzzz go to the end, the last
// one last. False on a refusal; *toEnd when Zzzz was named.
static bool PlaceNamed(Builder* b, const uint32_t* codes, size_t count, bool* toEnd)
{
    *toEnd = false;
    size_t length = count;
    for (size_t i = 0; i < length;)
    {
        uint32_t code = codes[i++];
        if (code == scriptUnknown)
        {
            *toEnd = true;
            while (i < length)
            {
                code = codes[--length];
                if (code == scriptUnknown || !Place(b, code, true))
                {
                    return false;
                }
            }
            return true;
        }
        if (!Place(b, code, false))
        {
            return false;
        }
    }
    return true;
}

// The ranges from the table, as ICU's makeReorderRanges ends: a pair
// for each run of start indexes moving by one offset, the reserved
// ranges (0xFF) taking their neighbours' offset.
static uint32_t Ranges(const Builder* b, uint32_t* ranges)
{
    const CollationRoot* r = b->root;
    uint32_t last = r->startCount - 1;
    uint32_t n = 0;
    int32_t offset = 0;
    for (uint32_t i = 1;; i++)
    {
        int32_t nextOffset = offset;
        for (; i < last; i++)
        {
            uint32_t lead = b->table[i];
            if (lead != 0xFF)
            {
                nextOffset = (int32_t)lead - (int32_t)(mlocCollationStart(r, i) >> 8);
                if (nextOffset != offset)
                {
                    break;
                }
            }
        }
        if (offset != 0 || i < last)
        {
            // A range for each start at most: fewer than the caller's 256.
            MLOC_ASSERT(n < i);
            ranges[n++] = mlocCollationStart(r, i) << 16 | ((uint32_t)offset & 0xFFFF);
        }
        if (i == last)
        {
            return n;
        }
        offset = nextOffset;
    }
}

// The lead byte table and the shared bytes' ranges from the ranges, as
// ICU's setReordering; false when the shared bytes need more ranges than
// the reordering holds.
static bool Arrays(const uint32_t* ranges, uint32_t n, CollationReorder* out)
{
    // The caller returns before making no ranges.
    MLOC_ASSERT(n > 0);
    out->minHighNoReorder = ranges[n - 1] & 0xFFFF0000;
    uint32_t b = 0;
    uint32_t firstSplit = n;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t limit = ranges[i] >> 24;
        for (; b < limit; b++)
        {
            out->table[b] = (uint8_t)(b + ranges[i]);
        }
        if ((ranges[i] & 0xFF0000) != 0)
        {
            out->table[limit] = 0;
            b = limit + 1;
            firstSplit = firstSplit == n ? i : firstSplit;
        }
    }
    for (; b <= 0xFF; b++)
    {
        out->table[b] = (uint8_t)b;
    }
    out->rangeCount = n - (firstSplit == n ? n : firstSplit);
    if (out->rangeCount > collationMaxReorderRanges)
    {
        return false;
    }
    memcpy(out->ranges, ranges + (firstSplit == n ? 0 : firstSplit),
           out->rangeCount * sizeof(uint32_t));
    return true;
}

static mlocResult Make(const CollationRoot* root, const uint32_t* codes, size_t count,
                       bool latinMustMove, uint32_t* ranges, uint32_t* n)
{
    // The caller returns before an empty order.
    MLOC_ASSERT(count > 0);
    Builder b = {.root = root,
                 .lowStart = mlocCollationStart(root, 1),
                 .highLimit = mlocCollationStart(root, root->startCount - 1)};
    memset(b.table, 0, sizeof(b.table));
    uint32_t reserved[2] = {mlocCollationScriptIndex(root, collationReservedBeforeLatin),
                            mlocCollationScriptIndex(root, collationReservedAfterLatin)};
    for (int k = 0; k < 2; k++)
    {
        b.table[reserved[k]] = reserved[k] != 0 ? 0xFF : b.table[reserved[k]];
    }
    PlaceGroups(&b, codes, count);
    // Latin first and no group named: the space before Latin is
    // skipped, so Latin keeps its bytes, unless the order does not fit.
    bool groupsNamed = false;
    for (size_t i = 0; i < count; i++)
    {
        groupsNamed = groupsNamed || codes[i] - collationGroupSpace < groupCount;
    }
    uint32_t skippedReserved = 0;
    uint32_t latin = mlocCollationScriptIndex(root, scriptLatin);
    if (!groupsNamed && codes[0] == scriptLatin && !latinMustMove && latin != 0)
    {
        uint32_t start = mlocCollationStart(root, latin);
        skippedReserved = start - b.lowStart;
        b.lowStart = start;
    }
    bool toEnd = false;
    if (!PlaceNamed(&b, codes, count, &toEnd))
    {
        return mloc_errorInvalid;
    }
    for (uint32_t i = 1; i + 1 < root->startCount; i++)
    {
        if (b.table[i] != 0)
        {
            continue;
        }
        uint32_t start = mlocCollationStart(root, i);
        b.lowStart = !toEnd && start > b.lowStart ? start : b.lowStart;
        AddLow(&b, i);
    }
    if (b.overflow || b.lowStart > b.highLimit)
    {
        if (!latinMustMove && b.lowStart - (skippedReserved & 0xFF00) <= b.highLimit)
        {
            return Make(root, codes, count, true, ranges, n);
        }
        return mloc_errorLimit;
    }
    *n = Ranges(&b, ranges);
    return mloc_success;
}

mlocResult mlocCollationMakeReorder(const CollationRoot* root, const uint32_t* codes, size_t count,
                                    CollationReorder* out, bool* changes)
{
    *changes = false;
    if (count == 0 || (count == 1 && codes[0] == scriptUnknown))
    {
        return mloc_success;
    }
    uint32_t ranges[256];
    uint32_t n = 0;
    mlocResult result = Make(root, codes, count, false, ranges, &n);
    if (result != mloc_success || n == 0)
    {
        return result;
    }
    CollationReorder made;
    if (!Arrays(ranges, n, &made))
    {
        return mloc_errorLimit;
    }
    *out = made;
    *changes = true;
    return mloc_success;
}
