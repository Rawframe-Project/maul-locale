// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root collation reader (record mloc-0064) on small files of
// FractionalUCA.txt's form: weights left-aligned into 64-bit elements,
// prefixes and contractions, elements naming a code point's own, Han's
// implicit primaries, script starts and special lines kept apart, the
// runtime's fixed code points left out, and the lines it refuses.

// The C runtime of Windows reports fopen as deprecated.
#define _CRT_SECURE_NO_WARNINGS

#include "collation_root.h"
#include "test_harness.h"

#include <stdio.h>
#include <string.h>

static const char* s_path = "gencollation.txt";

static int Read(const char* text, RootTable* t, char* error, size_t size)
{
    FILE* f = fopen(s_path, "wb");
    if (f == nullptr)
    {
        return 0;
    }
    fputs(text, f);
    fclose(f);
    return RootRead(s_path, t, error, size);
}

static const RootMapping* Single(const RootTable* t, uint32_t c)
{
    for (size_t i = 0; i < t->count; i++)
    {
        if (t->mappings[i].textLength == 1 && t->mappings[i].prefixLength == 0 &&
            t->mappings[i].text[0] == c)
        {
            return &t->mappings[i];
        }
    }
    return nullptr;
}

static const char s_file[] = "\xEF\xBB\xBF# FractionalUCA.txt, shortened\n"
                             "[UCA version = 18.0.1]\n"
                             "[Unified_Ideograph 4E00..4E01 3400]\n"
                             "[top_byte\t2A\tLatn]\n"
                             "[top_byte\t62\tCyrl\tCOMPRESS]\n"
                             "[first variable [03 04, 05, 05]] # ignored\n"
                             "0061; [2B, 05, 05]\t# Latn Ll\n"
                             "0041; [2B, 05, 9C]\n"
                             "00E0; [2B, 05, 05][, 8A, 05]\n"
                             "0300; [, 8A, 05]\n"
                             "0430; [62 0A, 05, 05]\n"
                             "0063 0068; [2F 02, 05, 05]\n"
                             "004C | 00B7; [, FB B6, 05]\n"
                             "3220; [09 7C, 05, 17][U+4E00, 10][09 7E, 05, 17]\n"
                             "2F00; [U+4E01, 70, 20]\n"
                             "1F600; [0D 4C 5C, 05, 05]\n"
                             "FDD1 0061; [2B, 05, 05]\n"
                             "FDD0 0034; [10, 05, 05]\n"
                             "FFFD;\t[EF FD, 05, 05]\n"
                             "FFFE;\t[02, 05, 05]\n"
                             "FFFF;\t[EF FF, 05, 05]\n";

