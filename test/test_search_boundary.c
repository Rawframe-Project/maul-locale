// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Search's grapheme cluster boundaries at any position: for random texts
// of the code points the rules read back through (regional indicators,
// emoji ZWJ sequences, conjuncts with their linkers, Hangul jamo and
// syllables, prepends, spacing marks, CR and LF, controls) and ill-formed
// bytes, every code point boundary is a boundary exactly when Maul
// Unicode's iterator over the whole text says so, and the following
// boundary is the next it gives; the same over each text's UTF-16 form, in
// units.

#include "search_boundary.h"
#include "test_harness.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/segment.h"

#include <stdint.h>
#include <string.h>

enum
{
    maxText = 256,
};

static const uint32_t s_pool[] = {
    'a',     'b',     ' ',     0x0D,    0x0A,    0x01,    0x7F,    0x0300, 0x0301,  0x200D,
    0x200C,  0x1F1E6, 0x1F1E8, 0x1F1FA, 0x1F468, 0x1F469, 0x2764,  0xFE0F, 0x1F3FB, 0x0915,
    0x0937,  0x094D,  0x093F,  0x0940,  0x0924,  0x1100,  0x1161,  0x11A8, 0xAC00,  0xAC01,
    0x0600,  0x0605,  0x0E33,  0x0E40,  0x00AD,  0x2060,  0x1F600, 0x0903, 0x0BCD,  0xFFFD,
    0xE0061, 0x1F3F4, 0x0A3C,  0x0D4D,  0x110BD, 0x0CF3,  0x16D63, 0x05D0, 0x0645,  0x0E01,
};

static uint32_t s_seed = 2026;

static uint32_t Random(void)
{
    s_seed = s_seed * 1103515245u + 12345u;
    return s_seed >> 8;
}

// A random text: code points from the pool and now and then an
// ill-formed byte; each code point boundary's byte offset marked.
static size_t Text(uint8_t* bytes, bool* starts, bool* wellFormed)
{
    size_t n = 0;
    uint32_t count = Random() % 24;
    *wellFormed = true;
    for (uint32_t i = 0; i < count; i++)
    {
        if (Random() % 16 == 0)
        {
            static const uint8_t bad[] = {0x80, 0xC3, 0xE2, 0xF4, 0xFF};
            bytes[n++] = bad[Random() % sizeof(bad)];
            *wellFormed = false;
            continue;
        }
        size_t size = 0;
        (void)muniEncodeUtf8(s_pool[Random() % (sizeof(s_pool) / sizeof(s_pool[0]))],
                             (char*)bytes + n, &size);
        n += size;
    }
    // The code point boundaries as decoding finds them: an ill-formed
    // byte may join the sequence before it.
    memset(starts, 0, n + 1);
    for (size_t i = 0; i < n;)
    {
        uint32_t c = 0;
        size_t size = 1;
        (void)muniDecodeUtf8((const char*)bytes + i, n - i, &c, &size);
        starts[i] = true;
        i += size;
    }
    starts[n] = true;
    return n;
}

// The boundaries of the whole text, as flags by byte.
static void Whole(const uint8_t* bytes, size_t n, bool* boundary)
{
    memset(boundary, 0, n + 1);
    boundary[0] = true;
    muniSegmentIterator g;
    (void)muniInitGraphemeIterator(&g, (const char*)bytes, n, false);
    size_t offset = 0;
    while (muniNextSegmentBreak(&g, &offset) == muni_success)
    {
        boundary[offset] = true;
    }
}

// The next boundary after a byte, from the flags.
static size_t Next(const bool* boundary, size_t n, size_t at)
{
    size_t b = at + 1;
    while (b < n && !boundary[b])
    {
        b++;
    }
    return b < n ? b : n;
}

// The UTF-16 form of a well-formed text and each byte offset's unit.
static size_t Units(const uint8_t* bytes, size_t n, uint16_t* units, size_t* unitOf)
{
    size_t u = 0;
    for (size_t i = 0; i < n;)
    {
        uint32_t c = 0;
        size_t size = 0;
        (void)muniDecodeUtf8((const char*)bytes + i, n - i, &c, &size);
        unitOf[i] = u;
        if (c >= 0x10000)
        {
            units[u++] = (uint16_t)(0xD7C0 + (c >> 10));
            units[u++] = (uint16_t)(0xDC00 | (c & 0x3FF));
        }
        else
        {
            units[u++] = (uint16_t)c;
        }
        i += size;
    }
    unitOf[n] = u;
    return u;
}

