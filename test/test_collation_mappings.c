// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder's lookup (record mloc-0071) on a small root: the
// root's elements alone (a contraction, a discontiguous one, a prefix,
// Han, the unassigned, U+FFFD); then tailored mappings over it: one that
// replaces the root's, one whose code point has root contractions
// (copied), contractions and prefixes added, a prefix with contractions
// only falling back to a shorter one, a mapping replaced, ignorable
// elements left out, more elements than an expansion holds, many code
// points, and the mappings refused. conform_collation_mappings checks
// the lookup against the runtime's iterator on the whole root.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_mappings.h"
#include "collation_root.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

static const char* s_path = "collation_mappings.txt";

static const char s_file[] = "[Unified_Ideograph 4E00..4E05]\n"
                             "0061; [2A 10, 05, 05]\n"
                             "0062; [2A 12, 05, 05]\n"
                             "0063; [2A 14, 05, 05]\n"
                             "0068; [2A 16, 05, 05]\n"
                             "006C; [2A 18, 05, 05]\n"
                             "00B7; [05 0A, 05, 05]\n"
                             "006C | 00B7; [, FB B6, 05]\n"
                             "0301; [, 88, 05]\n"
                             "0323; [, 8A, 05]\n"
                             "0061 0301; [2A 11, 05, 05]\n";

static RootTable s_root;
static CollationMappings s_m;

static int Open(void)
{
    FILE* f = fopen(s_path, "wb");
    if (f == nullptr)
    {
        return 0;
    }
    fputs(s_file, f);
    fclose(f);
    char error[128];
    return RootRead(s_path, &s_root, error, sizeof(error)) && CollationMappingsInit(&s_m, &s_root);
}

static uint64_t E(uint32_t p)
{
    return (uint64_t)p << 32 | 0x05000500;
}

// Whether text[start, length) after text[0, start) has the elements.
static bool Is(const uint32_t* text, uint32_t length, uint32_t start, const uint64_t* expected,
               uint32_t count)
{
    uint64_t out[collationMaxExpansion];
    uint32_t n = CollationMappingsElements(&s_m, text, length, start, out, 0);
    bool same = n == count && memcmp(out, expected, count * sizeof *out) == 0;
    if (!same)
    {
        fprintf(stderr, "  %u elements:", n);
        for (uint32_t i = 0; i < n && i < collationMaxExpansion; i++)
        {
            fprintf(stderr, " %016llX", (unsigned long long)out[i]);
        }
        fprintf(stderr, "\n");
    }
    return same;
}

#define TEXT(...)     (const uint32_t[]){__VA_ARGS__}, sizeof((const uint32_t[]){__VA_ARGS__}) / 4
#define ELEMENTS(...) (const uint64_t[]){__VA_ARGS__}, sizeof((const uint64_t[]){__VA_ARGS__}) / 8

static void TestRoot(void)
{
    CHECK(Is(TEXT(0x61), 0, ELEMENTS(E(0x2A100000))) &&
              Is(TEXT(0x61, 0x301), 0, ELEMENTS(E(0x2A110000))),
          "a code point and the root's contraction");
    CHECK(Is(TEXT(0x61, 0x323, 0x301), 0, ELEMENTS(E(0x2A110000), 0x8A000500)) &&
              Is(TEXT(0x61, 0x301, 0x301), 0, ELEMENTS(E(0x2A110000), 0x88000500)),
          "a discontiguous contraction, the skipped mark after it");
    CHECK(Is(TEXT(0x6C, 0xB7), 0, ELEMENTS(E(0x2A180000), 0xFBB60500)) &&
              Is(TEXT(0x61, 0xB7), 0, ELEMENTS(E(0x2A100000), E(0x050A0000))) &&
              Is(TEXT(0x6C, 0xB7), 1, ELEMENTS(0xFBB60500)),
          "the root's prefix, matched or not, and before the start");
    CHECK(Is(TEXT(0x4E01), 0, ELEMENTS(E(0xFD020400))) &&
              Is(TEXT(0xFFFD), 0, ELEMENTS(E(0xFFFD0000))) &&
              Is(TEXT(0xFFFF), 0, ELEMENTS(E(0xFFFF0000))) &&
              Is(TEXT(0x78), 0, ELEMENTS(E(0xFE0408B8))),
          "Han, U+FFFD, U+FFFF and an unassigned code point by rule");
}

