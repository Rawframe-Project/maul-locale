// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Builds a code point trie (src/code_point_trie.h, record mloc-0063) from a value for each
// code point: the high start is the lowest multiple of 4,096 from which
// every code point has U+10FFFF's value, and equal data blocks and equal
// index blocks are stored once.

#ifndef MLOCGEN_TRIE_GEN_H
#define MLOCGEN_TRIE_GEN_H

#include <stddef.h>
#include <stdint.h>

enum
{
    trieCodePoints = 0x110000,
};

// The trie of values[0..0x10FFFF]; the caller frees it.
uint8_t* TrieBuild(const uint32_t* values, size_t* sizeOut);

#endif
