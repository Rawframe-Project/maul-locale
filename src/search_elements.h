// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A text's processed elements for search (record mloc-0074), as ICU4C's
// UCollationPCE makes them: each 64-bit element split into ICU's two
// 32-bit halves (the second, when not 0, marked as a continuation), and
// each half reduced to the collator's strength, 16 bits a level:
// primary in bits 63..48, secondary 47..32, tertiary 31..16, quaternary
// 15..0. Under shifted, a half below the variable top keeps only its
// primary, as the quaternary (0 below the quaternary strength), and the
// primary ignorables after it are dropped; at the quaternary strength
// and above, every other half's quaternary is FFFF. Halves that reduce
// to 0 are passed over.
//
// Each element has a source range: a half's own element's, the second
// half's empty at the first's end, as ICU4C's iterator offsets go.

#ifndef MLOC_SRC_SEARCH_ELEMENTS_H
#define MLOC_SRC_SEARCH_ELEMENTS_H

#include "collation_compare.h"
#include "collation_iterator.h"

#include <stdint.h>

// ICU4C's UCOL_PROCESSED_NULLORDER: the end of the text.
constexpr uint64_t searchEnd = UINT64_MAX;

typedef struct SearchElements
{
    CollationIterator it;
    uint32_t strength;
    bool shifted;
    uint32_t variableTop;
    // The last half was shifted.
    bool afterShifted;
    // The continuation half still to be given, or 0, and where its
    // element ended.
    uint32_t otherHalf;
    uint32_t otherAt;
} SearchElements;

void mlocSearchElementsStart(SearchElements* e, const CollationRoot* root,
                             const CollationSettings* settings, NfdText text);

// The next processed element and its range, or searchEnd at the end with
// the text's length as both.
uint64_t mlocSearchNextElement(SearchElements* e, uint32_t* lowOut, uint32_t* highOut);

#endif
