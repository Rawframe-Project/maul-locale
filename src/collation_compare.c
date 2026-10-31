// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_compare.h"

#include "collation_iterator.h"
#include "invariant.h"

#include "maul-unicode/properties.h"

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

// The code points before a compare's start, which both texts share (the
// identical prefix), oldest first: the last collationMaxContext of its
// NFD, which the iterators' prefix contexts read back.
typedef struct History
{
    uint32_t codePoints[collationMaxContext];
    uint32_t count;
} History;

static History Before(NfdText text, size_t start)
{
    History h = {.count = 0};
    uint32_t newest[collationMaxContext];
    uint32_t n = 0;
    for (size_t at = start; at > 0 && n < collationMaxContext;)
    {
        at = mlocNfdBefore(text, at);
        size_t size = 0;
        uint32_t d[nfdMaxDecomposition];
        uint8_t lead = 0;
        uint8_t trail = 0;
        uint32_t k = mlocNfdDecompose(mlocNfdDecodeAt(text, at, &size), d, &lead, &trail);
        for (uint32_t i = k; i-- > 0 && n < collationMaxContext;)
        {
            newest[n++] = d[i];
        }
    }
    while (n > 0)
    {
        h.codePoints[h.count++] = newest[--n];
    }
    return h;
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
                  uint32_t level, NfdText text, const History* h)
{
    // Field by field: the iterator's start sets each field it reads before
    // writing, and clearing the whole level (its 2 KB iterator) was most
    // of a short text's time.
    l->settings = settings;
    l->level = level;
    l->afterVariable = false;
    mlocCollationIteratorStartText(&l->it, root, text);
    l->it.tailoring = settings->tailoring;
    l->it.han = settings->han;
    l->it.numeric = settings->numeric;
    for (uint32_t i = 0; i < h->count; i++)
    {
        mlocCollationIteratorRemember(&l->it, h->codePoints[i]);
    }
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

// A primary with the script reordering, if any.
static uint32_t Reordered(const CollationSettings* s, uint32_t primary)
{
    return s->reorder == nullptr ? primary : mlocCollationReorderPrimary(s->reorder, primary);
}

// The weight of an element at the level, 0 for none.
static uint32_t Weight(const Level* l, uint64_t element, bool variable)
{
    uint32_t primary = (uint32_t)(element >> 32);
    uint32_t lower = (uint32_t)element;
    switch (l->level)
    {
    case collationPrimary:
        return variable ? 0 : Reordered(l->settings, primary);
    case collationSecondary:
        return lower >> 16;
    case collationTertiary:
        return Tertiary(l->settings, lower);
    case levelCase:
        return Case(l->settings, primary, lower);
    default:
    {
        // The compares read no other level.
        MLOC_ASSERT(l->level == collationQuaternary);
        // ICU's quaternary: a variable element's primary, else the
        // weight in the tertiary's bits 7..6 above every primary.
        uint32_t q = lower & 0xFFFF;
        return q <= 0x0100 ? Reordered(l->settings, primary) : q | 0xFFFFFF3F;
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
                        uint32_t level, NfdText a, NfdText b, const History* h)
{
    Level left;
    Level right;
    Start(&left, root, settings, level, a, h);
    Start(&right, root, settings, level, b, h);
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

bool mlocCollationPrimaries(const CollationRoot* root, const CollationSettings* settings,
                            NfdText text, uint32_t* weightsOut, uint32_t capacity,
                            uint32_t* countOut)
{
    Level l;
    Start(&l, root, settings, collationPrimary, text, &(History){.count = 0});
    uint32_t count = 0;
    for (uint32_t w = Next(&l); w != End(collationPrimary); w = Next(&l))
    {
        if (count == capacity)
        {
            *countOut = count;
            return false;
        }
        weightsOut[count++] = w;
    }
    *countOut = count;
    return true;
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

// One segment's secondaries of each text, the pairs lined up from the
// end: the order of the last pair that differs, else of the counts; and
// whether either text ended.
static int CompareSegment(Level* left, Level* right, bool* endedOut)
{
    Level l = *left;
    Level r = *right;
    bool leftEnded = false;
    bool rightEnded = false;
    uint32_t leftCount = CountSegment(left, &leftEnded);
    uint32_t rightCount = CountSegment(right, &rightEnded);
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
    MLOC_ASSERT(order != 0 || leftEnded == rightEnded);
    *endedOut = leftEnded || rightEnded;
    return order;
}

// The secondary level backwards within each segment, as ICU compares it,
// with nothing buffered: each segment's secondaries are counted, then
// read forwards again with the longer one's surplus skipped, so that
// the pairs line up from the end, and the last pair that differs decides.
static int Backwards(const CollationRoot* root, const CollationSettings* settings, NfdText a,
                     NfdText b, const History* h)
{
    Level left;
    Level right;
    Start(&left, root, settings, collationSecondary, a, h);
    Start(&right, root, settings, collationSecondary, b, h);
    for (;;)
    {
        bool ended = false;
        int order = CompareSegment(&left, &right, &ended);
        if (order != 0 || ended)
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
static int Identical(NfdText a, NfdText b)
{
    NfdStream left;
    NfdStream right;
    mlocNfdStartText(&left, a);
    mlocNfdStartText(&right, b);
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

static bool InRanges(const CollationRoot* unit, uint32_t c)
{
    return unit != nullptr && mlocCollationInUnsafe(unit->unsafe, unit->unsafeCount, c);
}

bool mlocCollationUnsafeBackward(const CollationRoot* root, const CollationSettings* settings,
                                 uint32_t codePoint)
{
    uint32_t d[nfdMaxDecomposition];
    uint8_t lead = 0;
    uint8_t trail = 0;
    (void)mlocNfdDecompose(codePoint, d, &lead, &trail);
    return lead != 0 || InRanges(root, codePoint) || InRanges(root, d[0]) ||
           InRanges(settings->tailoring, codePoint) || InRanges(settings->tailoring, d[0]) ||
           (settings->numeric && muniGetDecimalDigitValue(codePoint) >= 0);
}

// Whether the code point at a position of a text, if any, is
// unsafe-backward.
static bool UnsafeAt(const CollationRoot* root, const CollationSettings* settings, NfdText text,
                     size_t at)
{
    size_t size = 0;
    return at < text.length &&
           mlocCollationUnsafeBackward(root, settings, mlocNfdDecodeAt(text, at, &size));
}

// Whether a position is inside a code point: a UTF-8 continuation byte,
// or a UTF-16 trail surrogate after a lead.
static bool Inside(NfdText text, size_t at)
{
    if (at == 0 || at >= text.length)
    {
        return false;
    }
    return text.units != nullptr
               ? text.units[at] - 0xDC00u < 0x400 && text.units[at - 1] - 0xD800u < 0x400
               : (text.bytes[at] & 0xC0) == 0x80;
}

size_t mlocCollationIdenticalPrefix(const CollationRoot* root, const CollationSettings* settings,
                                    NfdText a, NfdText b)
{
    size_t limit = a.length < b.length ? a.length : b.length;
    size_t p = 0;
    // Texts of two encodings (an index's UTF-16 name and its UTF-8
    // boundaries) share no prefix of units. Shifted, a variable element
    // before the cut makes the ignorables after it read as 0, a state no
    // backing up over unsafe code points keeps: "[" against "[" U+0005
    // U+0323 is equal by the keys, and ICU4C 79's compare, which skips
    // the prefix there too, orders them apart. With backwards secondaries
    // the prefix's are the last read, and they decide where one text's
    // others are the start of the other's (fr-CA: U+0323 against U+0323
    // U+001F U+0323 U+0308 U+0308). No prefix is skipped in either case.
    if ((a.units != nullptr) != (b.units != nullptr) || settings->shifted || settings->backwards)
    {
        return 0;
    }
    if (a.units != nullptr)
    {
        while (p < limit && a.units[p] == b.units[p])
        {
            p++;
        }
    }
    else
    {
        while (p < limit && a.bytes[p] == b.bytes[p])
        {
            p++;
        }
    }
    while (p > 0 && (Inside(a, p) || Inside(b, p)))
    {
        p--;
    }
    if (p > 0 && (UnsafeAt(root, settings, a, p) || UnsafeAt(root, settings, b, p)))
    {
        size_t size = 0;
        do
        {
            p = mlocNfdBefore(a, p);
        } while (p > 0 &&
                 mlocCollationUnsafeBackward(root, settings, mlocNfdDecodeAt(a, p, &size)));
    }
    // Backing up reads a alone: the texts share their units up to p, so
    // where p falls in a code point it falls alike in b.
    MLOC_ASSERT(Inside(a, p) == Inside(b, p));
    return p;
}

// A text from a position.
static NfdText From(NfdText text, size_t at)
{
    return text.units != nullptr ? mlocNfdUtf16(text.units + at, text.length - at)
                                 : mlocNfdUtf8(text.bytes + at, text.length - at);
}

int mlocCollationCompareFrom(const CollationRoot* root, const CollationSettings* settings,
                             NfdText a, NfdText b, size_t prefix)
{
    // The prefix is the texts' identical one (mlocCollationIdenticalPrefix).
    MLOC_ASSERT(prefix <= a.length && prefix <= b.length);
    History h = Before(a, prefix);
    a = From(a, prefix);
    b = From(b, prefix);
    int order = CompareLevel(root, settings, collationPrimary, a, b, &h);
    if (order == 0 && settings->strength >= collationSecondary)
    {
        order = settings->backwards ? Backwards(root, settings, a, b, &h)
                                    : CompareLevel(root, settings, collationSecondary, a, b, &h);
    }
    if (order == 0 && settings->caseLevel)
    {
        order = CompareLevel(root, settings, levelCase, a, b, &h);
    }
    for (uint32_t level = collationTertiary;
         order == 0 && level <= settings->strength && level <= collationQuaternary; level++)
    {
        order = CompareLevel(root, settings, level, a, b, &h);
    }
    if (order == 0 && settings->strength == collationIdentical)
    {
        order = Identical(a, b);
    }
    return order;
}

int mlocCollationCompareText(const CollationRoot* root, const CollationSettings* settings,
                             NfdText a, NfdText b)
{
    return mlocCollationCompareFrom(root, settings, a, b,
                                    mlocCollationIdenticalPrefix(root, settings, a, b));
}
