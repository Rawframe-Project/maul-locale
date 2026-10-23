// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A text's processed elements backward, as ICU4C's
// UCollationPCE::previousProcessed gives them: the raw 32-bit halves
// before a position, walking back, gathered to one with a primary that is
// not a continuation, then that chunk processed forward and handed out
// last first. The shifted state goes on from one chunk to the one before
// it, so a run without a primary at the text's start is processed as if
// the chunk after it came first.
//
// The halves come from pieces read forward: each piece ends where the
// one after it starts (the first ends at the walk's start, the text
// after it unread) and starts at a safe code point
// (mlocSearchPieceStart), its prefix history the code points before.
// Ranges are the forward iterator's, which ICU4C's backward offsets
// equal.

#ifndef MLOC_SRC_SEARCH_BACK_H
#define MLOC_SRC_SEARCH_BACK_H

#include "search_elements.h"

#include <stddef.h>
#include <stdint.h>

// The halves a piece or a chunk holds at most: a longer run of unsafe
// code points, or of halves without a primary, sets full.
enum
{
    searchBackHalves = 128
};

typedef struct SearchHalf
{
    uint32_t half;
    uint32_t low;
    uint32_t high;
} SearchHalf;

typedef struct SearchProcessed
{
    uint64_t element;
    uint32_t low;
    uint32_t high;
} SearchProcessed;

typedef struct SearchBack
{
    const CollationRoot* root;
    const CollationSettings* settings;
    NfdText text;
    // The piece being read, whose settings process the halves.
    SearchElements piece;
    // The last half processed was shifted.
    bool afterShifted;
    // Where the piece read last starts: the next one ends there.
    size_t start;
    // The piece's halves in text order, taken from the end.
    SearchHalf halves[searchBackHalves];
    uint32_t halfCount;
    // The chunk being gathered, walking back.
    SearchHalf chunk[searchBackHalves];
    // The chunk's processed elements in text order, handed out from the
    // end.
    SearchProcessed processed[searchBackHalves];
    uint32_t processedCount;
    // A piece or a chunk did not fit.
    bool full;
} SearchBack;

// Starts the walk back from a position.
void mlocSearchBackStart(SearchBack* b, const CollationRoot* root,
                         const CollationSettings* settings, NfdText text, size_t from);

// The previous processed element and its range, or searchEnd at the
// text's start (0 as both), or when full is set.
uint64_t mlocSearchPreviousElement(SearchBack* b, uint32_t* lowOut, uint32_t* highOut);

#endif
