// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A unit's unsafe-backward ranges (src/collation_unsafe.h, record
// mloc-0074), found in the unit read back: every contraction list a trie
// value or a prefix list names, each entry's code points (those after
// the first), merged into ranges.

#ifndef MLOCGEN_COLLATION_UNSAFE_TABLE_H
#define MLOCGEN_COLLATION_UNSAFE_TABLE_H

#include "collation_tables.h"

#include <stddef.h>
#include <stdint.h>

// The unit with its ranges section after it, found from its view (the
// unit read back); the unit is reallocated.
uint8_t* CollationUnsafeAppend(uint8_t* unit, size_t* size, const CollationRoot* view);

#endif