// Whether a code point is after a prefix and starts a contraction.
static bool ContextsAre(uint32_t codePoint, bool prefixed, bool contracts)
{
    bool p = !prefixed;
    bool c = !contracts;
    CollationMappingsContexts(&s_m, codePoint, &p, &c);
    return p == prefixed && c == contracts;
}

static void TestContextsRoot(void)
{
    CHECK(ContextsAre(0x61, false, true) && ContextsAre(0xB7, true, false) &&
              ContextsAre(0x62, false, false) && ContextsAre(0x6C, false, false),
          "the root's contexts: a's contraction, the middle dot's prefix, none for b or l");
    CHECK(CollationMappingsExtends(&s_m, TEXT(0x61)) &&
              !CollationMappingsExtends(&s_m, TEXT(0x61, 0x301)) &&
              !CollationMappingsExtends(&s_m, TEXT(0x62)) &&
              !CollationMappingsExtends(&s_m, TEXT(0xB7)) &&
              !CollationMappingsExtends(&s_m, TEXT(0x6C, 0xB7)) &&
              !CollationMappingsExtends(&s_m, nullptr, 0),
          "a mapping longer than a text that starts with it: a before a acute alone");
}

static void TestContextsTailored(void)
{
    CHECK(ContextsAre(0x63, false, true) && ContextsAre(0x68, true, true) &&
              ContextsAre(0x62, false, false),
          "the tailoring's contexts: c's contraction, h's prefixes with contractions");
    CHECK(CollationMappingsExtends(&s_m, TEXT(0x63)) &&
              !CollationMappingsExtends(&s_m, TEXT(0x63, 0x68)) &&
              CollationMappingsExtends(&s_m, TEXT(0x68)) &&
              !CollationMappingsExtends(&s_m, TEXT(0x68, 0x63)) &&
              !CollationMappingsExtends(&s_m, TEXT(0x68, 0x61)),
          "longer mappings: ch after c, hc and hb after h, none after hc");
}

static void TestTailored(void)
{
    const uint64_t t1 = E(0x30000000);
    const uint64_t t2 = E(0x31000000);
    const uint64_t t3 = E(0x32000000);
    const uint64_t t4 = E(0x33000000);
    CHECK(CollationMappingsAdd(&s_m, nullptr, 0, TEXT(0x62), &t1, 1) &&
              Is(TEXT(0x62), 0, ELEMENTS(t1)) &&
              s_m.mappings[CollationMappingsFirst(&s_m, 0x62)].next < 0,
          "a mapping replaces the root's, nothing copied");
    CHECK(CollationMappingsAdd(&s_m, nullptr, 0, TEXT(0x61), &t2, 1) &&
              Is(TEXT(0x61), 0, ELEMENTS(t2)) && Is(TEXT(0x61, 0x301), 0, ELEMENTS(E(0x2A110000))),
          "the root's contraction copied with the first mapping");
    CHECK(CollationMappingsAdd(&s_m, nullptr, 0, TEXT(0x63, 0x68), &t3, 1) &&
              Is(TEXT(0x63, 0x68), 0, ELEMENTS(t3)) && Is(TEXT(0x63), 0, ELEMENTS(E(0x2A140000))) &&
              Is(TEXT(0x63, 0x62), 0, ELEMENTS(E(0x2A140000), t1)),
          "a contraction added, the root's mapping alone copied");
    CHECK(CollationMappingsAdd(&s_m, TEXT(0x63), TEXT(0x68), &t4, 1) &&
              Is(TEXT(0x63, 0x68), 0, ELEMENTS(t3)) && Is(TEXT(0x63, 0x68), 1, ELEMENTS(t4)) &&
              Is(TEXT(0x62, 0x68), 0, ELEMENTS(t1, E(0x2A160000))),
          "a prefix added: matched after its code point, not after another");
}

static void TestPrefixFallback(void)
{
    const uint64_t t5 = E(0x34000000);
    const uint64_t t6 = E(0x35000000);
    CHECK(CollationMappingsAdd(&s_m, TEXT(0x61), TEXT(0x68, 0x63), &t5, 1) &&
              CollationMappingsAdd(&s_m, TEXT(0x62, 0x61), TEXT(0x68, 0x62), &t6, 1),
          "prefixes with contractions only");
    CHECK(Is(TEXT(0x61, 0x68, 0x63), 1, ELEMENTS(t5)) &&
              Is(TEXT(0x61, 0x68), 1, ELEMENTS(E(0x2A160000))) &&
              Is(TEXT(0x62, 0x61, 0x68, 0x62), 2, ELEMENTS(t6)) &&
              Is(TEXT(0x62, 0x61, 0x68, 0x63), 2, ELEMENTS(t5)) &&
              Is(TEXT(0x62, 0x61, 0x68, 0x61), 2, ELEMENTS(E(0x2A160000), E(0x31000000))),
          "no contraction of the prefix matching: the next shorter prefix's");
}

