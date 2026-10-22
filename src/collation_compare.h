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
#include "nfd_stream.h"

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
    // The locale's tailoring, read before the root, or null; the caller
    // keeps it.
    const CollationRoot* tailoring;
    // The radical-stroke Han table a unihan tailoring reads, or null.
    const CollationHan* han;
} CollationSettings;

// CLDR's root defaults: tertiary, non-ignorable, the variable top at the
// end of punctuation (maxVariable punct).
CollationSettings mlocCollationDefaults(const CollationRoot* root);

// Whether a position before the code point may be inside a contraction,
// a mark's reordering or a numeric run (ICU4C's unsafe-backward set: a
// mark, a later code point of a contraction in the root or the
// tailoring, a digit under numeric).
bool mlocCollationUnsafeBackward(const CollationRoot* root, const CollationSettings* settings,
                                 uint32_t codePoint);

// ICU4C's identical prefix: how many bytes or units both texts begin
// with, backed up to a code point's start and then, where a code point
// at the cut is unsafe-backward, over the unsafe ones before it, so that
// what follows reads alone as it reads in the whole text (but for the
// prefix contexts, which the compare gives it from before the cut).
// None when one text is UTF-8 and the other UTF-16, when shifted, or
// with backwards secondaries.
size_t mlocCollationIdenticalPrefix(const CollationRoot* root, const CollationSettings* settings,
                                    NfdText a, NfdText b);

// -1, 0 or 1 as a sorts before, with or after b, compared from a prefix
// mlocCollationIdenticalPrefix gave (0 for the whole texts).
int mlocCollationCompareFrom(const CollationRoot* root, const CollationSettings* settings,
                             NfdText a, NfdText b, size_t prefix);

// -1, 0 or 1 as a sorts before, with or after b, the identical prefix
// skipped.
int mlocCollationCompareText(const CollationRoot* root, const CollationSettings* settings,
                             NfdText a, NfdText b);

static inline int mlocCollationCompare(const CollationRoot* root, const CollationSettings* settings,
                                       const uint8_t* a, size_t aLength, const uint8_t* b,
                                       size_t bLength)
{
    return mlocCollationCompareText(root, settings, mlocNfdUtf8(a, aLength),
                                    mlocNfdUtf8(b, bLength));
}

#endif
