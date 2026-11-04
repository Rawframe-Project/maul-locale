// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collator a mlocCollator value holds, shared by the compare
// (collate.c) and search (search.c).

#ifndef MLOC_SRC_COLLATOR_H
#define MLOC_SRC_COLLATOR_H

#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collation_fast_latin.h"
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

// The fast Latin compare's options for the collator's settings, over
// its tailoring's table, or the root's when it names the root's or the
// locale has no tailored mappings.
static inline void mlocSetCollatorFastLatin(Collator* c)
{
    const CollationRoot* unit =
        c->tailored != 0 && c->tailoring.fastLatinCount != fastLatinRootTable ? &c->tailoring
                                                                              : &c->root;
    CollationSettings s = c->settings;
    s.reorder = c->reordered != 0 ? &c->reorder : nullptr;
    c->fastLatin =
        mlocFastLatinOptions(&c->root, &s, unit->fastLatin, unit->fastLatinCount, &c->fast) ? 1 : 0;
}

// The head of a collator's value (its stamp, flags and version), copied
// out; false for bytes the library did not make, or whose reordering, when
// the head says it is in use, holds more ranges than it has room for. The
// value may lie at any alignment (an index holds one), so each part is
// copied by its bytes.
MLOC_NODISCARD static inline bool mlocReadCollatorHead(const void* value, Collator* c)
{
    if (value == nullptr)
    {
        return false;
    }
    memcpy(c, value, offsetof(Collator, root));
    if (c->stamp != collatorStamp)
    {
        return false;
    }
    uint32_t ranges = 0;
    if (c->reordered != 0)
    {
        memcpy(&ranges, (const uint8_t*)value + offsetof(Collator, reorder.rangeCount),
               sizeof(ranges));
    }
    return ranges <= collationMaxReorderRanges;
}

static_assert(offsetof(FastLatinOptions, digitsBail) - offsetof(FastLatinOptions, backwards) == 5,
              "the fast Latin options' six bools are side by side");

// Whether a collator value's fast Latin options hold a bool's byte in
// each bool, checked before they are read as bools: the six side by side,
// read at once, each 0 or 1. A fast compare reads them on every call.
MLOC_NODISCARD static inline bool mlocCollatorFastSound(const void* value)
{
    uint64_t flags = 0;
    memcpy(&flags, (const uint8_t*)value + offsetof(Collator, fast.backwards), 6);
    // The two bytes past them stay 0, so the mask holds in either byte
    // order.
    return (flags & 0xFEFEFEFEFEFEFEFEull) == 0;
}

// Whether a collator value's settings hold a bool's byte in each bool and
// a strength and case order the library has, checked before they are read.
MLOC_NODISCARD static inline bool mlocCollatorSettingsSound(const void* value)
{
    const uint8_t* b = value;
    unsigned flags =
        b[offsetof(Collator, settings.shifted)] | b[offsetof(Collator, settings.backwards)] |
        b[offsetof(Collator, settings.caseLevel)] | b[offsetof(Collator, settings.numeric)];
    uint32_t strength = 0;
    uint32_t caseFirst = 0;
    memcpy(&strength, b + offsetof(Collator, settings.strength), sizeof(strength));
    memcpy(&caseFirst, b + offsetof(Collator, settings.caseFirst), sizeof(caseFirst));
    return flags <= 1 && strength <= collationIdentical && caseFirst <= collationUpperFirst;
}

// The fast Latin compare's options, after the head; false for options
// whose bools are not bools.
MLOC_NODISCARD static inline bool mlocReadCollatorFast(const void* value, Collator* c)
{
    if (!mlocCollatorFastSound(value))
    {
        return false;
    }
    memcpy(&c->fast, (const uint8_t*)value + offsetof(Collator, fast), sizeof(c->fast));
    return true;
}

// The general path's parts, after the head: the root and the settings, and
// the tailoring, Han table and reordering only where the head says they
// are in use (the reordering alone is 520 bytes), the settings pointed at
// them: copying the whole value was most of a fast compare's time. False
// for settings whose bools or kinds are none the library writes.
MLOC_NODISCARD static inline bool mlocReadCollatorRest(const void* value, Collator* c)
{
    if (!mlocCollatorSettingsSound(value))
    {
        return false;
    }
    const uint8_t* b = value;
    memcpy(&c->root, b + offsetof(Collator, root), sizeof(c->root));
    memcpy(&c->settings, b + offsetof(Collator, settings), sizeof(c->settings));
    if (c->tailored != 0)
    {
        memcpy(&c->tailoring, b + offsetof(Collator, tailoring), sizeof(c->tailoring));
    }
    if (c->radical != 0)
    {
        memcpy(&c->han, b + offsetof(Collator, han), sizeof(c->han));
    }
    if (c->reordered != 0)
    {
        memcpy(&c->reorder, b + offsetof(Collator, reorder), sizeof(c->reorder));
    }
    mlocPointCollator(c);
    return true;
}

// A collator the library made, copied out whole with its settings
// pointing at its own parts, for a caller that keeps or copies it on (a
// searcher, an index); false for any other. A call that only compares
// reads the parts above instead.
MLOC_NODISCARD static inline bool mlocReadCollator(const void* value, Collator* c)
{
    if (!mlocReadCollatorHead(value, c) || !mlocCollatorFastSound(value) ||
        !mlocCollatorSettingsSound(value))
    {
        return false;
    }
    memcpy(c, value, sizeof(*c));
    mlocPointCollator(c);
    return true;
}

#endif
