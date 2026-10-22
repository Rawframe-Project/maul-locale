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
    // Where in the whole text the iterator's text starts.
    uint32_t base;
} SearchElements;

void mlocSearchElementsStart(SearchElements* e, const CollationRoot* root,
                             const CollationSettings* settings, NfdText text);

// Starts at a position as ICU4C's setOffset places its iterator: back
// over unsafe code points to a safe one (a mark, a code point whose NFD
// starts in the root's or the tailoring's unsafe ranges, a digit under
// numeric), then forward to the last boundary between the elements of
// what follows that is at or before the position; the iterator starts
// there, the code points before it its prefix history. The text holds
// at most UINT32_MAX bytes or units, as the ranges are 32-bit.
void mlocSearchElementsStartAt(SearchElements* e, const CollationRoot* root,
                               const CollationSettings* settings, NfdText text, size_t from);

// Where mlocSearchElementsStartAt starts for a position: the last
// boundary between elements at or before it.
size_t mlocSearchStartPosition(const CollationRoot* root, const CollationSettings* settings,
                               NfdText text, size_t from);

// Starts on the text between two positions, the code points before the
// start its prefix history; ranges count from the whole text's start.
void mlocSearchElementsStartPiece(SearchElements* e, const CollationRoot* root,
                                  const CollationSettings* settings, NfdText text, size_t start,
                                  size_t limit);

// Where a piece of text that ends at a position may start, read forward
// as ICU4C's previous reads it: the code point before the end and back
// over the unsafe ones before it (as mlocSearchElementsStartAt backs
// up), so no element crosses the start. end is above 0.
size_t mlocSearchPieceStart(const CollationRoot* root, const CollationSettings* settings,
                            NfdText text, size_t end);

// ICU4C's CollationElementIterator::next: the next 32-bit half and its
// range, relative to the iterator's text; false at its end.
bool mlocSearchNextHalf(SearchElements* e, uint32_t* halfOut, uint32_t* lowOut, uint32_t* highOut);

// ICU4C's processCE on a 32-bit half, keeping its shifted state in e: 0
// for one passed over.
uint64_t mlocSearchProcess(SearchElements* e, uint32_t half);

// The next processed element and its range, or searchEnd at the end with
// the text's length as both.
uint64_t mlocSearchNextElement(SearchElements* e, uint32_t* lowOut, uint32_t* highOut);

// Whether a pattern has an element usearch keeps (ICU4C's getCE over its
// 32-bit halves: masked to the strength, a variable one dropped when
// shifted below the quaternary strength); usearch refuses a pattern
// with none.
bool mlocSearchHasElements(const CollationRoot* root, const CollationSettings* settings,
                           NfdText text);

#endif