static void TestRead(void)
{
    RootTable t;
    char error[128] = "";
    int ok = Read(s_file, &t, error, sizeof(error));
    CHECK(ok && t.version[0] == 18 && t.version[1] == 0 && t.version[2] == 1 &&
              t.hanRangeCount == 2 && t.compressible[0x62] && !t.compressible[0x2A],
          "the version, the Han ranges and the compressible lead bytes");
    // Three Han code points: 3 x 2 + 4 primaries fit one lead byte below FE.
    CHECK(ok && t.firstHanPrimary == 0xFD020200,
          "Han's first primary one lead byte below the unassigned byte");
    const RootMapping* a = ok ? Single(&t, 0x61) : nullptr;
    const RootMapping* upper = ok ? Single(&t, 0x41) : nullptr;
    const RootMapping* grave = ok ? Single(&t, 0xE0) : nullptr;
    const RootMapping* cyrillic = ok ? Single(&t, 0x430) : nullptr;
    const RootMapping* face = ok ? Single(&t, 0x1F600) : nullptr;
    CHECK(a != nullptr && a->elementCount == 1 && a->elements[0] == 0x2B00000005000500ULL &&
              upper != nullptr && upper->elements[0] == 0x2B00000005009C00ULL,
          "one-byte weights left-aligned, the tertiary's case bits kept");
    CHECK(grave != nullptr && grave->elementCount == 2 &&
              grave->elements[1] == 0x000000008A000500ULL && cyrillic != nullptr &&
              cyrillic->elements[0] == 0x620A000005000500ULL && face != nullptr &&
              face->elements[0] == 0x0D4C5C0005000500ULL,
          "two elements, an empty primary, two- and three-byte primaries");
    bool contraction = false;
    bool prefix = false;
    for (size_t i = 0; ok && i < t.count; i++)
    {
        const RootMapping* m = &t.mappings[i];
        contraction |= m->textLength == 2 && m->text[0] == 0x63 && m->text[1] == 0x68 &&
                       m->prefixLength == 0 && m->elements[0] == 0x2F02000005000500ULL;
        prefix |= m->prefixLength == 1 && m->prefix[0] == 0x4C && m->textLength == 1 &&
                  m->text[0] == 0xB7 && m->elements[0] == 0x00000000FBB60500ULL;
    }
    CHECK(contraction && prefix, "a contraction, and a prefix before '|' kept apart from the text");
    const RootMapping* paren = ok ? Single(&t, 0x3220) : nullptr;
    const RootMapping* radical = ok ? Single(&t, 0x2F00) : nullptr;
    CHECK(paren != nullptr && paren->elementCount == 3 &&
              paren->elements[1] == 0xFD02020005001000ULL && radical != nullptr &&
              radical->elements[0] == 0xFD02040070002000ULL,
          "an element naming a Han code point: its tertiary, or its secondary and tertiary, "
          "replaced");
    CHECK(ok && t.scriptStartCount == 1 && t.scriptStarts[0].codePoint == 0x61 &&
              t.specialCount == 1 && t.specials[0].element == 0x1000000005000500ULL &&
              Single(&t, 0xFDD1) == nullptr,
          "FDD1 and FDD0 lines kept apart, not as mappings");
    CHECK(ok && Single(&t, 0xFFFD) == nullptr && Single(&t, 0xFFFF) == nullptr &&
              Single(&t, 0xFFFE) != nullptr && Single(&t, 0xFFFE)->elements[0] >> 32 == 0x02000000,
          "U+FFFD and U+FFFF left to the runtime, U+FFFE kept");
    CHECK(ok && RootHanElement(&t, 0x4E00) == 0xFD02020005000500ULL &&
              RootHanElement(&t, 0x3400) == 0xFD02060005000500ULL &&
              RootHanElement(&t, 0x4E02) == 0,
          "Han primaries a step of 2 apart in the ranges' order, none outside them");
    if (ok)
    {
        RootFree(&t);
    }
}

static void TestUnassigned(void)
{
    CHECK(RootUnassignedPrimary(0x378) == 0xFE043364 && RootUnassignedPrimary(0) == 0xFE040210 &&
              RootUnassignedPrimary(0x10FFFF) == 0xFEF7AF1E,
          "unassigned primaries as ICU makes them, for the first, an unassigned and the last "
          "code point");
}

static void TestRefusals(void)
{
    static const char* const bad[] = {
        "[Unified_Ideograph 4E00]\n0061; [2B 00 03, 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061; [2B 03 04 05 06, 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061 [2B, 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061;\n",
        "[Unified_Ideograph 4E00]\n110000; [2B, 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061; [2B, 05 05 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061; [2B; 05, 05]\n",
        "[UCA version = 18]\n[Unified_Ideograph 4E00]\n0061; [2B, 05, 05]\n",
        "0061; [2B, 05, 05]\n",
        "[Unified_Ideograph 4E05..4E00]\n0061; [2B, 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061; [U+4E00, 10\n",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        RootTable t;
        char error[128] = "";
        CHECK(!Read(bad[i], &t, error, sizeof(error)) && error[0] != '\0' && t.count == 0,
              "a bad weight, too many bytes, no ';', no elements, a code point past U+10FFFF, a "
              "three-byte secondary, a wrong separator, a short version, no Han line, a Han "
              "range ending before it starts, an unclosed element: refused with a message");
    }
    RootTable t;
    char error[128] = "";
    CHECK(!RootRead("no-such-file.txt", &t, error, sizeof(error)) &&
              strstr(error, "cannot read") != nullptr,
          "a missing file refused");
}

int main(void)
{
    TestRead();
    TestUnassigned();
    TestRefusals();
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
