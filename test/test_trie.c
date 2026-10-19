// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Code point tries (record mloc-0063): the generator's builder and the
// library's reader agree on every code point for maps of different
// shapes, blocks are shared, and the reader refuses a trie whose sizes or
// offsets do not hold.

#include "code_point_trie.h"
#include "test_harness.h"
#include "trie_gen.h"

#include <stdlib.h>
#include <string.h>

static uint32_t* s_values;

// Builds the map's trie and reads every code point back; the trie's size
// through sizeOut.
static bool RoundTrip(size_t* sizeOut, CodePointTrie* trieOut)
{
    size_t size = 0;
    uint8_t* bytes = TrieBuild(s_values, &size);
    CodePointTrie t;
    bool ok = mlocReadCodePointTrie((DataSpan){bytes, (uint32_t)size}, &t) && t.size == size;
    for (uint32_t c = 0; ok && c <= trieMaxCodePoint; c++)
    {
        ok = mlocTrieValue(&t, c) == s_values[c];
    }
    *sizeOut = size;
    *trieOut = t;
    free(bytes);
    return ok;
}

static void Fill(uint32_t value)
{
    for (uint32_t c = 0; c <= trieMaxCodePoint; c++)
    {
        s_values[c] = value;
    }
}

static void TestShapes(void)
{
    size_t size = 0;
    CodePointTrie t;
    Fill(7);
    CHECK(RoundTrip(&size, &t) && t.highStart == 0x10000 && t.highValue == 7 &&
              t.dataLength == trieBlockLength && t.blockCount == 0,
          "one value: the BMP shares one data block, everything above it is the high value");
    Fill(0);
    s_values[0x41] = 1;
    s_values[0xFFFF] = 2;
    s_values[0x10000] = 3;
    s_values[0x1F600] = 4;
    s_values[0x10FFFF] = 5;
    CHECK(RoundTrip(&size, &t) && t.highStart == 0x110000 && t.highValue == 5,
          "single code points at the BMP's ends, the first supplementary one, U+10FFFF");
    Fill(0);
    for (uint32_t c = 0x20000; c < 0x2A6E0; c++)
    {
        s_values[c] = 0xE0000000U | c;
    }
    CHECK(RoundTrip(&size, &t) && t.highStart == 0x2B000,
          "a range of distinct values ending inside a 4,096 section: the high start after it");
    for (uint32_t c = 0; c <= trieMaxCodePoint; c++)
    {
        s_values[c] = (c * 2654435761U) >> 7;
    }
    CHECK(RoundTrip(&size, &t), "a different value for every code point");
    Fill(9);
    for (uint32_t c = 0x3000; c < 0x4000; c++)
    {
        s_values[c] = c & 63;
    }
    for (uint32_t c = 0x10000; c < 0x30000; c++)
    {
        s_values[c] = c & 63;
    }
    CHECK(RoundTrip(&size, &t) && t.dataLength == 2 * trieBlockLength && t.blockCount == 1,
          "repeated blocks stored once: two data blocks, one index block for 32 sections");
}

// A trie of a few values to break.
static uint8_t* Small(size_t* sizeOut)
{
    Fill(1);
    s_values[0x10400] = 2;
    return TrieBuild(s_values, sizeOut);
}

static bool Reads(const uint8_t* bytes, size_t size)
{
    CodePointTrie t;
    return mlocReadCodePointTrie((DataSpan){bytes, (uint32_t)size}, &t);
}

static void Put32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
    {
        p[i] = (uint8_t)(v >> (8 * i));
    }
}

static void TestRefusals(void)
{
    size_t size = 0;
    uint8_t* bytes = Small(&size);
    uint8_t* copy = malloc(size);
    CHECK(bytes != nullptr && copy != nullptr && Reads(bytes, size), "the small trie reads");
    CHECK(!Reads(bytes, size - 1) && !Reads(bytes, trieHead - 1), "a span too short refused");
    static const struct
    {
        size_t at;
        uint32_t value;
    } breaks[] = {
        {0, 0xFFFF}, {0, 0x110001},   {0, 0x10800}, {0, 0x111000},
        {12, 63},    {12, 0x8000000}, {8, 0x10001}, {trieHead, 0x7FFFFFFF},
    };
    for (size_t i = 0; copy != nullptr && i < sizeof(breaks) / sizeof(breaks[0]); i++)
    {
        memcpy(copy, bytes, size);
        Put32(copy + breaks[i].at, breaks[i].value);
        CHECK(!Reads(copy, size), "a high start, data length, block count or BMP offset that "
                                  "does not hold refused");
    }
    CodePointTrie t;
    if (copy != nullptr && mlocReadCodePointTrie((DataSpan){bytes, (uint32_t)size}, &t))
    {
        // The data length less one block, so that the last offset no longer fits.
        memcpy(copy, bytes, size);
        size_t last = (size_t)(t.data - bytes) - 4;
        Put32(copy + last, t.dataLength - trieBlockLength + 1);
        CHECK(!Reads(copy, size), "an index block's offset past the data refused");
        memcpy(copy, bytes, size);
        copy[t.supplementary - bytes] = (uint8_t)t.blockCount;
        CHECK(!Reads(copy, size), "a section naming an index block that is not refused");
    }
    free(copy);
    free(bytes);
}

int main(void)
{
    s_values = malloc((size_t)trieCodePoints * 4);
    if (s_values == nullptr)
    {
        return 1;
    }
    TestShapes();
    TestRefusals();
    free(s_values);
    return s_failures == 0 ? 0 : 1;
}
