// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Script reordering (record mloc-0069), as ICU4C's
// CollationData::makeReorderRanges and CollationSettings::setReordering
// build it over the root's script starts: the codes asked for (Maul
// Unicode's packed scripts and the five groups) move to the front in
// their order, the groups not named stay first, the rest follow in the
// root's order, and Zzzz ("others") sends the codes after it to the end.
// A primary's lead byte is mapped through a table; a lead byte that two
// ranges share is mapped through the ranges.

#ifndef MLOC_SRC_COLLATION_REORDER_H
#define MLOC_SRC_COLLATION_REORDER_H

#include "collation_tables.h"

#include "maul-locale/base.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    collationMaxReorderRanges = 256,
};

typedef struct CollationReorder
{
    // Primaries at or above it keep their place.
    uint32_t minHighNoReorder;
    // A lead byte's new one; 0 for a byte two ranges share.
    uint8_t table[256];
    // From the first shared byte on: each a range's limit (top 16 bits)
    // and the offset (low 16 bits) its lead bytes move by.
    uint32_t ranges[collationMaxReorderRanges];
    uint32_t rangeCount;
} CollationReorder;

// Builds the reordering for the codes. Duplicates, two codes of one
// range and a second Zzzz are refused with mloc_errorInvalid, an order
// whose ranges do not fit the lead bytes with mloc_errorCapacity. Codes
// the root has no range for are passed over. *changes is false when the
// order is the root's (nothing named, or Zzzz alone).
mlocResult mlocCollationMakeReorder(const CollationRoot* root, const uint32_t* codes, size_t count,
                                    CollationReorder* out, bool* changes);

static inline uint32_t mlocCollationReorderPrimary(const CollationReorder* r, uint32_t p)
{
    uint32_t b = r->table[p >> 24];
    if (b != 0 || p <= 1)
    {
        return b << 24 | (p & 0xFFFFFF);
    }
    if (p >= r->minHighNoReorder)
    {
        return p;
    }
    uint32_t q = p | 0xFFFF;
    uint32_t i = 0;
    while (i + 1 < r->rangeCount && q >= r->ranges[i])
    {
        i++;
    }
    return p + (r->ranges[i] << 24);
}

#endif
