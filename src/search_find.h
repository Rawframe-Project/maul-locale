// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What collation search's two directions share (record mloc-0074): the
// searcher's layout, ICU4C 79's buffer of the text's elements
// (CEIBuffer), its element comparison (compareCE64s), its midcluster
// rule and its identical check.

#ifndef MLOC_SRC_SEARCH_FIND_H
#define MLOC_SRC_SEARCH_FIND_H

#include "collator.h"
#include "search_back.h"
#include "search_elements.h"

#include "maul-locale/search.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    searcherStamp = 0x53,
    // The pattern's processed elements, and the text's held at once
    // (ICU4C's CEIBuffer, sized as it sizes it: the pattern's elements,
    // 32 more, and with a wildcard comparison 3 for each UTF-16 unit of
    // the pattern, 8 for one that may be a leading jamo).
    searchMaxElements = 480,
    searchRing = 512,
    searchExtra = 32,
    // ICU4C's compareCE64s results.
    ceMatch = -1,
    ceNoMatch = 0,
    ceSkipText = 1,
    ceSkipPattern = 2,
    // ICU4C's U_CE_LEVEL2_BASE and U_CE_LEVEL3_BASE: a common weight.
    levelBase = 5,
};

typedef struct Searcher
{
    uint32_t stamp;
    uint32_t comparison;
    uint32_t count;
    uint32_t ring;
    // The pattern's NFD, for the identical strength; 0 code points below.
    uint32_t nfdCount;
    Collator collator;
    uint64_t elements[searchMaxElements];
    uint32_t nfd[searchMaxElements];
} Searcher;

// An element of the text and its range (ICU4C's CEI).
typedef struct Cei
{
    uint64_t ce;
    uint32_t low;
    uint32_t high;
} Cei;

// ICU4C's CEIBuffer: the text's elements by index, the last size held,
// read forward from e or, when back is set, backward from it.
typedef struct Ring
{
    Cei buf[searchRing];
    uint32_t size;
    uint32_t first;
    uint32_t limit;
    SearchElements e;
    SearchBack* back;
} Ring;

// A text's view for a find: the text, the searcher's collator pointed at
// its own parts.
typedef struct Find
{
    const Searcher* s;
    Collator collator;
    NfdText text;
    Ring ring;
    bool full;
} Find;

// The element at an index: one held, or the next; null for one no
// longer held or not next, or when the walk back is full.
const Cei* mlocSearchRingGet(Ring* r, uint32_t index);

// An element's weight at a level, 1 to 4.
uint32_t mlocSearchLevel(uint64_t ce, int level);

// ICU4C's compareCE64s: ceMatch, ceNoMatch, ceSkipText or ceSkipPattern.
int mlocSearchCompare(uint64_t text, uint64_t pattern, uint32_t comparison);

// The ring's element at an index, full set when it is not held.
const Cei* mlocSearchAt(Find* f, uint32_t index);

// ICU4C's midcluster rule: the next element starts a primary inside its
// code point's range, at a normalization boundary.
bool mlocSearchMidCluster(const Find* f, const Cei* last, const Cei* next, uint32_t maxLimit);

// Whether the text between the bounds has the pattern's NFD, at the
// identical strength.
bool mlocSearchIdentical(const Find* f, size_t start, size_t end);

// A position inside a code point moved to its start.
size_t mlocSearchAlign(NfdText t, size_t at);

// Checks a find's arguments and points its collator; mloc_success or
// mloc_errorInvalid. The ring is empty, its source still to be started.
mlocResult mlocSearchOpenFind(Find* f, const mlocSearcher* searcher, NfdText text, size_t from,
                              const size_t* startOut, const size_t* endOut);

#endif
