// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root elements the tailoring builder reads, as ICU4C's
// CollationBaseDataBuilder::buildRootElementsTable makes them and its
// CollationRootElements reads them: every distinct element of the root
// without its case bits, in order, so that the builder can ask for the
// weight before or after a root weight at each level.
//
// The table: five index units, then the elements without a primary
// (secondary and tertiary units), then the primaries. A primary unit
// holds a primary of at most three bytes; each later set of secondary
// and tertiary weights under it is a unit with the delta flag, the
// common set implied. A run of primaries a fixed step apart, all with
// common weights, is its first and last primary, the last carrying the
// step; Han's primaries are such runs. A sentinel above every primary
// ends the table. The table is built in the generator, never shipped.

#ifndef MLOCGEN_COLLATION_ROOT_ELEMENTS_H
#define MLOCGEN_COLLATION_ROOT_ELEMENTS_H

#include "collation_root.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    // The index units: where the tertiary, secondary and primary units
    // start, the common secondary and tertiary, and the bounds (bits
    // 31..24 the last common secondary byte, 23..16 the first ignorable
    // secondary byte, 7..0 the first ignorable tertiary byte).
    rootElementsFirstTertiaryIndex = 0,
    rootElementsFirstSecondaryIndex = 1,
    rootElementsFirstPrimaryIndex = 2,
    rootElementsCommonIndex = 3,
    rootElementsBoundsIndex = 4,
    rootElementsIndexCount = 5,
    // A unit of secondary and tertiary weights rather than a primary.
    rootElementsDeltaFlag = 0x80,
    // The step of a primary that ends a run.
    rootElementsStepMask = 0x7F,
};

typedef struct RootElements
{
    uint32_t* units;
    int32_t length;
    // Han's primaries, from the first to one past the last character's;
    // 0 without Han ranges.
    uint32_t firstHan;
    uint32_t lastHan;
} RootElements;

// Builds the table; 0 with a message when an element is one the table
// cannot hold (a four-byte primary, a secondary or tertiary of 01 xx or
// less, a Han primary under weights below the common ones).
int RootElementsBuild(const RootTable* table, RootElements* out, char* error, size_t errorSize);

void RootElementsFree(RootElements* elements);

// The bounds: the upper limit of the tertiaries of primary and
// secondary elements, the last common secondary (the lower limit of the
// secondaries of primary elements), and the upper limit of those
// secondaries.
static inline uint32_t RootElementsTertiaryBoundary(const RootElements* e)
{
    return (e->units[rootElementsBoundsIndex] << 8) & 0xFF00;
}

static inline uint32_t RootElementsLastCommonSecondary(const RootElements* e)
{
    return (e->units[rootElementsBoundsIndex] >> 16) & 0xFF00;
}

static inline uint32_t RootElementsSecondaryBoundary(const RootElements* e)
{
    return (e->units[rootElementsBoundsIndex] >> 8) & 0xFF00;
}

// The first and last secondary and tertiary weights of the elements
// without a primary, those with a tertiary only, and the first primary.
static inline uint32_t RootElementsFirstTertiary(const RootElements* e)
{
    return e->units[e->units[rootElementsFirstTertiaryIndex]] & ~(uint32_t)rootElementsDeltaFlag;
}

static inline uint32_t RootElementsLastTertiary(const RootElements* e)
{
    return e->units[e->units[rootElementsFirstSecondaryIndex] - 1] &
           ~(uint32_t)rootElementsDeltaFlag;
}

static inline uint32_t RootElementsFirstSecondary(const RootElements* e)
{
    return e->units[e->units[rootElementsFirstSecondaryIndex]] & ~(uint32_t)rootElementsDeltaFlag;
}

static inline uint32_t RootElementsLastSecondary(const RootElements* e)
{
    return e->units[e->units[rootElementsFirstPrimaryIndex] - 1] & ~(uint32_t)rootElementsDeltaFlag;
}

static inline uint32_t RootElementsFirstPrimary(const RootElements* e)
{
    return e->units[e->units[rootElementsFirstPrimaryIndex]];
}

// The queries, as ICU's of the same names. A primary passed must be a
// root primary (in a run counts) of at most three bytes, at least the
// first one and below the sentinel; a primary's index is what
// RootElementsFindPrimary gives, or 0 for the primary 0.

// The index of the unit with p, or of the run p is in.
int32_t RootElementsFindPrimary(const RootElements* e, uint32_t p);

// The root primary before p, which must be above the first one.
uint32_t RootElementsPrimaryBefore(const RootElements* e, uint32_t p, bool compressible);

// The root primary after p.
uint32_t RootElementsPrimaryAfter(const RootElements* e, uint32_t p, int32_t index,
                                  bool compressible);

// The root secondary before [p, s], or 0100 when s is p's first.
uint32_t RootElementsSecondaryBefore(const RootElements* e, uint32_t p, uint32_t s);

// The root tertiary before [p, s, t], or 0100 when t is the first.
uint32_t RootElementsTertiaryBefore(const RootElements* e, uint32_t p, uint32_t s, uint32_t t);

// The root secondary after [p, s], or the limit of p's secondaries.
uint32_t RootElementsSecondaryAfter(const RootElements* e, int32_t index, uint32_t s);

// The root tertiary after [p, s, t], or the limit of the tertiaries.
uint32_t RootElementsTertiaryAfter(const RootElements* e, int32_t index, uint32_t s, uint32_t t);

// The last root element with a primary below p, and the first with a
// primary of at least p, for a p that need not be a root primary (the
// bounds of a reordering group); 0 for p 0.
uint64_t RootElementsLastBefore(const RootElements* e, uint32_t p);
uint64_t RootElementsFirstAtLeast(const RootElements* e, uint32_t p);

#endif
