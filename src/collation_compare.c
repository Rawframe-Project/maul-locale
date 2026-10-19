// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_compare.h"

#include "collation_iterator.h"

enum
{
    // The case level, read between the secondary and tertiary levels.
    levelCase = 6,
};

// ICU's NO_CE, read after a text's last element: primary 1, secondary
// and tertiary 0x0100, below every weight.
constexpr uint64_t noElement = 0x0000000101000100ULL;

CollationSettings mlocCollationDefaults(const CollationRoot* root)
{
    return (CollationSettings){
        .strength = collationTertiary,
        .variableTop = mlocCollationLastPrimary(root, collationGroupPunctuation),
    };
}

// One text's elements as a level reads them, the shifting applied.
typedef struct Level
{
    CollationIterator it;
    const CollationSettings* settings;
    uint32_t level;
    // After a variable element, until the next primary.
    bool afterVariable;
} Level;

static void Start(Level* l, const CollationRoot* root, const CollationSettings* settings,
                  uint32_t level, const uint8_t* text, size_t length)
{
    *l = (Level){.settings = settings, .level = level};
    mlocCollationIteratorStart(&l->it, root, text, length);
}

// The next element with the shifting applied: a variable element keeps
// its primary alone, and the primary ignorables after it read as 0;
// noElement at the end.
static uint64_t Element(Level* l, bool* variable)
{
    uint64_t element = 0;
    *variable = false;
    if (!mlocCollationNextElement(&l->it, &element))
    {
        return noElement;
    }
    if (!l->settings->shifted)
    {
        return element;
    }
    uint32_t primary = (uint32_t)(element >> 32);
    if (primary > collationMergeSeparator && primary <= l->settings->variableTop)
    {
        *variable = true;
        l->afterVariable = true;
        return element & 0xFFFFFFFF00000000ULL;
    }
    if (primary == 0 && l->afterVariable)
    {
        return 0;
    }
    if (primary != 0)
    {
        l->afterVariable = false;
    }
    return element;
}

static uint32_t End(uint32_t level)
{
    return level == collationSecondary || level == collationTertiary ? 0x0100 : 1;
}

static uint32_t Tertiary(const CollationSettings* s, uint32_t lower)
{
    if (s->caseFirst == collationCaseFirstOff || s->caseLevel)
    {
        return lower & 0x3F3F;
    }
    uint32_t t = lower & 0xFF3F;
    if (s->caseFirst == collationUpperFirst && t > 0x0100)
    {
        // ICU's upper first: a tertiary element's artificial upper case
        // stays above the others' case.
        t = lower > 0xFFFF ? t ^ 0xC000 : t + 0x4000;
    }
    return t;
}

// The case level's weight, kept non-zero, or 0 for an element it skips:
// at the primary strength the primary ignorables, otherwise the
// secondary ignorables.
static uint32_t Case(const CollationSettings* s, uint32_t primary, uint32_t lower)
{
    bool counted = s->strength == collationPrimary ? primary != 0 && lower != 0 : lower > 0xFFFF;
    if (!counted)
    {
        return 0;
    }
    uint32_t c = lower & 0xC000;
    return (s->caseFirst == collationUpperFirst ? 0xC000 - c : c) | 0x10000;
}

// The weight of an element at the level, 0 for none.
static uint32_t Weight(const Level* l, uint64_t element, bool variable)
{
    uint32_t primary = (uint32_t)(element >> 32);
    uint32_t lower = (uint32_t)element;
    switch (l->level)
    {
    case collationPrimary:
        return variable ? 0 : primary;
    case collationSecondary:
        return lower >> 16;
    case collationTertiary:
        return Tertiary(l->settings, lower);
    case levelCase:
        return Case(l->settings, primary, lower);
    default:
    {
        // ICU's quaternary: a variable element's primary, else the
        // weight in the tertiary's bits 7..6 above every primary.
        uint32_t q = lower & 0xFFFF;
        return q <= 0x0100 ? primary : q | 0xFFFFFF3F;
    }
    }
}

// The next non-zero weight of the level, or the end's.
static uint32_t Next(Level* l)
{
    for (;;)
    {
        bool variable = false;
        uint64_t element = Element(l, &variable);
        if (element == noElement)
        {
            return End(l->level);
        }
        uint32_t w = element == 0 ? 0 : Weight(l, element, variable);
        if (w != 0)
        {
            return w;
        }
    }
}

