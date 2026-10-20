// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The element iterator on a small root built by the generator: one
// element, expansions, an offset, the implicit word, U+FFFD for bad
// bytes and U+FFFF, Hangul through its jamo, a prefix, contractions
// contiguous and discontiguous with their blocked marks, and the
// lookahead's bound; each element's source range.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_iterator.h"
#include "collation_root.h"
#include "collation_unit.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char s_root[] = "[UCA version = 18.0.1]\n"
                             "[Unified_Ideograph 4E00..4E01]\n"
                             "0061; [2B, 05, 05]\n"
                             "0062; [2C, 05, 05]\n"
                             "0063; [2D, 05, 05]\n"
                             "0068; [2E, 05, 05]\n"
                             "0063 0068; [2F, 05, 05]\n"
                             "0063 004C; [31, 05, 05]\n"
                             "004C; [30, 05, 05]\n"
                             "00B7; [, 8C, 05]\n"
                             "004C | 00B7; [, FB B6, 05]\n"
                             "0300; [, 8A, 05]\n"
                             "0301; [, 88, 05]\n"
                             "0323; [, 90, 05]\n"
                             "0323 0301; [, 92, 05]\n"
                             "0061 0300; [2B 80, 05, 05]\n"
                             "1100; [3A, 05, 05]\n"
                             "1161; [3B, 05, 05]\n"
                             "3220; [09 7C, 05, 17][U+4E00, 10][09 7E, 05, 17]\n";

static CollationRoot s_unit;
static uint8_t* s_bytes;

static bool Build(void)
{
    const char* path = "collation-iterator.txt";
    FILE* f = fopen(path, "wb");
    if (f == nullptr)
    {
        return false;
    }
    fputs(s_root, f);
    fclose(f);
    RootTable t;
    char error[128] = "";
    size_t size = 0;
    bool ok = RootRead(path, &t, error, sizeof(error));
    remove(path);
    s_bytes = ok ? CollationRootUnit(&t, &size, error, sizeof(error)) : nullptr;
    if (ok)
    {
        RootFree(&t);
    }
    return s_bytes != nullptr &&
           mlocReadCollationRoot((DataSpan){s_bytes, (uint32_t)size}, &s_unit);
}

// Whether the text's elements are the expected ones.
static bool Gives(const char* text, size_t length, const uint64_t* want, uint32_t count)
{
    CollationIterator it;
    mlocCollationIteratorStart(&it, &s_unit, (const uint8_t*)text, length);
    uint64_t e = 0;
    uint32_t n = 0;
    bool same = true;
    while (mlocCollationNextElement(&it, &e))
    {
        same = same && n < count && e == want[n];
        n++;
    }
    return same && n == count;
}

#define GIVES(text, ...)                                                                           \
    Gives(text, sizeof(text) - 1, (const uint64_t[]){__VA_ARGS__},                                 \
          sizeof((const uint64_t[]){__VA_ARGS__}) / sizeof(uint64_t))

constexpr uint64_t a = 0x2B00000005000500ULL;
constexpr uint64_t b = 0x2C00000005000500ULL;
constexpr uint64_t c = 0x2D00000005000500ULL;
constexpr uint64_t h = 0x2E00000005000500ULL;
constexpr uint64_t ch = 0x2F00000005000500ULL;
constexpr uint64_t cL = 0x3100000005000500ULL;
constexpr uint64_t dotBelowAcute = 0x0000000092000500ULL;
constexpr uint64_t capitalL = 0x3000000005000500ULL;
constexpr uint64_t dot = 0x000000008C000500ULL;
constexpr uint64_t dotAfterL = 0x00000000FBB60500ULL;
constexpr uint64_t grave = 0x000000008A000500ULL;
constexpr uint64_t acute = 0x0000000088000500ULL;
constexpr uint64_t dotBelow = 0x0000000090000500ULL;
constexpr uint64_t aGrave = 0x2B80000005000500ULL;
constexpr uint64_t jamoL = 0x3A00000005000500ULL;
constexpr uint64_t jamoV = 0x3B00000005000500ULL;
constexpr uint64_t common = 0x05000500ULL;

