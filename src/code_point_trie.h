// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A code point trie of 32-bit values (record mloc-0063), a part of a unit
// rather than a unit of its own: collation's elements are its first use.
// The layout follows ICU's fast code point trie in spirit, not in its
// bytes: the BMP takes two reads, a supplementary code point three, and
// every code point from the high start takes one value. Data blocks of
// 64 values are shared wherever they are equal. The reader is inline,
// so that the generator's tests read what the generator writes. All
// numbers are little-endian.
//
//   0  u32 highStart: a multiple of 4096 from 0x10000 to 0x110000
//   4  u32 highValue, the value of every code point from highStart
//   8  u32 blockCount, the index blocks below
//  12  u32 dataLength, in values, at least 64
//  16  u32 bmp[1024]: for each 64 code points of the BMP, the offset of
//      its data block in values
//      u16 supplementary[(highStart - 0x10000) >> 12]: for each 4,096
//      code points, an index block's number; then zero bytes to a
//      multiple of 4
//      u32 blocks[blockCount][64]: for each 64 code points, the offset of
//      its data block in values
//      u32 data[dataLength]

#ifndef MLOC_CODE_POINT_TRIE_H
#define MLOC_CODE_POINT_TRIE_H

#include "bytes.h"
#include "data_view.h"

#include <stdint.h>

enum
{
    trieHead = 16,
    trieBmpBlocks = 1024,
    trieBlockLength = 64,
    trieSupplementaryShift = 12,
    trieMaxCodePoint = 0x10FFFF,
};

typedef struct CodePointTrie
{
    const uint8_t* bmp;
    const uint8_t* supplementary;
    const uint8_t* blocks;
    const uint8_t* data;
    uint32_t highStart;
    uint32_t highValue;
    uint32_t blockCount;
    uint32_t dataLength;
    // The trie's bytes, so that what follows it in a unit starts there.
    uint32_t size;
} CodePointTrie;

// The bytes of the supplementary index for a high start, padded to 4.
static inline uint32_t mlocTrieSupplementarySize(uint32_t highStart)
{
    uint32_t count = (highStart - 0x10000) >> trieSupplementaryShift;
    return (count * 2 + 3) / 4 * 4;
}

// Whether a data block offset leaves a whole block inside the data.
static inline bool mlocTrieBlockFits(uint32_t offset, uint32_t dataLength)
{
    return offset <= dataLength - trieBlockLength;
}

// Reads a trie at the start of a span and checks every offset in it;
// false for one that does not fit the span or points outside its data.
static inline bool mlocReadCodePointTrie(DataSpan span, CodePointTrie* out)
{
    if (span.size < trieHead)
    {
        return false;
    }
    const uint8_t* p = span.bytes;
    uint32_t highStart = mlocLoad32(p);
    uint32_t blockCount = mlocLoad32(p + 8);
    uint32_t dataLength = mlocLoad32(p + 12);
    if (highStart < 0x10000 || highStart > 0x110000 || (highStart & 0xFFF) != 0 ||
        dataLength < trieBlockLength || blockCount > 0x10000 || dataLength > 0x4000000)
    {
        return false;
    }
    uint64_t supplementary = trieHead + (uint64_t)trieBmpBlocks * 4;
    uint64_t blocks = supplementary + mlocTrieSupplementarySize(highStart);
    uint64_t data = blocks + (uint64_t)blockCount * trieBlockLength * 4;
    uint64_t size = data + (uint64_t)dataLength * 4;
    if (size > span.size)
    {
        return false;
    }
    *out = (CodePointTrie){
        .bmp = p + trieHead,
        .supplementary = p + supplementary,
        .blocks = p + blocks,
        .data = p + data,
        .highStart = highStart,
        .highValue = mlocLoad32(p + 4),
        .blockCount = blockCount,
        .dataLength = dataLength,
        .size = (uint32_t)size,
    };
    bool ok = true;
    for (uint32_t i = 0; ok && i < trieBmpBlocks; i++)
    {
        ok = mlocTrieBlockFits(mlocLoad32(out->bmp + i * 4), dataLength);
    }
    uint32_t sections = (highStart - 0x10000) >> trieSupplementaryShift;
    for (uint32_t i = 0; ok && i < sections; i++)
    {
        ok = mlocLoad16(out->supplementary + i * 2) < blockCount;
    }
    for (uint32_t i = 0; ok && i < blockCount * trieBlockLength; i++)
    {
        ok = mlocTrieBlockFits(mlocLoad32(out->blocks + i * 4), dataLength);
    }
    return ok;
}

// A code point's value; codePoint must be at most U+10FFFF.
static inline uint32_t mlocTrieValue(const CodePointTrie* t, uint32_t codePoint)
{
    uint32_t block = 0;
    if (codePoint < 0x10000)
    {
        block = mlocLoad32(t->bmp + (codePoint >> 6) * 4);
    }
    else if (codePoint < t->highStart)
    {
        uint32_t section =
            mlocLoad16(t->supplementary + ((codePoint - 0x10000) >> trieSupplementaryShift) * 2);
        block = mlocLoad32(t->blocks +
                           ((size_t)section * trieBlockLength + ((codePoint >> 6) & 63)) * 4);
    }
    else
    {
        return t->highValue;
    }
    return mlocLoad32(t->data + ((size_t)block + (codePoint & 63)) * 4);
}

#endif
