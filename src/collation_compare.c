// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "collation_compare.h"

#include "collation_iterator.h"

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

// What a level reads after a text's last element: below every weight,
// as ICU's NO_CE is (primary 1, secondary and tertiary 0x0100).
static uint32_t End(uint32_t level)
{
    return level == collationSecondary || level == collationTertiary ? 0x0100 : 1;
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
        return variable ? 0 : lower >> 16;
    case collationTertiary:
        return variable ? 0 : lower & 0x3F3F;
    default:
    {
        // ICU's quaternary: a variable element's primary, else the
        // weight in the tertiary's bits 7..6 above every primary.
        uint32_t q = variable ? 0 : lower & 0xFFFF;
        return q <= 0x0100 ? primary : q | 0xFFFFFF3F;
    }
    }
}

// The next non-zero weight of the level, or the end's.
static uint32_t Next(Level* l)
{
    uint64_t element = 0;
    while (mlocCollationNextElement(&l->it, &element))
    {
        uint32_t primary = (uint32_t)(element >> 32);
        bool variable = false;
        if (l->settings->shifted)
        {
            if (primary > collationMergeSeparator && primary <= l->settings->variableTop)
            {
                variable = true;
                l->afterVariable = true;
                element &= 0xFFFFFFFF00000000ULL;
            }
            else if (primary == 0 && l->afterVariable)
            {
                element = 0;
            }
            else if (primary != 0)
            {
                l->afterVariable = false;
            }
        }
        uint32_t w = element == 0 ? 0 : Weight(l, element, variable);
        if (w != 0)
        {
            return w;
        }
    }
    return End(l->level);
}

static int CompareLevel(const CollationRoot* root, const CollationSettings* settings,
                        uint32_t level, const uint8_t* a, size_t aLength, const uint8_t* b,
                        size_t bLength)
{
    Level left = {.settings = settings, .level = level};
    Level right = {.settings = settings, .level = level};
    mlocCollationIteratorStart(&left.it, root, a, aLength);
    mlocCollationIteratorStart(&right.it, root, b, bLength);
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

int mlocCollationCompare(const CollationRoot* root, const CollationSettings* settings,
                         const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    int order = 0;
    for (uint32_t level = collationPrimary; order == 0 && level <= settings->strength; level++)
    {
        order = CompareLevel(root, settings, level, a, aLength, b, bLength);
    }
    return order;
}