static void TestElements(void)
{
    CHECK(GIVES("ab", a, b) && Gives("", 0, nullptr, 0),
          "one element a code point, none for the empty text");
    CollationIterator it;
    mlocCollationIteratorStart(&it, &s_unit, (const uint8_t*)"\xE4\xB8\x81", 3);
    uint64_t e = 0;
    bool second = mlocCollationNextElement(&it, &e);
    uint32_t primary = (uint32_t)(e >> 32);
    mlocCollationIteratorStart(&it, &s_unit, (const uint8_t*)"\xE4\xB8\x80", 3);
    uint64_t first = 0;
    CHECK(second && mlocCollationNextElement(&it, &first) &&
              primary == mlocAddToPrimary((uint32_t)(first >> 32), false, 2) &&
              (e & 0xFFFFFFFF) == common,
          "Han's second code point two steps past its first through the offset");
    CHECK(GIVES("\xCD\xB8", (uint64_t)mlocCollationUnassignedPrimary(0x378) << 32 | common),
          "an unassigned code point's implicit primary");
    CHECK(GIVES("\xFF", 0xFFFD000000000000ULL | common) &&
              GIVES("\xEF\xBF\xBD", 0xFFFD000000000000ULL | common) &&
              GIVES("\xEF\xBF\xBF", 0xFFFF000000000000ULL | common) &&
              GIVES("a\xC3", a, 0xFFFD000000000000ULL | common),
          "bad bytes as U+FFFD, and U+FFFD and U+FFFF with their fixed primaries");
    CHECK(GIVES("\xEA\xB0\x80", jamoL, jamoV), "a Hangul syllable through its jamo");
    CollationIterator paren;
    mlocCollationIteratorStart(&paren, &s_unit, (const uint8_t*)"\xE3\x88\xA0", 3);
    uint32_t n = 0;
    while (mlocCollationNextElement(&paren, &e))
    {
        n++;
    }
    CHECK(n == 3, "an expansion of three elements, one not a CE32");
}

static void TestContexts(void)
{
    CHECK(GIVES("ch", ch) && GIVES("c", c) && GIVES("cb", c, b) && GIVES("chh", ch, h),
          "a contraction, its default alone or before another code point");
    CHECK(GIVES("L\xC2\xB7", capitalL, dotAfterL) && GIVES("\xC2\xB7", dot) &&
              GIVES("aL\xC2\xB7", a, capitalL, dotAfterL) && GIVES("a\xC2\xB7", a, dot),
          "a prefix matched on the code point before, the default otherwise");
    CHECK(GIVES("a\xCC\x80", aGrave) && GIVES("\xC3\xA0", aGrave) && GIVES("a", a),
          "a contraction with a mark, through NFD too");
    CHECK(GIVES("a\xCC\xA3\xCC\x80", aGrave, dotBelow) &&
              GIVES("a\xCC\x80\xCC\xA3", aGrave, dotBelow),
          "a discontiguous contraction past a mark of a lower class, in either order");
    CHECK(GIVES("a\xCC\x81\xCC\x80", a, acute, grave),
          "a mark of the same class as one skipped is blocked");
    CHECK(GIVES("ab\xCC\x80", a, b, grave), "a starter ends a contraction");
    CHECK(GIVES("cL\xC2\xB7", cL, dotAfterL),
          "a prefix matched on the last code point of a contraction before");
    CHECK(GIVES("a\xCC\xA3\xCC\x80"
                "b\xCC\x81",
                aGrave, dotBelow, b, acute) &&
              GIVES("\xCC\xA3\xCC\x81", dotBelowAcute),
          "a mark left by a discontiguous match keeps its class: no match past the starter "
          "after it");
}

