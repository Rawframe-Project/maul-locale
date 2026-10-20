// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation elements written as CE32s (collation_encode.h).

#include "collation_encode.h"

#include "collation_tables.h"
#include "table_gen.h"

#include <stdlib.h>

size_t CollationPush(CollationWords* w, uint64_t value)
{
    if (w->count == w->capacity)
    {
        w->capacity = w->capacity == 0 ? 1024 : w->capacity * 2;
        uint64_t* grown = realloc(w->items, w->capacity * sizeof(uint64_t));
        if (grown == nullptr)
        {
            GenDie("collation unit", "out of memory");
        }
        w->items = grown;
    }
    w->items[w->count] = value;
    return w->count++;
}

uint32_t CollationSpecial(uint32_t tag, size_t index, uint32_t length)
{
    return (uint32_t)index << 13 | length << 8 | collationSpecialByte | tag;
}

// The CE32 of one element, when one holds it: simple, long primary or
// long secondary.
static bool OneCe32(uint64_t element, uint32_t* out)
{
    uint32_t p = (uint32_t)(element >> 32);
    uint32_t lower = (uint32_t)element;
    if ((p & 0xFFFF) == 0 && (lower & 0x00FF00FF) == 0 && ((lower >> 8) & 0xFF) < 0xC0)
    {
        *out = p | (lower >> 16 & 0xFF00) | (lower >> 8 & 0xFF);
        return true;
    }
    if ((p & 0xFF) == 0 && p != 0 && lower == collationCommonSecondaryTertiary)
    {
        *out = p | collationSpecialByte | collationTagLongPrimary;
        return true;
    }
    if (p == 0 && (lower & 0xFF) == 0)
    {
        *out = (lower & 0xFFFF0000) | (lower & 0xFF00) | collationSpecialByte |
               collationTagLongSecondary;
        return true;
    }
    return false;
}

uint32_t CollationEncode(CollationWords* ces, CollationWords* ce32s, const uint64_t* elements,
                         uint32_t count)
{
    uint32_t one = 0;
    if (count == 1 && OneCe32(elements[0], &one))
    {
        return one;
    }
    bool small = true;
    for (uint32_t i = 0; i < count; i++)
    {
        small = small && OneCe32(elements[i], &one);
    }
    size_t index = small ? ce32s->count : ces->count;
    for (uint32_t i = 0; i < count; i++)
    {
        if (small)
        {
            (void)OneCe32(elements[i], &one);
            CollationPush(ce32s, one);
        }
        else
        {
            CollationPush(ces, elements[i]);
        }
    }
    return CollationSpecial(small ? collationTagExpansion32 : collationTagExpansion, index, count);
}
