// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder's weight allocator (record mloc-0071), as
// ICU4C's CollationWeights allocates: n weights strictly between two
// weights of a level, each of one to four bytes (left-aligned in 32
// bits), within the bytes each position allows, as short as the gap
// permits. A gap too small is refused, never wrapped.

#ifndef MLOCGEN_COLLATION_WEIGHTS_H
#define MLOCGEN_COLLATION_WEIGHTS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct WeightRange
{
    uint32_t start;
    uint32_t end;
    int32_t length;
    int32_t count;
} WeightRange;

typedef struct CollationWeights
{
    // The length of the middle range's weights: 1 for primaries, 3 for
    // secondaries and tertiaries (whose first two bytes are 0).
    int32_t middleLength;
    // The bytes allowed at positions 1 to 4.
    uint32_t minBytes[5];
    uint32_t maxBytes[5];
    WeightRange ranges[7];
    int32_t rangeIndex;
    int32_t rangeCount;
} CollationWeights;

// The bytes of primaries: a lead byte from 03 to FF, a second byte from
// 04 to FE when the lead byte is compressible (from 02 to FF when not),
// later bytes from 02 to FF.
void CollationWeightsForPrimary(CollationWeights* w, bool compressible);

// Secondaries: 16-bit, a first byte from 02 to FF, a second from 02 to FF.
void CollationWeightsForSecondary(CollationWeights* w);

// Tertiaries: as secondaries, each byte at most 3F (the case bits clear).
void CollationWeightsForTertiary(CollationWeights* w);

// Prepares n weights strictly between lowerLimit and upperLimit; false
// when they do not fit.
bool CollationWeightsAllocate(CollationWeights* w, uint32_t lowerLimit, uint32_t upperLimit,
                              int32_t n);

// The next allocated weight, rising; 0xFFFFFFFF once all are given.
uint32_t CollationWeightsNext(CollationWeights* w);

// A weight's length in bytes, 1 to 4.
int32_t CollationWeightLength(uint32_t weight);

#endif
