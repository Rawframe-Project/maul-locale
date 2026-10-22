// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "search_elements.h"

#include "search_boundary.h"

#include "maul-unicode/properties.h"

void mlocSearchElementsStart(SearchElements* e, const CollationRoot* root,
                             const CollationSettings* settings, NfdText text)
{
    *e = (SearchElements){.strength = settings->strength,
                          .shifted = settings->shifted,
                          .variableTop = settings->variableTop};
    mlocCollationIteratorStartText(&e->it, root, text);
    e->it.tailoring = settings->tailoring;
    e->it.han = settings->han;
    e->it.numeric = settings->numeric;
}

uint64_t mlocSearchProcess(SearchElements* e, uint32_t half)
{
    uint64_t primary = half >> 16;
    uint64_t secondary = e->strength >= collationSecondary ? half >> 8 & 0xFF : 0;
    uint64_t tertiary = e->strength >= collationTertiary ? half & 0xFF : 0;
    uint64_t quaternary = 0;
    if ((e->shifted && e->variableTop > half && primary != 0) || (e->afterShifted && primary == 0))
    {
        if (primary == 0)
        {
            return 0;
        }
        quaternary = e->strength >= collationQuaternary ? primary : 0;
        primary = 0;
        secondary = 0;
        tertiary = 0;
        e->afterShifted = true;
    }
    else
    {
        quaternary = e->strength >= collationQuaternary ? 0xFFFF : 0;
        e->afterShifted = false;
    }
    return primary << 48 | secondary << 32 | tertiary << 16 | quaternary;
}

bool mlocSearchNextHalf(SearchElements* e, uint32_t* halfOut, uint32_t* lowOut, uint32_t* highOut)
{
    if (e->otherHalf != 0)
    {
        *halfOut = e->otherHalf;
        *lowOut = e->otherAt;
        *highOut = e->otherAt;
        e->otherHalf = 0;
        return true;
    }
    uint64_t element = 0;
    if (!mlocCollationNextElementAt(&e->it, &element, lowOut, highOut))
    {
        return false;
    }
    uint32_t p = (uint32_t)(element >> 32);
    uint32_t lower = (uint32_t)element;
    *halfOut = (p & 0xFFFF0000) | (lower >> 16 & 0xFF00) | (lower >> 8 & 0xFF);
    uint32_t second = p << 16 | (lower >> 8 & 0xFF00) | (lower & 0x3F);
    e->otherHalf = second != 0 ? second | 0xC0 : 0;
    e->otherAt = *highOut;
    return true;
}

uint64_t mlocSearchNextElement(SearchElements* e, uint32_t* lowOut, uint32_t* highOut)
{
    uint32_t half = 0;
    while (mlocSearchNextHalf(e, &half, lowOut, highOut))
    {
        uint64_t processed = mlocSearchProcess(e, half);
        if (processed != 0)
        {
            *lowOut += e->base;
            *highOut += e->base;
            return processed;
        }
    }
    *lowOut = e->base + (uint32_t)e->it.nfd.length;
    *highOut = *lowOut;
    return searchEnd;
}

static bool InRanges(const CollationRoot* unit, uint32_t c)
{
    return unit != nullptr && mlocCollationInUnsafe(unit->unsafe, unit->unsafeCount, c);
}

// Whether a position before the code point may be inside a contraction
// or a numeric run.
static bool Unsafe(const CollationRoot* root, const CollationSettings* s, uint32_t c)
{
    uint32_t d[nfdMaxDecomposition];
    uint8_t lead = 0;
    uint8_t trail = 0;
    (void)mlocNfdDecompose(c, d, &lead, &trail);
    return lead != 0 || InRanges(root, c) || InRanges(root, d[0]) || InRanges(s->tailoring, c) ||
           InRanges(s->tailoring, d[0]) || (s->numeric && muniGetDecimalDigitValue(c) >= 0);
}

// The text between two positions.
static NfdText Between(NfdText text, size_t start, size_t limit)
{
    return text.units != nullptr ? mlocNfdUtf16(text.units + start, limit - start)
                                 : mlocNfdUtf8(text.bytes + start, limit - start);
}

