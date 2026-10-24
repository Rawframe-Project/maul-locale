// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A built tailoring written as a unit over the root
// (src/collation_tables.h): a trie of the tailored code points' CE32s, the
// fallback word for every other, and per code point a prefix list over
// contraction lists as its mappings ask. A prefix whose mappings are
// contractions only falls back to the longest shorter prefix it ends with,
// as ICU4C's built contexts do. The rules' settings and reorder codes go
// in the head. A locale's types are one unit of its key.

#ifndef MLOCGEN_COLLATION_TAILORING_H
#define MLOCGEN_COLLATION_TAILORING_H

#include "collation_mappings.h"
#include "collation_rules.h"

#include <stddef.h>
#include <stdint.h>

// The unit's bytes, the caller frees them; null with a message when a
// mapping cannot be held or the reorder codes make no reordering.
// settings are the builder's, or null for none; root is the root read
// back, over which the codes' reordering is made and stored.
uint8_t* CollationTailoringUnit(const CollationMappings* mappings,
                                const CollationRuleSettings* settings, const CollationRoot* root,
                                size_t* sizeOut, char* error, size_t errorSize);

// A type's written tailoring and its name as BCP 47's co spells it.
typedef struct CollationTypeBody
{
    const char* code;
    uint8_t* body;
    size_t size;
} CollationTypeBody;

// A locale's unit of its types (src/collation_tables.h), sorting them by
// name; defaultCode is "" when the locale names no default. The caller
// frees the bytes.
uint8_t* CollationTypesUnit(CollationTypeBody* types, size_t count, const char* defaultCode,
                            size_t* sizeOut);

#endif
