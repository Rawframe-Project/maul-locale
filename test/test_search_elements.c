// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Search's processed elements (record mloc-0074) on a small root, as
// ICU4C's UCollationPCE makes them: an element in one half or two (a
// three-byte primary's continuation), each level kept to the strength,
// halves that reduce to 0 passed over, shifted halves below the variable
// top keeping only their quaternary and the ignorables after them
// dropped (a continuation half compared alone, as ICU does), the ranges
// of both halves and the end; and the same elements walking back, as
// UCollationPCE::previousProcessed gives them.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_root.h"
#include "collation_unit.h"
#include "search_back.h"
#include "search_elements.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char s_file[] = "[UCA version = 18.0.1]\n"
                             "[Unified_Ideograph 4E00..4E01]\n"
                             "0020; [05 10, 05, 05]\n"
                             "0061; [2B, 05, 05]\n"
                             "0062; [2C 10 20, 05, 05]\n"
                             "0063; [30 02 04, 05, 05]\n"
                             "0041; [2B, 05, 8F]\n"
                             "0301; [, 88, 05]\n"
                             "0009; [, 06, 05]\n";

static CollationRoot s_unit;
static uint8_t* s_bytes;

static bool Build(void)
{
    const char* path = "search-elements.txt";
    FILE* f = fopen(path, "wb");
    if (f == nullptr)
    {
        return false;
    }
    fputs(s_file, f);
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

// Whether a text's processed elements are the expected ones, three
// numbers each (the element, its low and high), then the end.
static bool Gives(uint32_t strength, bool shifted, NfdText text, const uint64_t* want,
                  uint32_t count)
{
    CollationSettings settings = {
        .strength = strength, .shifted = shifted, .variableTop = 0x0600FFFF};
    SearchElements e;
    mlocSearchElementsStart(&e, &s_unit, &settings, text);
    uint32_t n = 0;
    uint32_t low = 0;
    uint32_t high = 0;
    bool same = true;
    for (uint64_t p = mlocSearchNextElement(&e, &low, &high); p != searchEnd;
         p = mlocSearchNextElement(&e, &low, &high))
    {
        same = same && n + 2 < count && p == want[n] && low == want[n + 1] && high == want[n + 2];
        n += 3;
    }
    return same && n == count && low == text.length && high == text.length &&
           mlocSearchNextElement(&e, &low, &high) == searchEnd;
}

#define GIVES(strength, shifted, text, ...)                                                        \
    Gives(strength, shifted, mlocNfdUtf8((const uint8_t*)(text), sizeof(text) - 1),                \
          (const uint64_t[]){__VA_ARGS__},                                                         \
          sizeof((const uint64_t[]){__VA_ARGS__}) / sizeof(uint64_t))

constexpr uint64_t a3 = 0x2B00000500050000ULL;
constexpr uint64_t b3 = 0x2C10000500050000ULL;
constexpr uint64_t bRest3 = 0x2000000000C00000ULL;

static void TestHalves(void)
{
    CHECK(GIVES(collationTertiary, false, "ab", a3, 0, 1, b3, 1, 2, bRest3, 2, 2),
          "a three-byte primary in two halves, the continuation empty at the element's end");
    CHECK(GIVES(collationPrimary, false, "ab", 0x2B00ULL << 48, 0, 1, 0x2C10ULL << 48, 1, 2,
                0x2000ULL << 48, 2, 2),
          "the primary strength keeps the primaries of both halves");
    CHECK(GIVES(collationSecondary, false, "aA", 0x2B00000500000000ULL, 0, 1, 0x2B00000500000000ULL,
                1, 2) &&
              GIVES(collationTertiary, false, "A", 0x2B000005008F0000ULL, 0, 1),
          "the secondary strength drops the tertiary, case bits and all");
    CHECK(GIVES(collationPrimary, false, "a\xCC\x81", 0x2B00ULL << 48, 0, 1) &&
              GIVES(collationSecondary, false, "a\xCC\x81", 0x2B00000500000000ULL, 0, 1,
                    0x0000008800000000ULL, 1, 3),
          "a half that reduces to 0 at the strength is passed over");
    static const uint16_t units[] = {'a', 'b'};
    static const uint64_t want[] = {a3, 0, 1, b3, 1, 2, bRest3, 2, 2};
    CHECK(Gives(collationTertiary, false, mlocNfdUtf16(units, 2), want, 9) &&
              Gives(collationTertiary, false, mlocNfdUtf8(nullptr, 0), nullptr, 0),
          "UTF-16 ranges in units; an empty text gives only the end");
}

static SearchBack s_back;

// Whether the elements walking back from a position are the expected
// ones, three numbers each, last first, then the start.
static bool Backs(uint32_t strength, bool shifted, NfdText text, size_t from, const uint64_t* want,
                  uint32_t count)
{
    CollationSettings settings = {
        .strength = strength, .shifted = shifted, .variableTop = 0x0600FFFF};
    mlocSearchBackStart(&s_back, &s_unit, &settings, text, from);
    uint32_t n = 0;
    uint32_t low = 1;
    uint32_t high = 1;
    bool same = true;
    for (uint64_t p = mlocSearchPreviousElement(&s_back, &low, &high); p != searchEnd;
         p = mlocSearchPreviousElement(&s_back, &low, &high))
    {
        same = same && n + 2 < count && p == want[n] && low == want[n + 1] && high == want[n + 2];
        n += 3;
    }
    return same && n == count && low == 0 && high == 0 && !s_back.full &&
           mlocSearchPreviousElement(&s_back, &low, &high) == searchEnd;
}

#define BACKS(strength, shifted, text, from, ...)                                                  \
    Backs(strength, shifted, mlocNfdUtf8((const uint8_t*)(text), sizeof(text) - 1), from,          \
          (const uint64_t[]){__VA_ARGS__},                                                         \
          sizeof((const uint64_t[]){__VA_ARGS__}) / sizeof(uint64_t))

// Whether walking back over a text sets full.
static bool Fills(NfdText text)
{
    CollationSettings settings = {.strength = collationTertiary};
    mlocSearchBackStart(&s_back, &s_unit, &settings, text, text.length);
    uint32_t low = 0;
    uint32_t high = 0;
    while (mlocSearchPreviousElement(&s_back, &low, &high) != searchEnd)
    {
    }
    return s_back.full;
}

static void TestBackward(void)
{
    CHECK(BACKS(collationTertiary, false, "ab", 2, bRest3, 2, 2, b3, 1, 2, a3, 0, 1) &&
              BACKS(collationSecondary, false, "a\xCC\x81", 3, 0x0000008800000000ULL, 1, 3,
                    0x2B00000500000000ULL, 0, 1),
          "walking back gives the forward elements last first, a continuation before its half");
    CHECK(BACKS(collationTertiary, false, "abc", 2, bRest3, 2, 2, b3, 1, 2, a3, 0, 1),
          "a walk from inside the text reads nothing after its start");
    static const uint16_t units[] = {'a', 'b'};
    static const uint64_t want[] = {bRest3, 2, 2, b3, 1, 2, a3, 0, 1};
    CHECK(Backs(collationTertiary, false, mlocNfdUtf16(units, 2), 2, want, 9) &&
              Backs(collationTertiary, false, mlocNfdUtf8(nullptr, 0), 0, nullptr, 0),
          "UTF-16 ranges in units; a walk from the start gives only the start");
    // Forward the mark at the start follows nothing shifted and is kept;
    // walking back, the space's chunk comes first and leaves its state
    // on, so the mark is dropped, as ICU4C 79 drops it.
    CHECK(GIVES(collationTertiary, true,
                "\xCC\x81"
                " a",
                0x0000008800050000ULL, 0, 2, a3, 3, 4) &&
              BACKS(collationTertiary, true,
                    "\xCC\x81"
                    " a",
                    4, a3, 3, 4),
          "the shifted state goes back from a chunk to the one before it");
}

static void TestBackwardCapacity(void)
{
    static uint8_t text[1 + 2 * searchBackHalves];
    text[0] = 'a';
    for (size_t i = 0; i < searchBackHalves; i++)
    {
        text[1 + 2 * i] = 0xCC;
        text[2 + 2 * i] = 0x81;
    }
    static uint8_t tabs[searchBackHalves + 1];
    memset(tabs, '\t', sizeof tabs);
    CHECK(Fills(mlocNfdUtf8(text, sizeof text)) && !Fills(mlocNfdUtf8(text, sizeof text - 2)) &&
              Fills(mlocNfdUtf8(tabs, sizeof tabs)) && !Fills(mlocNfdUtf8(tabs, sizeof tabs - 1)),
          "a piece of more halves than the walk holds (marks after a letter), or a chunk of "
          "more without a primary (tabs), sets full");
}

static void TestShifted(void)
{
    CHECK(GIVES(collationTertiary, true,
                " \xCC\x81"
                "a",
                a3, 3, 4),
          "below the quaternary strength a shifted half and the mark after it are dropped");
    CHECK(GIVES(collationQuaternary, true,
                " \xCC\x81"
                "a",
                0x0510ULL, 0, 1, a3 | 0xFFFF, 3, 4),
          "at the quaternary strength a shifted half keeps its primary as the quaternary");
    CHECK(GIVES(collationQuaternary, false, " a", 0x0510000500050000ULL | 0xFFFF, 0, 1, a3 | 0xFFFF,
                1, 2),
          "non-ignorable: every half's quaternary is FFFF");
    CHECK(GIVES(collationQuaternary, true, "c\xCC\x81", 0x3002000500050000ULL | 0xFFFF, 0, 1,
                0x0400ULL, 1, 1),
          "a continuation half below the variable top is shifted, compared alone as ICU4C "
          "does, and the mark after it dropped");
    CHECK(GIVES(collationIdentical, true, "a", a3 | 0xFFFF, 0, 1),
          "the identical strength processes as the quaternary");
}

int main(void)
{
    bool built = Build();
    CHECK(built, "the small root builds into a unit that reads");
    if (built)
    {
        TestHalves();
        TestShifted();
        TestBackward();
        TestBackwardCapacity();
    }
    free(s_bytes);
    return s_failures == 0 ? 0 : 1;
}