static void TestRandom(void)
{
    static uint8_t bytes[maxText];
    static bool starts[maxText];
    static bool boundary[maxText];
    static uint16_t units[maxText];
    static size_t unitOf[maxText];
    bool same = true;
    bool same16 = true;
    for (int round = 0; round < 20000; round++)
    {
        bool wellFormed = true;
        size_t n = Text(bytes, starts, &wellFormed);
        Whole(bytes, n, boundary);
        NfdText t = mlocNfdUtf8(bytes, n);
        size_t u = wellFormed ? Units(bytes, n, units, unitOf) : 0;
        NfdText t16 = mlocNfdUtf16(units, u);
        for (size_t at = 0; at <= n; at++)
        {
            if (!starts[at])
            {
                continue;
            }
            size_t next = Next(boundary, n, at);
            same = same && mlocSearchIsBoundary(t, at) == boundary[at] &&
                   mlocSearchFollowing(t, at) == next;
            if (wellFormed)
            {
                same16 = same16 && mlocSearchIsBoundary(t16, unitOf[at]) == boundary[at] &&
                         mlocSearchFollowing(t16, unitOf[at]) == unitOf[next];
            }
        }
    }
    CHECK(same, "UTF-8: every code point boundary as the whole text's iterator says");
    CHECK(same16, "UTF-16: the same, in units");
}

static void TestEdges(void)
{
    static const uint8_t flags[] = {0xF0, 0x9F, 0x87, 0xBA, 0xF0, 0x9F, 0x87, 0xB8,
                                    0xF0, 0x9F, 0x87, 0xBA, 0xF0, 0x9F, 0x87, 0xB8};
    NfdText t = mlocNfdUtf8(flags, sizeof(flags));
    CHECK(mlocSearchIsBoundary(t, 8) && !mlocSearchIsBoundary(t, 4) &&
              !mlocSearchIsBoundary(t, 12) && mlocSearchFollowing(t, 4) == 8 &&
              mlocSearchFollowing(t, 8) == 16,
          "regional indicators pair from the text's start, however far back");
    NfdText empty = mlocNfdUtf8(nullptr, 0);
    CHECK(mlocSearchIsBoundary(empty, 0) && mlocSearchFollowing(empty, 0) == 0 &&
              mlocSearchIsBoundary(t, 0) && mlocSearchIsBoundary(t, 16) &&
              mlocSearchFollowing(t, 16) == 16,
          "the start and the end are boundaries; none follows the end");
    // A long cluster in UTF-16, past one piece of UTF-8.
    static uint16_t marks[1 + 300];
    marks[0] = 'a';
    for (int i = 1; i <= 300; i++)
    {
        marks[i] = 0x0301;
    }
    NfdText m = mlocNfdUtf16(marks, 301);
    CHECK(!mlocSearchIsBoundary(m, 150) && mlocSearchFollowing(m, 0) == 301 &&
              mlocSearchFollowing(m, 299) == 301,
          "a cluster longer than a piece of the UTF-16 text's UTF-8 form");
    // Pieces filled to their last byte: four-byte code points, one
    // cluster (an emoji and variation selectors) and 64 clusters (Han).
    static uint16_t full[2 * 301];
    static uint16_t han[2 * 64];
    full[0] = 0xD83D;
    full[1] = 0xDE00;
    for (int i = 1; i <= 300; i++)
    {
        full[2 * i] = 0xDB40;
        full[2 * i + 1] = 0xDD00;
    }
    for (int i = 0; i < 64; i++)
    {
        han[2 * i] = 0xD840;
        han[2 * i + 1] = 0xDC00;
    }
    NfdText f = mlocNfdUtf16(full, 602);
    NfdText h = mlocNfdUtf16(han, 128);
    bool each = true;
    for (size_t at = 0; at < 128; at += 2)
    {
        each = each && mlocSearchIsBoundary(h, at) && mlocSearchFollowing(h, at) == at + 2;
    }
    CHECK(mlocSearchFollowing(f, 0) == 602 && !mlocSearchIsBoundary(f, 64) && each,
          "pieces filled to their last byte, a cluster across them and one ending at each");
}

int main(void)
{
    TestRandom();
    TestEdges();
    return s_failures == 0 ? 0 : 1;
}