// The last boundary between elements, from a safe position, at or
// before the position: ICU4C's lastSafeOffset.
static size_t LastBoundary(const CollationRoot* root, const CollationSettings* settings,
                           NfdText text, size_t safe, size_t from)
{
    SearchElements scan;
    mlocSearchElementsStart(&scan, root, settings, Between(text, safe, text.length));
    size_t last = safe;
    uint64_t element = 0;
    uint32_t low = 0;
    uint32_t high = 0;
    while (safe + high < from && mlocCollationNextElementAt(&scan.it, &element, &low, &high))
    {
        last = safe + high <= from ? safe + high : last;
    }
    return last;
}

size_t mlocSearchStartPosition(const CollationRoot* root, const CollationSettings* settings,
                               NfdText text, size_t from)
{
    size_t start = from;
    if (from > 0 && from < text.length)
    {
        size_t offset = from;
        size_t size = 0;
        while (offset > 0 && Unsafe(root, settings, mlocNfdDecodeAt(text, offset, &size)))
        {
            offset = mlocSearchBefore(text, offset);
        }
        start = offset < from ? LastBoundary(root, settings, text, offset, from) : from;
    }
    return start;
}

void mlocSearchElementsStartAt(SearchElements* e, const CollationRoot* root,
                               const CollationSettings* settings, NfdText text, size_t from)
{
    mlocSearchElementsStartPiece(e, root, settings, text,
                                 mlocSearchStartPosition(root, settings, text, from), text.length);
}

size_t mlocSearchPieceStart(const CollationRoot* root, const CollationSettings* settings,
                            NfdText text, size_t end)
{
    size_t start = mlocSearchBefore(text, end);
    size_t size = 0;
    while (start > 0 && Unsafe(root, settings, mlocNfdDecodeAt(text, start, &size)))
    {
        start = mlocSearchBefore(text, start);
    }
    return start;
}

void mlocSearchElementsStartPiece(SearchElements* e, const CollationRoot* root,
                                  const CollationSettings* settings, NfdText text, size_t start,
                                  size_t limit)
{
    mlocSearchElementsStart(e, root, settings, Between(text, start, limit));
    e->base = (uint32_t)start;
    // The code points before, oldest first, for the prefixes.
    size_t back[collationMaxContext];
    uint32_t count = 0;
    for (size_t at = start; at > 0 && count < collationMaxContext; count++)
    {
        at = mlocSearchBefore(text, at);
        back[count] = at;
    }
    while (count-- > 0)
    {
        size_t size = 0;
        uint32_t d[nfdMaxDecomposition];
        uint8_t lead = 0;
        uint8_t trail = 0;
        uint32_t n = mlocNfdDecompose(mlocNfdDecodeAt(text, back[count], &size), d, &lead, &trail);
        for (uint32_t i = 0; i < n; i++)
        {
            mlocCollationIteratorRemember(&e->it, d[i]);
        }
    }
}

// ICU4C's getCE: a half as usearch keeps it for the pattern's elements,
// 0 for one it drops.
static uint32_t PatternCe(const SearchElements* e, uint32_t half)
{
    uint32_t mask = e->strength == collationPrimary     ? 0xFFFF0000
                    : e->strength == collationSecondary ? 0xFFFFFF00
                                                        : 0xFFFFFFFF;
    half &= mask;
    if (e->shifted)
    {
        if (e->variableTop > half)
        {
            half = e->strength >= collationQuaternary ? half & 0xFFFF0000 : 0;
        }
    }
    else if (e->strength >= collationQuaternary && half == 0)
    {
        half = 0xFFFF;
    }
    return half;
}

bool mlocSearchHasElements(const CollationRoot* root, const CollationSettings* settings,
                           NfdText text)
{
    SearchElements e;
    mlocSearchElementsStart(&e, root, settings, text);
    uint32_t half = 0;
    uint32_t low = 0;
    uint32_t high = 0;
    while (mlocSearchNextHalf(&e, &half, &low, &high))
    {
        if (PatternCe(&e, half) != 0)
        {
            return true;
        }
    }
    return false;
}
