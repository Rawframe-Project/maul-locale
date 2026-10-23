// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An index's buckets, as ICU4C's AlphabeticIndex makes them: the script
// boundaries sorted (initLabels' first and last bound the labels), the
// labels filtered, sorted and thinned, then createBucketList's underflow,
// inflow, label, invisible and overflow buckets, the pinyin ones shown as
// the ASCII ones and the inflow ones next to another special merged. The
// value is written as index_value.h lays it out; a first pass with no
// buffer gives an upper bound of its size.

#ifndef MLOC_SRC_INDEX_BUILD_H
#define MLOC_SRC_INDEX_BUILD_H

#include "collator.h"
#include "index_labels.h"

#include "maul-locale/index.h"

#include <stddef.h>
#include <stdint.h>

// The underflow, inflow and overflow labels, UTF-8.
typedef struct IndexSpecials
{
    const uint8_t* text[3];
    uint32_t length[3];
} IndexSpecials;

// Builds over a collator at primary strength and its candidates, which
// it sorts and filters in place. sizeOut receives the size written, or
// with mloc_errorCapacity the size needed (an upper bound).
mlocResult mlocIndexBuild(const Collator* primary, IndexCandidates* candidates,
                          const IndexSpecials* specials, uint32_t maxLabelCount, uint8_t* out,
                          size_t capacity, size_t* sizeOut);

#endif