static void TestReplaceAndLimits(void)
{
    const uint64_t t7 = E(0x36000000);
    const uint64_t withIgnorable[] = {0, E(0x37000000)};
    CHECK(CollationMappingsAdd(&s_m, nullptr, 0, TEXT(0x62), &t7, 1) &&
              Is(TEXT(0x62), 0, ELEMENTS(t7)) &&
              s_m.mappings[CollationMappingsFirst(&s_m, 0x62)].next < 0,
          "a mapping of the same prefix and text replaced");
    CHECK(CollationMappingsAdd(&s_m, nullptr, 0, TEXT(0x6C), withIgnorable, 2) &&
              Is(TEXT(0x6C), 0, ELEMENTS(E(0x37000000))),
          "a completely ignorable element left out");
    uint32_t many[40];
    for (uint32_t i = 0; i < 40; i++)
    {
        many[i] = 0x62;
    }
    uint64_t out[collationMaxExpansion];
    CHECK(CollationMappingsElements(&s_m, many, 40, 0, out, 0) == 40 && out[30] == t7,
          "more elements than an expansion holds: counted, 31 written");
    uint32_t eight[collationMaxContext] = {0x61, 0x62, 0x63, 0x68, 0x6C, 0x61, 0x62, 0x63};
    uint32_t nine[collationMaxContext + 2] = {0x68, 0x61, 0x62, 0x63, 0x68,
                                              0x6C, 0x61, 0x62, 0x63, 0x61};
    const uint64_t t8 = E(0x38000000);
    const uint64_t t9 = E(0x39000000);
    CHECK(CollationMappingsAdd(&s_m, eight, collationMaxContext, TEXT(0x6C), &t8, 1) &&
              Is((const uint32_t[]){0x61, 0x62, 0x63, 0x68, 0x6C, 0x61, 0x62, 0x63, 0x6C}, 9, 8,
                 ELEMENTS(t8)),
          "a prefix of eight code points");
    CHECK(CollationMappingsAdd(&s_m, nullptr, 0, nine, collationMaxContext + 1, &t9, 1) &&
              Is(nine, collationMaxContext + 2, 0, ELEMENTS(t9, E(0x31000000))),
          "a contraction of nine code points, then the next");
    uint64_t long32[collationMaxExpansion + 1] = {0};
    CHECK(!CollationMappingsAdd(&s_m, nullptr, 0, many, 0, &t7, 1) &&
              !CollationMappingsAdd(&s_m, nullptr, 0, many, collationMaxContext + 2, &t7, 1) &&
              !CollationMappingsAdd(&s_m, many, collationMaxContext + 1, many, 1, &t7, 1) &&
              !CollationMappingsAdd(&s_m, nullptr, 0, many, 1, long32, collationMaxExpansion + 1),
          "an empty or long text, a long prefix, too many elements refused");
}

static void TestManyCodePoints(void)
{
    bool ok = true;
    for (uint32_t c = 0x100; ok && c < 0x500; c++)
    {
        uint64_t e = E(0x40000000 + (c << 8));
        ok = CollationMappingsAdd(&s_m, nullptr, 0, &c, 1, &e, 1);
    }
    for (uint32_t c = 0x100; ok && c < 0x500; c++)
    {
        uint64_t out[collationMaxExpansion];
        ok = CollationMappingsElements(&s_m, &c, 1, 0, out, 0) == 1 &&
             out[0] == E(0x40000000 + (c << 8));
    }
    CHECK(ok && s_m.slotCount >= 2 * s_m.slotsUsed, "1,024 code points through the table's growth");
}

int main(void)
{
    if (!Open())
    {
        CHECK(false, "the root reads");
        return 1;
    }
    TestRoot();
    TestContextsRoot();
    TestTailored();
    TestPrefixFallback();
    TestContextsTailored();
    TestReplaceAndLimits();
    TestManyCodePoints();
    CollationMappingsFree(&s_m);
    RootFree(&s_root);
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
