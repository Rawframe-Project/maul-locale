// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation elements of UTF-8 text (record mloc-0066), one 64-bit
// element at a time, from a root unit (record mloc-0065) read through
// the NFD stream:
// - a trie value that is one element gives it; expansions give theirs
//   from the arrays; an offset's element is computed; the implicit word
//   gives the unassigned primary;
// - a prefix list is matched against the code points before, longest
//   entry first, from a history of the last eight;
// - a contraction list takes the longest entry the code points after
//   spell, then UTS #10's discontiguous rule: a mark not blocked by a
//   skipped mark of an equal or higher class, nor by a starter, extends
//   the match. The marks matched are taken out; the skipped ones follow.
//   The rule looks at most collationLookahead code points ahead.
// Hangul syllables reach the trie as jamo, which NFD gives.

#ifndef MLOC_SRC_COLLATION_ITERATOR_H
#define MLOC_SRC_COLLATION_ITERATOR_H

#include "collation_tables.h"
#include "nfd_stream.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    collationLookahead = 72,
};

typedef struct CollationIterator
{
    const CollationRoot* root;
    NfdStream nfd;
    // Elements still to be given: [ceNext, ceCount).
    uint64_t ces[collationMaxExpansion + 1];
    uint32_t ceCount;
    uint32_t ceNext;
    // Code points read from the stream ahead of the one being given.
    uint32_t upcoming[collationLookahead];
    uint8_t upcomingClasses[collationLookahead];
    uint32_t upcomingCount;
    // The last code points given, the latest at (historyAt - 1) % 8.
    uint32_t history[collationMaxContext];
    uint32_t historyAt;
    uint32_t historyCount;
} CollationIterator;

void mlocCollationIteratorStart(CollationIterator* it, const CollationRoot* root,
                                const uint8_t* text, size_t length);

// The next element; false at the end of the text. A completely
// ignorable element is given as 0.
bool mlocCollationNextElement(CollationIterator* it, uint64_t* elementOut);

#endif
