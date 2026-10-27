// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Sort keys, byte for byte as ICU4C's
// CollationKeys::writeSortKeyUpToQuaternary and
// RuleBasedCollator::writeIdenticalLevel write them: the primaries, with a
// compressible lead byte written once for a run; then each further level
// after a 01 byte, its common weights' runs compressed; the case level in
// nibbles; the identical level as BOCSU over the NFD code points; a final
// 00. Nothing is buffered: each level reads the text again and writes into
// the caller's bytes, and a backwards segment is reversed where it was
// written.

#ifndef MLOC_SRC_COLLATION_KEY_H
#define MLOC_SRC_COLLATION_KEY_H

#include "collation_compare.h"
#include "invariant.h"
#include "nfd_stream.h"

#include <stddef.h>
#include <stdint.h>

// The caller's bytes for a key; length counts past the capacity, in 64
// bits: a text's key can be 14.5 times its length, past a 32-bit size_t.
typedef struct CollationSink
{
    uint8_t* out;
    size_t capacity;
    uint64_t length;
} CollationSink;

static inline void mlocCollationSinkPut(CollationSink* s, uint32_t byte)
{
    // Each caller masks or bounds its weight to a byte.
    MLOC_ASSERT(byte <= 0xFF);
    if (s->length < s->capacity)
    {
        s->out[s->length] = (uint8_t)byte;
    }
    s->length++;
}

// The key's length, SIZE_MAX when it passes SIZE_MAX.
static inline size_t mlocCollationSinkLength(const CollationSink* s)
{
    return s->length > SIZE_MAX ? SIZE_MAX : (size_t)s->length;
}

// Writes the key of a text into out, at most capacity bytes, and returns
// its length with the final 00. When that is more than capacity, the
// bytes written are not a key.
size_t mlocCollationSortKeyText(const CollationRoot* root, const CollationSettings* settings,
                                NfdText text, uint8_t* out, size_t capacity);

static inline size_t mlocCollationSortKey(const CollationRoot* root,
                                          const CollationSettings* settings, const uint8_t* text,
                                          size_t length, uint8_t* out, size_t capacity)
{
    return mlocCollationSortKeyText(root, settings, mlocNfdUtf8(text, length), out, capacity);
}

#endif
