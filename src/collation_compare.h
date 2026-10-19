// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The general compare of two UTF-8 texts over a root unit (record
// mloc-0067): UCA's levels one at a time, as ICU4C's CollationCompare
// orders them. At each level the elements whose weight there is zero are
// passed over, and a text that ends first is the lesser. Primaries are
// compared whole, secondaries forwards, tertiaries without the case
// bits; the alternate setting is non-ignorable. Nothing is buffered: a
// level that the one before leaves equal reads both texts again.

#ifndef MLOC_SRC_COLLATION_COMPARE_H
#define MLOC_SRC_COLLATION_COMPARE_H

#include "collation_tables.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    collationPrimary = 1,
    collationSecondary = 2,
    collationTertiary = 3,
};

// -1, 0 or 1 as a sorts before, with or after b up to the strength.
int mlocCollationCompare(const CollationRoot* root, uint32_t strength, const uint8_t* a,
                         size_t aLength, const uint8_t* b, size_t bLength);

#endif
