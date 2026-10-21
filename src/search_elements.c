// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "search_elements.h"

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

// ICU4C's processCE on a 32-bit half: 0 for one passed over.
static uint64_t Process(SearchElements* e, uint32_t half)
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

// ICU4C's CollationElementIterator::next: the next half and its range.
static bool NextHalf(SearchElements* e, uint32_t* halfOut, uint32_t* lowOut, uint32_t* highOut)
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
    while (NextHalf(e, &half, lowOut, highOut))
    {
        uint64_t processed = Process(e, half);
        if (processed != 0)
        {
            return processed;
        }
    }
    *lowOut = (uint32_t)e->it.nfd.length;
    *highOut = *lowOut;
    return searchEnd;
}
