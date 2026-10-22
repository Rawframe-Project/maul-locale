// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Builds the root collation unit (src/collation_tables.h, record
// mloc-0065) from the root as read (collation_root.h). As ICU4X's data
// does, it leaves out every mapping whose first code point has a
// canonical decomposition, Hangul syllables and surrogates: the runtime
// decomposes text to NFD before it looks a code point up, so they are
// never read.

#ifndef MLOCGEN_COLLATION_UNIT_H
#define MLOCGEN_COLLATION_UNIT_H

#include "collation_root.h"

#include <stddef.h>
#include <stdint.h>

// The unit's payload; null with a message when a mapping cannot be held
// (a context longer than the unit allows, a prefix with a contraction).
uint8_t* CollationRootUnit(const RootTable* table, size_t* sizeOut, char* error, size_t errorSize);

// The radical-stroke Han table of a root's [radical] ranges
// (src/collation_tables.h); the caller frees the bytes.
uint8_t* CollationHanUnit(const RootTable* table, size_t* sizeOut);

// Writes a finished unit's hash at an offset (mlocCollationUnitHash), the
// last step of its bytes.
void CollationSeal(uint8_t* unit, size_t size, uint32_t at);

// Whether the builder leaves a mapping starting with this code point out.
bool CollationLeftOut(uint32_t codePoint);

// A script start's sample as the runtime reads it, in NFD: a Hangul
// syllable as its jamo (FractionalUCA's FDD1 AC00, which an NFD text never
// holds), any other code point as itself. Its length, at most 3.
uint32_t CollationSampleText(uint32_t sample, uint32_t* textOut);

#endif
