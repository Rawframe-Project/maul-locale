// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collator a mlocCollator value holds (record mloc-0069), shared by
// the compare (collate.c) and search (search.c).

#ifndef MLOC_SRC_COLLATOR_H
#define MLOC_SRC_COLLATOR_H

#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collation_reorder.h"
#include "collation_tables.h"

#include "maul-locale/collate.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum
{
    collatorStamp = 0x43,
};

typedef struct Collator
{
    uint32_t stamp;
    // 1 when the reordering is in use (a byte, read only after the stamp).
    uint8_t reordered;
    // 1 when the locale's tailoring maps code points.
    uint8_t tailored;
    // 1 when it orders Han by the radical-stroke table.
    uint8_t radical;
    // 1 when the fast Latin compare serves its settings.
    uint8_t fastLatin;
    // The keys' version: the format, the root unit's bytes and the
    // tailoring's, hashed.
    uint64_t version;
    CollationRoot root;
    CollationRoot tailoring;
    CollationHan han;
    // Its reorder pointer is set at each compare: a collator is copied.
    CollationSettings settings;
    CollationReorder reorder;
    FastLatinOptions fast;
} Collator;

static_assert(sizeof(Collator) <= sizeof(mlocCollator), "a collator fits its value");
static_assert(offsetof(Collator, stamp) == 0, "the stamp is read first, alone");

// Points a collator's settings at its own reordering, tailoring and Han
// table, after it was copied.
static inline void mlocPointCollator(Collator* c)
{
    c->settings.reorder = c->reordered != 0 ? &c->reorder : nullptr;
    c->settings.tailoring = c->tailored != 0 ? &c->tailoring : nullptr;
    c->settings.han = c->radical != 0 ? &c->han : nullptr;
}

// A collator the library made, copied out with its settings pointing at
// its reordering; false for any other.
static inline bool mlocReadCollator(const mlocCollator* collator, Collator* c)
{
    uint32_t stamp = 0;
    if (collator == nullptr)
    {
        return false;
    }
    memcpy(&stamp, collator, sizeof(stamp));
    if (stamp != collatorStamp)
    {
        return false;
    }
    memcpy(c, collator, sizeof(*c));
    mlocPointCollator(c);
    return true;
}

#endif
