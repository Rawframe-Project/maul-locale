// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_compare.h"

#include "collation_iterator.h"

// What a level reads after a text's last element: below every weight,
// as ICU's NO_CE is (primary 1, secondary and tertiary 0x0100).
static uint32_t End(uint32_t level)
{
    return level == collationPrimary ? 1 : 0x0100;
}

static uint32_t Weight(uint64_t element, uint32_t level)
{
    if (level == collationPrimary)
    {
        return (uint32_t)(element >> 32);
    }
    if (level == collationSecondary)
    {
        return (uint32_t)element >> 16;
    }
    return (uint32_t)element & 0x3F3F;
}

// The next non-zero weight of the level, or the end's.
static uint32_t Next(CollationIterator* it, uint32_t level)
{
    uint64_t element = 0;
    while (mlocCollationNextElement(it, &element))
    {
        uint32_t w = Weight(element, level);
        if (w != 0)
        {
            return w;
        }
    }
    return End(level);
}

static int CompareLevel(const CollationRoot* root, uint32_t level, const uint8_t* a, size_t aLength,
                        const uint8_t* b, size_t bLength)
{
    CollationIterator left;
    CollationIterator right;
    mlocCollationIteratorStart(&left, root, a, aLength);
    mlocCollationIteratorStart(&right, root, b, bLength);
    uint32_t end = End(level);
    for (;;)
    {
        uint32_t l = Next(&left, level);
        uint32_t r = Next(&right, level);
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

int mlocCollationCompare(const CollationRoot* root, uint32_t strength, const uint8_t* a,
                         size_t aLength, const uint8_t* b, size_t bLength)
{
    int order = 0;
    for (uint32_t level = collationPrimary; order == 0 && level <= strength; level++)
    {
        order = CompareLevel(root, level, a, aLength, b, bLength);
    }
    return order;
}