static int CompareLevel(const CollationRoot* root, const CollationSettings* settings,
                        uint32_t level, const uint8_t* a, size_t aLength, const uint8_t* b,
                        size_t bLength)
{
    Level left;
    Level right;
    Start(&left, root, settings, level, a, aLength);
    Start(&right, root, settings, level, b, bLength);
    uint32_t end = End(level);
    for (;;)
    {
        uint32_t l = Next(&left);
        uint32_t r = Next(&right);
        if (l != r)
        {
            return l < r ? -1 : 1;
        }
        if (l == end)
        {
            return 0;
        }
    }
}

// The next element of a segment with a non-zero secondary, its weight;
// 0 at the segment's end (the merge separator or the text's end), with
// *ended set when that is the text's.
static uint32_t SegmentSecondary(Level* l, bool* ended)
{
    for (;;)
    {
        bool variable = false;
        uint64_t element = Element(l, &variable);
        uint32_t primary = (uint32_t)(element >> 32);
        if (primary != 0 && primary <= collationMergeSeparator)
        {
            *ended = element == noElement;
            return 0;
        }
        uint32_t w = (uint32_t)element >> 16;
        if (w != 0)
        {
            return w;
        }
    }
}

static uint32_t CountSegment(Level* l, bool* ended)
{
    uint32_t n = 0;
    while (SegmentSecondary(l, ended) != 0)
    {
        n++;
    }
    return n;
}

// The secondary level backwards within each segment, as ICU compares it,
// with nothing buffered: each segment's secondaries are counted, then
// read forwards again with the longer one's surplus skipped, so that
// the pairs line up from the end, and the last pair that differs decides.
static int Backwards(const CollationRoot* root, const CollationSettings* settings, const uint8_t* a,
                     size_t aLength, const uint8_t* b, size_t bLength)
{
    Level left;
    Level right;
    Start(&left, root, settings, collationSecondary, a, aLength);
    Start(&right, root, settings, collationSecondary, b, bLength);
    for (;;)
    {
        Level l = left;
        Level r = right;
        bool leftEnded = false;
        bool rightEnded = false;
        uint32_t leftCount = CountSegment(&left, &leftEnded);
        uint32_t rightCount = CountSegment(&right, &rightEnded);
        bool ignored = false;
        for (uint32_t k = rightCount; k < leftCount; k++)
        {
            (void)SegmentSecondary(&l, &ignored);
        }
        for (uint32_t k = leftCount; k < rightCount; k++)
        {
            (void)SegmentSecondary(&r, &ignored);
        }
        int order = 0;
        uint32_t pairs = leftCount < rightCount ? leftCount : rightCount;
        for (uint32_t k = 0; k < pairs; k++)
        {
            uint32_t lw = SegmentSecondary(&l, &ignored);
            uint32_t rw = SegmentSecondary(&r, &ignored);
            order = lw != rw ? (lw < rw ? -1 : 1) : order;
        }
        if (order == 0 && leftCount != rightCount)
        {
            order = leftCount < rightCount ? -1 : 1;
        }
        // Equal primaries give both texts the same segments.
        if (order != 0 || leftEnded || rightEnded)
        {
            return order;
        }
    }
}

// A code point at the identical level, as ICU's compareNFDIter ranks it:
// the merge separator below every other, the end below that.
static int64_t Rank(NfdStream* s)
{
    uint32_t c = 0;
    if (!mlocNfdNext(s, &c))
    {
        return -2;
    }
    return c == 0xFFFE ? -1 : (int64_t)c;
}

// The identical level: the texts' NFD code points in order.
static int Identical(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    NfdStream left;
    NfdStream right;
    mlocNfdStart(&left, a, aLength);
    mlocNfdStart(&right, b, bLength);
    for (;;)
    {
        int64_t l = Rank(&left);
        int64_t r = Rank(&right);
        if (l != r)
        {
            return l < r ? -1 : 1;
        }
        if (l == -2)
        {
            return 0;
        }
    }
}

int mlocCollationCompare(const CollationRoot* root, const CollationSettings* settings,
                         const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    int order = CompareLevel(root, settings, collationPrimary, a, aLength, b, bLength);
    if (order == 0 && settings->strength >= collationSecondary)
    {
        order = settings->backwards
                    ? Backwards(root, settings, a, aLength, b, bLength)
                    : CompareLevel(root, settings, collationSecondary, a, aLength, b, bLength);
    }
    if (order == 0 && settings->caseLevel)
    {
        order = CompareLevel(root, settings, levelCase, a, aLength, b, bLength);
    }
    for (uint32_t level = collationTertiary;
         order == 0 && level <= settings->strength && level <= collationQuaternary; level++)
    {
        order = CompareLevel(root, settings, level, a, aLength, b, bLength);
    }
    if (order == 0 && settings->strength == collationIdentical)
    {
        order = Identical(a, aLength, b, bLength);
    }
    return order;
}
