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
// With a tailoring (record mloc-0071), a code point is read from it
// first and from the root where it gives the fallback word; the lists
// and expansions are read from the unit that gave the CE32, and a
// contraction a list names is matched in turn.
// With numeric on, a run of decimal digits (General_Category Nd) gives
// the primaries of its value, as ICU4C's appendNumericCEs: leading
// zeros dropped, the digits in segments of at most 254, each segment one
// element up to seven digits, else a length byte and digit pairs.

#ifndef MLOC_SRC_COLLATION_ITERATOR_H
#define MLOC_SRC_COLLATION_ITERATOR_H

#include "collation_tables.h"
#include "nfd_stream.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    // Code points read ahead; a discontiguous contraction looks further
    // through a copy of the stream.
    collationLookahead = 72,
    // Code points taken past the lookahead and not yet read.
    collationMaxDropped = 32,
    // A numeric segment's digits, and its most elements: a length byte
    // and two pairs, then three pairs an element, for 127 pairs.
    collationNumericDigits = 254,
    collationNumericElements = 43,
};

typedef struct CollationIterator
{
    const CollationRoot* root;
    // The tailoring read before the root, or null.
    const CollationRoot* tailoring;
    // The radical-stroke Han table, read for Han the tailoring leaves to
    // the root (the unihan types), or null.
    const CollationHan* han;
    NfdStream nfd;
    // Digits as numbers (UCA's numeric ordering, CLDR's kn); false after
    // start.
    bool numeric;
    // Elements still to be given: [ceNext, ceCount).
    uint64_t ces[collationNumericElements];
    uint32_t ceCount;
    uint32_t ceNext;
    // Code points read from the stream ahead of the one being given.
    uint32_t upcoming[collationLookahead];
    uint8_t upcomingClasses[collationLookahead];
    uint32_t upcomingFrom[collationLookahead];
    // Each one's count in the stream.
    uint32_t upcomingAt[collationLookahead];
    uint32_t upcomingCount;
    // The source offset where the last element read with its range ended.
    uint32_t high;
    // Code points read from the stream so far, and those a discontiguous
    // contraction took past the lookahead (by that count), still to be
    // read and dropped. The marks it skipped there need no new range: a
    // segment longer than the lookahead is longer than the NFD window,
    // so they already come from its end.
    uint32_t taken;
    uint32_t dropped[collationMaxDropped];
    uint32_t droppedCount;
    // The last discontiguous scan that reached a starter or the end: the
    // count where it stopped, and the highest class it left in the
    // stream. A later scan inside it that has skipped a mark of that
    // class or higher can extend no further and stops, so a run of marks
    // costs each of its contractions no more than the first did.
    uint32_t scanEnd;
    uint8_t scanMax;
    // The last code points given, the latest at (historyAt - 1) % 8.
    uint32_t history[collationMaxContext];
    uint32_t historyAt;
    uint32_t historyCount;
} CollationIterator;

// Starts reading text through the root alone; set tailoring after to
// read through one.
void mlocCollationIteratorStartText(CollationIterator* it, const CollationRoot* root, NfdText text);

static inline void mlocCollationIteratorStart(CollationIterator* it, const CollationRoot* root,
                                              const uint8_t* text, size_t length)
{
    mlocCollationIteratorStartText(it, root, mlocNfdUtf8(text, length));
}

// The next element and the range of the source it comes from, as ICU4C's
// processed elements place them: the first element of a code point,
// contraction or numeric run spans from where it starts to where the
// next code point starts (the end of the text after the last); the rest
// of an expansion have empty ranges at that end. Inside a segment the
// NFD changes, only the whole segment has a range (see NfdStream). A
// discontiguous contraction spans to the end of its last code point,
// and the marks it skipped have empty ranges there.
bool mlocCollationNextElementAt(CollationIterator* it, uint64_t* elementOut, uint32_t* lowOut,
                                uint32_t* highOut);

// The next element; false at the end of the text. A completely
// ignorable element is given as 0.
bool mlocCollationNextElement(CollationIterator* it, uint64_t* elementOut);

#endif