static void TestLookahead(void)
{
    // Within the lookahead, a grave after 70 dots below joins the a;
    // after 80 it is too far.
    static char text[512];
    size_t n = 0;
    text[n++] = 'a';
    for (int i = 0; i < 70; i++)
    {
        text[n++] = (char)0xCC;
        text[n++] = (char)0xA3;
    }
    text[n++] = (char)0xCC;
    text[n++] = (char)0x80;
    CollationIterator it;
    mlocCollationIteratorStart(&it, &s_unit, (const uint8_t*)text, n);
    uint64_t e = 0;
    bool first = mlocCollationNextElement(&it, &e) && e == aGrave;
    uint32_t dots = 0;
    while (mlocCollationNextElement(&it, &e))
    {
        dots += e == dotBelow;
    }
    n = 1;
    for (int i = 0; i < 80; i++)
    {
        text[n++] = (char)0xCC;
        text[n++] = (char)0xA3;
    }
    text[n++] = (char)0xCC;
    text[n++] = (char)0x80;
    mlocCollationIteratorStart(&it, &s_unit, (const uint8_t*)text, n);
    bool far = mlocCollationNextElement(&it, &e) && e == a;
    CHECK(first && dots == 70 && far, "a discontiguous match within the lookahead, none past it");
}

// Whether the text's elements cover the expected source ranges, two
// numbers (low, high) per element.
static bool Spans(NfdText text, const uint32_t* want, uint32_t count)
{
    CollationIterator it;
    mlocCollationIteratorStartText(&it, &s_unit, text);
    uint64_t e = 0;
    uint32_t low = 0;
    uint32_t high = 0;
    uint32_t n = 0;
    bool same = true;
    while (mlocCollationNextElementAt(&it, &e, &low, &high))
    {
        same = same && n + 1 < count && low == want[n] && high == want[n + 1];
        n += 2;
    }
    return same && n == count;
}

#define SPANS(text, ...)                                                                           \
    Spans(mlocNfdUtf8((const uint8_t*)(text), sizeof(text) - 1), (const uint32_t[]){__VA_ARGS__},  \
          sizeof((const uint32_t[]){__VA_ARGS__}) / sizeof(uint32_t))

static void TestSpans(void)
{
    CHECK(SPANS("abc", 0, 1, 1, 2, 2, 3), "each code point its own range");
    CHECK(SPANS("b\xCC\x81", 0, 1, 1, 3), "a segment NFD leaves alone keeps its code points");
    CHECK(SPANS("\xC3\xA0", 0, 2) && SPANS("\xC3\xA1", 0, 2, 2, 2) &&
              SPANS("b\xC3\xA0", 0, 1, 1, 3),
          "a precomposed letter is one range");
    CHECK(SPANS("\xE3\x88\xA0", 0, 3, 3, 3, 3, 3),
          "an expansion's first element spans the code point, the rest are empty at its end");
    CHECK(SPANS("a\xCC\x81\xCC\xA3"
                "b",
                0, 5, 5, 5, 5, 6),
          "a reordered segment: its first element covers it, the rest are empty at its end");
    CHECK(SPANS("ch", 0, 2) && SPANS("cb", 0, 1, 1, 2), "a contraction spans its code points");
    static const uint16_t units[] = {'a', 0xD83D, 0xDE00, 'b'};
    CHECK(Spans(mlocNfdUtf16(units, 4), (const uint32_t[]){0, 1, 1, 3, 3, 4}, 6),
          "UTF-16 ranges count code units");
    CHECK(Spans(mlocNfdUtf8((const uint8_t*)"", 0), nullptr, 0), "an empty text has no ranges");
}

int main(void)
{
    bool built = Build();
    CHECK(built, "the small root builds into a unit that reads");
    if (built)
    {
        TestElements();
        TestContexts();
        TestLookahead();
        TestSpans();
    }
    free(s_bytes);
    return s_failures == 0 ? 0 : 1;
}
