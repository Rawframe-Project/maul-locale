// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The general compare of two UTF-8 texts over a root unit (records
// mloc-0067 and mloc-0069): UCA's levels one at a time, as ICU4C's
// CollationCompare orders them. At each level the elements whose weight
// there is zero are passed over, and a text that ends first is the
// lesser. Primaries are compared whole; secondaries forwards, or
// backwards within the segments the merge separator (U+FFFE) parts; a
// case level between the secondary and tertiary levels when asked;
// tertiaries without the case bits unless case first asks for them,
// upper case then first or last; with numeric on, runs of digits by
// their value; primaries through the script reordering, if any. The identical level compares the
// texts' NFD code points, U+FFFE below every other as ICU ranks it. With the alternate setting
// shifted, a variable element (a primary above the merge separator, up to the variable top) keeps
// only its primary, for the quaternary level, and the primary ignorables after it are passed over
// at every level. Nothing is buffered: a level that the one before leaves equal reads both texts
// again.

#ifndef MLOC_SRC_COLLATION_COMPARE_H
#define MLOC_SRC_COLLATION_COMPARE_H

#include "collation_reorder.h"
#include "collation_tables.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    collationPrimary = 1,
    collationSecondary = 2,
    collationTertiary = 3,
    collationQuaternary = 4,
    collationIdentical = 5,
    // caseFirst
    collationCaseFirstOff = 0,
    collationLowerFirst = 1,
    collationUpperFirst = 2,
};

typedef struct CollationSettings
{
    uint32_t strength;
    bool shifted;
    // The last variable primary, when shifted.
    uint32_t variableTop;
    // Secondaries backwards (French, [backwards 2]).
    bool backwards;
    bool caseLevel;
    uint32_t caseFirst;
    // Digits as numbers (numeric ordering, CLDR's kn).
    bool numeric;
    // The script reordering (CLDR's kr), or null; the caller keeps it.
    const CollationReorder* reorder;
} CollationSettings;

// CLDR's root defaults: tertiary, non-ignorable, the variable top at the
// end of punctuation (maxVariable punct).
CollationSettings mlocCollationDefaults(const CollationRoot* root);

// -1, 0 or 1 as a sorts before, with or after b.
int mlocCollationCompare(const CollationRoot* root, const CollationSettings* settings,
                         const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength);

#endif
