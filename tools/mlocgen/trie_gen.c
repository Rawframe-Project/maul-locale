// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Code point tries (trie_gen.h).

#include "trie_gen.h"

#include "code_point_trie.h"
#include "table_gen.h"

#include <stdlib.h>
#include <string.h>

// Blocks of 64 32-bit words stored once each, found by a hash of their
// words in an open-addressed table.
typedef struct Blocks
{
    uint32_t* words;
    uint32_t count;
    uint32_t capacity;
    uint32_t* slots;
    uint32_t slotCount;
} Blocks;

static uint32_t Hash(const uint32_t* block)
{
    uint64_t h = 1469598103934665603ULL;
    for (int i = 0; i < trieBlockLength; i++)
    {
        h = (h ^ block[i]) * 1099511628211ULL;
    }
    return (uint32_t)(h ^ h >> 32);
}

static void BlocksInit(Blocks* b, uint32_t most)
{
    *b = (Blocks){0};
    b->capacity = most;
    b->words = GenAllocate((size_t)most * trieBlockLength * 4);
    b->slotCount = 1;
    while (b->slotCount < most * 2)
    {
        b->slotCount *= 2;
    }
    b->slots = GenAllocate((size_t)b->slotCount * 4);
}

// The index of a block equal to the given one, added when new.
static uint32_t BlocksAdd(Blocks* b, const uint32_t* block)
{
    uint32_t slot = Hash(block) & (b->slotCount - 1);
    for (;; slot = (slot + 1) & (b->slotCount - 1))
    {
        uint32_t held = b->slots[slot];
        if (held == 0)
        {
            break;
        }
        if (memcmp(b->words + (size_t)(held - 1) * trieBlockLength, block, trieBlockLength * 4) ==
            0)
        {
            return held - 1;
        }
    }
    if (b->count == b->capacity)
    {
        GenDie("trie", "more blocks than code points");
    }
    memcpy(b->words + (size_t)b->count * trieBlockLength, block, trieBlockLength * 4);
    b->slots[slot] = ++b->count;
    return b->count - 1;
}

static void BlocksFree(Blocks* b)
{
    free(b->words);
    free(b->slots);
}

static uint32_t HighStart(const uint32_t* values)
{
    uint32_t high = values[trieMaxCodePoint];
    uint32_t start = trieCodePoints;
    while (start > 0x10000 && values[start - 1] == high)
    {
        start--;
    }
    return (start + 0xFFF) & ~(uint32_t)0xFFF;
}

uint8_t* TrieBuild(const uint32_t* values, size_t* sizeOut)
{
    uint32_t highStart = HighStart(values);
    Blocks data;
    Blocks index;
    BlocksInit(&data, highStart / trieBlockLength + 1);
    BlocksInit(&index, ((highStart - 0x10000) >> trieSupplementaryShift) + 1);
    uint32_t bmp[trieBmpBlocks];
    for (uint32_t i = 0; i < trieBmpBlocks; i++)
    {
        bmp[i] = BlocksAdd(&data, values + (size_t)i * trieBlockLength) * trieBlockLength;
    }
    uint32_t sections = (highStart - 0x10000) >> trieSupplementaryShift;
    uint16_t* supplementary = GenAllocate((size_t)sections * 2 + 2);
    for (uint32_t s = 0; s < sections; s++)
    {
        uint32_t offsets[trieBlockLength];
        uint32_t first = 0x10000 + (s << trieSupplementaryShift);
        for (uint32_t i = 0; i < trieBlockLength; i++)
        {
            offsets[i] =
                BlocksAdd(&data, values + first + (size_t)i * trieBlockLength) * trieBlockLength;
        }
        supplementary[s] = (uint16_t)BlocksAdd(&index, offsets);
    }
    uint32_t dataLength = data.count * trieBlockLength;
    size_t size = trieHead + (size_t)trieBmpBlocks * 4 + mlocTrieSupplementarySize(highStart) +
                  (size_t)index.count * trieBlockLength * 4 + (size_t)dataLength * 4;
    uint8_t* out = GenAllocate(size);
    uint8_t* p = out;
    GenPut(p, highStart, 4);
    GenPut(p + 4, values[trieMaxCodePoint], 4);
    GenPut(p + 8, index.count, 4);
    GenPut(p + 12, dataLength, 4);
    p += trieHead;
    for (uint32_t i = 0; i < trieBmpBlocks; i++, p += 4)
    {
        GenPut(p, bmp[i], 4);
    }
    for (uint32_t s = 0; s < sections; s++)
    {
        GenPut(p + s * 2, supplementary[s], 2);
    }
    p += mlocTrieSupplementarySize(highStart);
    for (uint32_t i = 0; i < index.count * trieBlockLength; i++, p += 4)
    {
        GenPut(p, index.words[i], 4);
    }
    for (uint32_t i = 0; i < dataLength; i++, p += 4)
    {
        GenPut(p, data.words[i], 4);
    }
    free(supplementary);
    BlocksFree(&data);
    BlocksFree(&index);
    *sizeOut = size;
    return out;
}
