// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A built tailoring written as a unit over the root (record mloc-0071,
// src/collation_tables.h): a trie of the tailored code points' CE32s,
// the fallback word for every other, and per code point a prefix list
// over contraction lists as its mappings ask. A prefix whose mappings
// are contractions only falls back to the longest shorter prefix it ends
// with, as ICU4C's built contexts do. The rules' settings and reorder
// codes go in the head.

#ifndef MLOCGEN_COLLATION_TAILORING_H
#define MLOCGEN_COLLATION_TAILORING_H

#include "collation_mappings.h"
#include "collation_rules.h"

#include <stddef.h>
#include <stdint.h>

// The unit's bytes, the caller frees them; null with a message when a
// mapping cannot be held.
uint8_t* CollationTailoringUnit(const CollationMappings* mappings, const CollationRules* rules,
                                size_t* sizeOut, char* error, size_t errorSize);

#endif
