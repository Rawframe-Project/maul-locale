// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation elements written as CE32s (src/collation_tables.h), shared by
// the root unit's writer and the tailorings': a sequence of elements is
// one element's own CE32 when it fits, else an expansion of CE32s when
// each element fits one, else an expansion of 64-bit elements.

#ifndef MLOCGEN_COLLATION_ENCODE_H
#define MLOCGEN_COLLATION_ENCODE_H

#include <stddef.h>
#include <stdint.h>

typedef struct CollationWords
{
    uint64_t* items;
    size_t count;
    size_t capacity;
} CollationWords;

// Appends a word and gives its index; the generator stops when out of
// memory.
size_t CollationPush(CollationWords* w, uint64_t value);

// A special CE32 of a tag, an index and a length.
uint32_t CollationSpecial(uint32_t tag, size_t index, uint32_t length);

// The CE32 of a sequence of elements, its expansion appended to ces or
// ce32s.
uint32_t CollationEncode(CollationWords* ces, CollationWords* ce32s, const uint64_t* elements,
                         uint32_t count);

#endif
