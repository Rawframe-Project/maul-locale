// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root collation reader on small files of FractionalUCA.txt's form:
// weights left-aligned into 64-bit elements, prefixes and contractions,
// elements naming a code point's own, Han's implicit primaries, script
// starts and special lines kept apart, the runtime's fixed code points
// left out, and the lines it refuses. Then the root unit built from them
// and read back, and its validator on units written by hand.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_root.h"
#include "collation_tables.h"
#include "collation_unit.h"
#include "table_gen.h"
#include "test_harness.h"
#include "trie_gen.h"

#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
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
                             "[Unified_Ideograph 4E00..4E01 3400..3400]\n"
                             "[top_byte\t03\tSPACE]\n"
                             "[top_byte\t2A\tLatn] COMPRESS\n"
                             "[top_byte\t62\tCyrl\tCOMPRESS]\n"
                             "[first variable [03 04, 05, 05]] # ignored\n"
                             "0061; [2B, 05, 05]\t# Latn Ll\n"
                             "0041; [2B, 05, 9C]\n"
                             "0062; [2c, 05, 0f]\n"
                             "2F01; [U+0061, 9C]\n"
                             "00E0; [2B, 05, 05][, 8A, 05]\n"
                             "0300; [, 8A, 05]\n"
                             "0430; [62 0A, 05, 05]\n"
                             "0063 0068; [2F 02, 05, 05]\n"
                             "004C | 00B7; [, FB B6, 05]\n"
                             "3220; [09 7C, 05, 17][U+4E00, 10][09 7E, 05, 17]\n"
                             "2F00; [U+4E01, 70, 20]\n"
                             "1F600; [0D 4C 5C, 05, 05]\n"
                             "FDD1 0061; [2B, 05, 05]\n"
                             "FDD1 0430; [62 0A, 05, 05]\n"
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
    const RootMapping* b = ok ? Single(&t, 0x62) : nullptr;
    const RootMapping* named = ok ? Single(&t, 0x2F01) : nullptr;
    CHECK(b != nullptr && b->elements[0] == 0x2C00000005000F00ULL && named != nullptr &&
              named->elements[0] == 0x2B00000005009C00ULL && t.compressible[0x03] == 0,
          "lowercase hex, an element naming a code point that is not Han, a top byte of 0x, "
          "COMPRESS only inside the brackets");
    const RootMapping* paren = ok ? Single(&t, 0x3220) : nullptr;
    const RootMapping* radical = ok ? Single(&t, 0x2F00) : nullptr;
    CHECK(paren != nullptr && paren->elementCount == 3 &&
              paren->elements[1] == 0xFD02020005001000ULL && radical != nullptr &&
              radical->elements[0] == 0xFD02040070002000ULL,
          "an element naming a Han code point: its tertiary, or its secondary and tertiary, "
          "replaced");
    CHECK(ok && t.scriptStartCount == 2 && t.scriptStarts[0].codePoint == 0x61 &&
              t.scriptStarts[1].codePoint == 0x430 && t.specialCount == 1 &&
              t.specials[0].element == 0x1000000005000500ULL && Single(&t, 0xFDD1) == nullptr,
          "FDD1 and FDD0 lines kept apart, not as mappings");
    CHECK(ok && Single(&t, 0xFFFD) == nullptr && Single(&t, 0xFFFF) == nullptr &&
              Single(&t, 0xFFFE) != nullptr && Single(&t, 0xFFFE)->elements[0] >> 32 == 0x02000000,
          "U+FFFD and U+FFFF left to the runtime, U+FFFE kept");
    CHECK(ok && RootHanElement(&t, 0x4E00) == 0xFD02020005000500ULL &&
              RootHanElement(&t, 0x3400) == 0xFD02060005000500ULL &&
              RootHanElement(&t, 0x4E02) == 0 && RootHanElement(&t, 0) == 0,
          "Han primaries a step of 2 apart in the ranges' order, none outside them");
    if (ok)
    {
        RootFree(&t);
    }
}

static void TestUnassigned(void)
{
    CHECK(mlocCollationUnassignedPrimary(0x378) == 0xFE043364 &&
              mlocCollationUnassignedPrimary(0) == 0xFE040210 &&
              mlocCollationUnassignedPrimary(0x10FFFF) == 0xFEF7AF1E,
          "unassigned primaries as ICU makes them, for the first, an unassigned and the last "
          "code point");
}

// Han's lead bytes at the bound of one: 254 x 254 primaries hold twice
// 32,256 Han code points and 4 more, and no more.
static void TestHanLeadBytes(void)
{
    RootTable t;
    char error[128] = "";
    int one =
        Read("[Unified_Ideograph 4E00..CBFF]\n0061; [2B, 05, 05]\n", &t, error, sizeof(error));
    uint32_t first = one ? t.firstHanPrimary : 0;
    if (one)
    {
        RootFree(&t);
    }
    int two =
        Read("[Unified_Ideograph 4E00..CC00]\n0061; [2B, 05, 05]\n", &t, error, sizeof(error));
    CHECK(one && first == 0xFD020200 && two && t.firstHanPrimary == 0xFC020200,
          "32,256 Han code points in one lead byte, one more in two");
    if (two)
    {
        RootFree(&t);
    }
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
        "[Unified_Ideograph 4E00]\n ; [2B, 05, 05]\n",
        "[Unified_Ideograph 4E00]\n0061; [U+0378, 10]\n",
        "[Unified_Ideograph 4E00]\n00E0; [2B, 05, 05][, 8A, 05]\n0062; [U+00E0, 10]\n",
        "[Unified_Ideograph 1 2 3 4 5 6 7 8 9 A B C D E F 10 11 12 13 14 15 16 17 18 19 1A 1B 1C "
        "1D "
        "1E 1F 20 21]\n0061; [2B, 05, 05]\n",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        RootTable t;
        char error[128] = "";
        CHECK(!Read(bad[i], &t, error, sizeof(error)) && error[0] != '\0' && t.count == 0,
              "a bad weight, too many bytes, no ';', no elements, a code point past U+10FFFF, a "
              "three-byte secondary, a wrong separator, a short version, no Han line, a Han "
              "range ending before it starts, an unclosed element, no code points, an element "
              "naming a code point with no element or with two, 33 Han ranges: refused with a "
              "message");
    }
    RootTable t;
    char error[128] = "";
    CHECK(!RootRead("no-such-file.txt", &t, error, sizeof(error)) &&
              strstr(error, "cannot read") != nullptr,
          "a missing file refused");
}

// The CE32 a context list gives for a string, or 0.
static uint32_t Entry(const CollationRoot* r, uint32_t ce32, uint32_t c)
{
    uint32_t index = mlocCollationListStart(r, mlocCe32Index(ce32));
    uint32_t count = mlocCollationContext(r, index + 1);
    for (uint32_t e = 0, at = index + 2; e < count; e++)
    {
        uint32_t n = mlocCollationContext(r, at);
        if (n == 1 && mlocCollationContext(r, at + 1) == c)
        {
            return mlocCollationContext(r, at + 2);
        }
        at += 2 + n;
    }
    return 0;
}

static void TestUnit(void)
{
    RootTable t;
    char error[128] = "";
    size_t size = 0;
    uint8_t* bytes = Read(s_file, &t, error, sizeof(error))
                         ? CollationRootUnit(&t, &size, error, sizeof(error))
                         : nullptr;
    CollationRoot r;
    bool ok = bytes != nullptr && mlocReadCollationRoot((DataSpan){bytes, (uint32_t)size}, &r);
    CHECK(ok && r.version == (18U << 16 | 1), "the unit reads back with its version");
    uint32_t a = ok ? mlocTrieValue(&r.trie, 0x61) : 0;
    uint32_t face = ok ? mlocTrieValue(&r.trie, 0x1F600) : 0;
    uint32_t combining = ok ? mlocTrieValue(&r.trie, 0x300) : 0;
    CHECK(ok && !mlocCe32IsSpecial(a) && mlocCe32Element(a) == 0x2B00000005000500ULL &&
              mlocCe32Tag(face) == collationTagLongPrimary &&
              mlocCe32Element(face) == 0x0D4C5C0005000500ULL && !mlocCe32IsSpecial(combining) &&
              mlocCe32Element(combining) == 0x000000008A000500ULL,
          "simple CE32s for one-byte weights, a long primary for three bytes");
    CHECK(ok && mlocTrieValue(&r.trie, 0xE0) == collationImplicitCe32 &&
              mlocTrieValue(&r.trie, 0xAC00) == collationImplicitCe32 &&
              mlocTrieValue(&r.trie, 0x378) == collationImplicitCe32,
          "a decomposable code point and a Hangul syllable left out, an unassigned one implicit");
    uint32_t han = ok ? mlocTrieValue(&r.trie, 0x3400) : 0;
    CHECK(ok && mlocCe32Tag(han) == collationTagOffset &&
              mlocCollationOffsetElement(mlocCollationCe(&r, mlocCe32Index(han)), 0x3400) ==
                  RootHanElement(&t, 0x3400) &&
              mlocCollationOffsetElement(mlocCollationCe(&r, mlocCe32Index(han)), 0x33FF) == 0,
          "Han through an offset, nothing for a code point before its range");
    uint32_t han2 = ok ? mlocTrieValue(&r.trie, 0x4E01) : 0;
    CHECK(ok && mlocCe32Tag(han2) == collationTagOffset &&
              mlocCollationOffsetElement(mlocCollationCe(&r, mlocCe32Index(han2)), 0x4E01) ==
                  RootHanElement(&t, 0x4E01) &&
              mlocTrieValue(&r.trie, 0) == collationImplicitCe32 &&
              mlocTrieValue(&r.trie, 0x10FFFF) == collationImplicitCe32,
          "a Han range's last code point through its offset; U+0000 and U+10FFFF implicit");
    uint32_t c = ok ? mlocTrieValue(&r.trie, 0x63) : 0;
    uint32_t dot = ok ? mlocTrieValue(&r.trie, 0xB7) : 0;
    CHECK(ok && mlocCe32Tag(c) == collationTagContraction &&
              mlocCollationContext(&r, mlocCollationListStart(&r, mlocCe32Index(c))) ==
                  collationImplicitCe32 &&
              mlocCe32Element(Entry(&r, c, 0x68)) == 0x2F02000005000500ULL &&
              mlocCe32Tag(dot) == collationTagPrefix &&
              mlocCe32Tag(Entry(&r, dot, 0x4C)) == collationTagLongSecondary &&
              mlocCe32Element(Entry(&r, dot, 0x4C)) == 0x00000000FBB60500ULL,
          "a contraction and a prefix as context lists with their defaults, a two-byte "
          "secondary as a long secondary");
    uint32_t paren = ok ? mlocTrieValue(&r.trie, 0x3220) : 0;
    uint32_t index = mlocCe32Index(paren);
    CHECK(ok && mlocCe32Tag(paren) == collationTagExpansion && mlocCe32Length(paren) == 3 &&
              mlocCollationCe(&r, index + 1) == 0xFD02020005001000ULL,
          "three elements, one not a CE32, as an expansion of 64-bit elements");
    CHECK(ok && mlocCollationCompressible(&r, 0x62) && !mlocCollationCompressible(&r, 0x2A) &&
              !mlocCollationCompressible(&r, 0x03) && mlocCollationCompressible(&r, 0xFE),
          "the compressible lead bytes: COMPRESS inside the brackets, and the unassigned byte");
    uint8_t* copy = ok ? malloc(size) : nullptr;
    if (copy != nullptr)
    {
        CollationRoot broken;
        memcpy(copy, bytes, size);
        copy[4] = (uint8_t)(copy[4] + 1);
        CHECK(!mlocReadCollationRoot((DataSpan){copy, (uint32_t)size}, &broken) &&
                  !mlocReadCollationRoot((DataSpan){bytes, (uint32_t)size - 1}, &broken) &&
                  !mlocReadCollationRoot((DataSpan){bytes, collationRootHead - 1}, &broken),
              "a unit longer than its payload, or a payload cut short, refused");
        // A trie value with an unknown tag, an expansion past its array, and a
        // context list longer than the contexts.
        static const uint32_t bad[] = {0xFFFFFFC3, 0xFFFFE3C6, 0xFFFFE0C9};
        for (size_t i = 0; i < 3; i++)
        {
            memcpy(copy, bytes, size);
            size_t at = (size_t)(r.trie.data - bytes) + (size_t)mlocLoad32(r.trie.bmp) * 4;
            for (int k = 0; k < 4; k++)
            {
                copy[at + k] = (uint8_t)(bad[i] >> (8 * k));
            }
            CHECK(!mlocReadCollationRoot((DataSpan){copy, (uint32_t)size}, &broken),
                  "a trie value with an unknown tag, an expansion past its array or a context "
                  "past the contexts refused");
        }
        // The lists: an empty list, a last start short
        // of the contexts, and a first list's entry count one more or one less
        // than fills it.
        size_t starts = (size_t)(r.listStarts - bytes);
        size_t first = (size_t)(r.contexts - bytes) + 4;
        uint32_t count = mlocCollationContext(&r, 1);
        const struct
        {
            size_t at;
            uint32_t value;
        } lists[] = {
            {starts + 4, 0},
            {starts + (size_t)r.listCount * 4, r.contextCount - 1},
            {first, count + 1},
            {first, count - 1},
        };
        for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++)
        {
            memcpy(copy, bytes, size);
            for (int k = 0; k < 4; k++)
            {
                copy[lists[i].at + k] = (uint8_t)(lists[i].value >> (8 * k));
            }
            CHECK(!mlocReadCollationRoot((DataSpan){copy, (uint32_t)size}, &broken),
                  "list starts not rising to the contexts, or a list its entries do not fill, "
                  "refused");
        }
        // The scripts: Cyrillic and Latin, by code, from the FDD1 lines, numeric
        // order's lead byte from FDD0 0034; then a start not from 0, a last
        // start not the trailing range's, starts not rising, a script naming
        // start 0 or the last, scripts not rising, and one script too many.
        CHECK(r.scriptCount == 2 && r.startCount == 4 &&
                  mlocCollationScriptCode(&r, 0) == MUNI_SCRIPT('C', 'y', 'r', 'l') &&
                  mlocCollationScriptIndex(&r, MUNI_SCRIPT('C', 'y', 'r', 'l')) == 2 &&
                  mlocCollationScriptIndex(&r, MUNI_SCRIPT('G', 'r', 'e', 'k')) == 0 &&
                  mlocCollationLastPrimary(&r, MUNI_SCRIPT('L', 'a', 't', 'n')) ==
                      (mlocCollationStart(&r, 2) << 16) - 1 &&
                  mlocCollationStart(&r, 3) == 0xFF00 && r.numericPrimary == 0x10000000,
              "the scripts and their starts from the FDD1 lines, numeric order's lead byte");
        size_t scripts = (size_t)(r.scripts - bytes);
        size_t scriptStarts = (size_t)(r.starts - bytes);
        const struct
        {
            size_t at;
            uint32_t value;
        } tables[] = {
            {scriptStarts, 1},
            {scriptStarts + 12, 0xFE00},
            {scriptStarts + 8, mlocCollationStart(&r, 1)},
            {scripts + 4, 0},
            {scripts + 4, 3},
            {scripts + 8, MUNI_SCRIPT('C', 'y', 'r', 'l')},
            {20, 3},
        };
        for (size_t i = 0; i < sizeof(tables) / sizeof(tables[0]); i++)
        {
            memcpy(copy, bytes, size);
            for (int k = 0; k < 4; k++)
            {
                copy[tables[i].at + k] = (uint8_t)(tables[i].value >> (8 * k));
            }
            CHECK(!mlocReadCollationRoot((DataSpan){copy, (uint32_t)size}, &broken),
                  "a script table out of order or past its arrays refused");
        }
        free(copy);
    }
    free(bytes);
    if (ok)
    {
        RootFree(&t);
    }
}

// A unit written by hand: U+0061's trie value, two elements, two CE32
// words (a simple one and a long primary), two context lists (an empty
// one, and one whose entry's code point has a special CE32's low byte),
// and one word set anywhere after the build.
typedef struct Crafted
{
    uint32_t value;
    uint32_t contexts[7];
    size_t patchAt;
    uint32_t patch;
} Crafted;

enum
{
    craftNoPatch = 0,
    craftAfterTrie = 1,
    craftTrieLength = 2,
};

static bool Opens(Crafted c)
{
    static const uint64_t ces[] = {0x2B00000005000500ULL, 0xFA02020000000000ULL};
    static const uint32_t ce32s[] = {0x2B000500, 0x0D4C5CC1};
    static const uint32_t starts[] = {0, 2, 7};
    uint32_t* values = malloc((size_t)trieCodePoints * 4);
    if (values == nullptr)
    {
        return false;
    }
    for (uint32_t cp = 0; cp <= trieMaxCodePoint; cp++)
    {
        values[cp] = collationImplicitCe32;
    }
    values[0x61] = c.value;
    size_t trieSize = 0;
    uint8_t* trie = TrieBuild(values, &trieSize);
    free(values);
    size_t padded = (trieSize + 7) / 8 * 8;
    static const uint32_t scriptStarts[] = {0, 0xFF00};
    // The compressible lead bytes (32, all clear) end the unit.
    size_t size = collationRootHead + padded + sizeof(ces) + sizeof(ce32s) + sizeof(starts) +
                  sizeof(c.contexts) + sizeof(scriptStarts) + 32;
    uint8_t* bytes = calloc(1, size);
    if (trie == nullptr || bytes == nullptr)
    {
        free(trie);
        free(bytes);
        return false;
    }
    uint32_t words[] = {18U << 16, 2, 2, 7, 2, 0, 2, 0};
    uint8_t* p = bytes;
    for (size_t i = 0; i < 8; i++, p += 4)
    {
        GenPut(p, words[i], 4);
    }
    memcpy(bytes + collationRootHead, trie, trieSize);
    p = bytes + collationRootHead + padded;
    for (size_t i = 0; i < 2; i++, p += 8)
    {
        GenPut(p, ces[i], 8);
    }
    for (size_t i = 0; i < 2; i++, p += 4)
    {
        GenPut(p, ce32s[i], 4);
    }
    for (size_t i = 0; i < 3; i++, p += 4)
    {
        GenPut(p, starts[i], 4);
    }
    for (size_t i = 0; i < 7; i++, p += 4)
    {
        GenPut(p, c.contexts[i], 4);
    }
    for (size_t i = 0; i < 2; i++, p += 4)
    {
        GenPut(p, scriptStarts[i], 4);
    }
    if (c.patchAt == craftAfterTrie)
    {
        GenPut(bytes + collationRootHead + trieSize, c.patch, 4);
    }
    else if (c.patchAt == craftTrieLength)
    {
        GenPut(bytes + collationRootHead + 12, c.patch, 4);
    }
    CollationRoot r;
    bool ok = mlocReadCollationRoot((DataSpan){bytes, (uint32_t)size}, &r);
    free(trie);
    free(bytes);
    return ok;
}

static void TestValidator(void)
{
    const uint32_t implicit = collationImplicitCe32;
    // A contraction on the second list, whose entry is a code point with
    // 0xC3 for its low byte and a simple CE32.
    Crafted good = {
        .value = 1U << 13 | 0xC0 | collationTagContraction,
        .contexts = {implicit, 0, implicit, 1, 1, 0xC3, 0x2B000500},
    };
    Crafted lastExpansion = good;
    lastExpansion.value = 1U << 13 | 1U << 8 | 0xC0 | collationTagExpansion32;
    Crafted lastOffset = good;
    lastOffset.value = 1U << 13 | 0xC0 | collationTagOffset;
    Crafted padding = good;
    padding.patchAt = craftAfterTrie;
    padding.patch = 0xC3;
    CHECK(Opens(good) && Opens(lastExpansion) && Opens(lastOffset) && Opens(padding),
          "an empty list, an entry's code point read as text, an expansion of a long "
          "primary ending at its array's end, the last offset, and the word after the "
          "trie's data, all accepted");
    Crafted bad[] = {good, good, good, good, good, good, good};
    bad[0].value = 0xC0;
    bad[1].value = 0x00000BD1;
    bad[2].value = 2U << 13 | 0xC0 | collationTagOffset;
    bad[3].value = 2U << 13 | 0xC0 | collationTagContraction;
    bad[4].contexts[0] = 0xFFFFFFC3;
    bad[5].contexts[6] = 0xC3;
    bad[6].patchAt = craftTrieLength;
    bad[6].patch = 0xFFFFFFFF;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        CHECK(!Opens(bad[i]), "a low byte 0xC0 with tag 0, a low byte 0xD1, an offset past "
                              "the elements, a context naming no list, a list's default or "
                              "entry not a CE32, or a broken trie, refused");
    }
}

// The unit of a root text, read back; null when it does not build.
static uint8_t* Unit(const char* text, RootTable* t, CollationRoot* r, char* error, size_t size)
{
    size_t bytesSize = 0;
    if (!Read(text, t, error, size))
    {
        return nullptr;
    }
    uint8_t* bytes = CollationRootUnit(t, &bytesSize, error, size);
    if (bytes != nullptr && !mlocReadCollationRoot((DataSpan){bytes, (uint32_t)bytesSize}, r))
    {
        free(bytes);
        bytes = nullptr;
    }
    return bytes;
}

// Appends a run of code points whose three-byte primaries rise by step;
// private use ones, so that none decomposes.
static size_t Run(char* out, size_t at, size_t size, uint32_t first, uint32_t last,
                  uint32_t primary, bool compressible, uint32_t step)
{
    for (uint32_t c = first; c <= last && at < size; c++)
    {
        uint32_t p = mlocAddToPrimary(primary, compressible, (c - first) * step);
        int n =
            snprintf(out + at, size - at, "%04X; [%02X %02X %02X, 05, 05]\n", (unsigned)c,
                     (unsigned)(p >> 24), (unsigned)(p >> 16 & 0xFF), (unsigned)(p >> 8 & 0xFF));
        at += n > 0 ? (size_t)n : 0;
    }
    return at;
}

// Whether every code point of a range reads through one offset, giving
// the reader's element.
static bool OneOffset(const RootTable* t, const CollationRoot* r, uint32_t first, uint32_t last)
{
    uint32_t ce32 = mlocTrieValue(&r->trie, first);
    bool ok = mlocCe32IsSpecial(ce32) && mlocCe32Tag(ce32) == collationTagOffset;
    for (uint32_t c = first; ok && c <= last; c++)
    {
        const RootMapping* m = Single(t, c);
        ok = mlocTrieValue(&r->trie, c) == ce32 && m != nullptr &&
             mlocCollationOffsetElement(mlocCollationCe(r, mlocCe32Index(ce32)), c) ==
                 m->elements[0];
    }
    return ok;
}

static void TestOffsetRuns(void)
{
    enum
    {
        textSize = 1 << 16,
    };
    char* text = malloc(textSize);
    if (text == nullptr)
    {
        CHECK(false, "memory for the runs");
        return;
    }
    size_t at = (size_t)snprintf(text, textSize,
                                 "[UCA version = 18.0.1]\n"
                                 "[Unified_Ideograph 4E00]\n"
                                 "[top_byte\t60\tCyrl]\n"
                                 "[top_byte\t62\tGrek\tCOMPRESS]\n");
    at = Run(text, at, textSize, 0xE000, 0xE0C0, 0x60100200, false, 2);
    at = Run(text, at, textSize, 0xE13C, 0xE143, 0x60600200, false, 4);
    at = Run(text, at, textSize, 0xE17C, 0xE182, 0x60700200, false, 4);
    at = Run(text, at, textSize, 0xE200, 0xE32B, 0x62100200, true, 2);
    at = Run(text, at, textSize, 0x10FF00, 0x10FFFF, 0x60800200, false, 2);
    CHECK(at < textSize, "the runs fit their text");
    RootTable t;
    CollationRoot r;
    char error[128] = "";
    uint8_t* bytes = Unit(text, &t, &r, error, sizeof(error));
    bool ok = bytes != nullptr;
    uint32_t short4 = ok ? mlocTrieValue(&r.trie, 0xE17C) : 0;
    CHECK(ok && OneOffset(&t, &r, 0xE000, 0xE0C0) && OneOffset(&t, &r, 0xE13C, 0xE143) &&
              OneOffset(&t, &r, 0xE200, 0xE32B) && OneOffset(&t, &r, 0x10FF00, 0x10FFFF) &&
              mlocCe32Tag(short4) == collationTagLongPrimary &&
              mlocCe32Tag(mlocTrieValue(&r.trie, 0xE182)) == collationTagLongPrimary,
          "runs of long primaries as one offset each: a step of 2 over three blocks, one "
          "boundary with 4 code points on either side, a compressible lead byte's second byte "
          "carried, a run to U+10FFFF; a run with 3 code points after its boundary kept");
    free(bytes);
    free(text);
    if (ok)
    {
        RootFree(&t);
    }
}

static void TestUnitShapes(void)
{
    static const char head[] = "[UCA version = 18.0.1]\n[Unified_Ideograph 4E00]\n";
    char text[2048];
    snprintf(text, sizeof(text),
             "%s0063; [30 05 06, 05, 05]\n0063 0062; [2D, 05, 05]\n0063 0068 0061; [2F, 05, 05]\n"
             "0063 0061; [2C, 05, 05]\n0063 0068; [2E, 05, 05]\n",
             head);
    RootTable t;
    CollationRoot r;
    char error[128] = "";
    uint8_t* bytes = Unit(text, &t, &r, error, sizeof(error));
    // The entries in increasing order: a, b, h, then h a.
    static const uint32_t order[] = {1, 0x61, 0, 1, 0x62, 0, 1, 0x68, 0, 2, 0x68, 0x61, 0};
    bool ok = bytes != nullptr && r.listCount == 1 && r.contextCount == 2 + 13 &&
              mlocCollationContext(&r, 1) == 4 &&
              mlocCe32Tag(mlocCollationContext(&r, 0)) == collationTagLongPrimary;
    for (size_t i = 0; ok && i < sizeof(order) / sizeof(order[0]); i++)
    {
        ok = order[i] == 0 || mlocCollationContext(&r, 2 + (uint32_t)i) == order[i];
    }
    CHECK(ok, "a contraction on a long primary, its entries by their code points, the shorter "
              "first");
    free(bytes);
    if (bytes != nullptr)
    {
        RootFree(&t);
    }
    char line[1024];
    size_t n = 0;
    for (int i = 0; i < collationMaxExpansion; i++)
    {
        n += (size_t)snprintf(line + n, sizeof(line) - n, "[2B, 05, 05]");
    }
    snprintf(text, sizeof(text), "%s0061; %s\n", head, line);
    bytes = Unit(text, &t, &r, error, sizeof(error));
    uint32_t a = bytes != nullptr ? mlocTrieValue(&r.trie, 0x61) : 0;
    CHECK(bytes != nullptr && mlocCe32Tag(a) == collationTagExpansion32 &&
              mlocCe32Length(a) == collationMaxExpansion && r.ce32Count == collationMaxExpansion,
          "an expansion of the most elements built");
    free(bytes);
    if (bytes != nullptr)
    {
        RootFree(&t);
    }
    static const char* const refused[] = {
        "0063 0068; [2E, 05, 05]\n004C | 0063; [2F, 05, 05]\n",
        "004C | 0063 0068; [2F, 05, 05]\n",
    };
    for (size_t i = 0; i < 2; i++)
    {
        snprintf(text, sizeof(text), "%s%s", head, refused[i]);
        error[0] = '\0';
        bytes = Unit(text, &t, &r, error, sizeof(error));
        CHECK(bytes == nullptr && error[0] != '\0',
              "a prefix with a contraction on one code point, and a prefix before two code "
              "points, refused with a message");
        free(bytes);
        RootFree(&t);
    }
}

static void TestLeftOut(void)
{
    static const uint32_t out[] = {0xAC00, 0xD7A3, 0xD800,  0xDFFF, 0xC0,
                                   0x100,  0x1E00, 0x1D15E, 0x2F800};
    static const uint32_t kept[] = {0x41,   0x7F,   0x80,    0x180,   0xD7A4,
                                    0xE000, 0x1E9C, 0x1D15D, 0x20000, 0x10FFFF};
    bool ok = true;
    for (size_t i = 0; i < sizeof(out) / sizeof(out[0]); i++)
    {
        ok = ok && CollationLeftOut(out[i]);
    }
    for (size_t i = 0; i < sizeof(kept) / sizeof(kept[0]); i++)
    {
        ok = ok && !CollationLeftOut(kept[i]);
    }
    CHECK(ok, "Hangul syllables, surrogates and decomposable code points of two, three and four "
              "UTF-8 bytes left out; the code points around them kept");
}

static void TestScripts(void)
{
    static const char root[] = "[UCA version = 18.0.1]\n[Unified_Ideograph 4E00]\n"
                               "[top_byte\t62\tCyrl\tCOMPRESS]\n"
                               "FDD1 00A0;\t[03 02 02, 05, 05]\n"
                               "FDD1 201C;\t[05 06 02, 05, 05]\n"
                               "FDD0 FF21;\t[28 02 02, 05, 05]\n"
                               "FDD1 0061;\t[2A 02 02, 05, 05]\n"
                               "FDD0 FF3A;\t[5F 02 02, 05, 05]\n"
                               "FDD1 0430;\t[62 04 02, 05, 05]\n"
                               "FDD1 3042;\t[7D 02 02, 05, 05]\n"
                               "FDD1 30A2;\t[7D 02 02, 05, 05]\n"
                               "FDD1 FDD0;\t[FE 04 02, 05, 05]\n"
                               "0061;\t[2A 02 02, 05, 05]\n";
    RootTable t;
    CollationRoot r;
    char error[128] = "";
    uint8_t* bytes = Unit(root, &t, &r, error, sizeof(error));
    static const uint32_t starts[] = {0,      0x0300, 0x0506, 0x2800, 0x2A00,
                                      0x5F00, 0x6200, 0x7D00, 0xFE00, 0xFF00};
    bool ok = bytes != nullptr && r.startCount == 10;
    for (uint32_t i = 0; ok && i < 10; i++)
    {
        ok = mlocCollationStart(&r, i) == starts[i];
    }
    CHECK(ok, "starts in primary order, a lead byte's lowest second byte put back, plain and "
              "compressible, the unassigned range's (its byte compressible, as ICU counts it), "
              "then the trailing range's");
    CHECK(ok && mlocCollationScriptIndex(&r, collationReservedBeforeLatin) == 3 &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('L', 'a', 't', 'n')) == 4 &&
              mlocCollationScriptIndex(&r, collationReservedAfterLatin) == 5 &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('H', 'i', 'r', 'a')) == 7 &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('K', 'a', 'n', 'a')) == 7 &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('H', 'r', 'k', 't')) == 7 &&
              mlocCollationScriptIndex(&r, MUNI_SCRIPT('Z', 'z', 'z', 'z')) == 8 &&
              mlocCollationLastPrimary(&r, collationGroupSpace) == 0x0505FFFF &&
              mlocCollationLastPrimary(&r, collationGroupPunctuation) == 0x27FFFFFF,
          "the reserved ranges either side of Latin, Hiragana and Katakana sharing a range with "
          "Hrkt, the groups' last primaries");
    free(bytes);
    if (bytes != nullptr)
    {
        RootFree(&t);
    }
    error[0] = '\0';
    bytes = Unit("[UCA version = 18.0.1]\n[Unified_Ideograph 4E00]\nFDD1 0021;\t[05 02, 05, 05]\n"
                 "0021;\t[05 02, 05, 05]\n",
                 &t, &r, error, sizeof(error));
    CHECK(bytes == nullptr && strstr(error, "no script") != nullptr,
          "a script start whose sample has no script refused");
    free(bytes);
    RootFree(&t);
}

int main(void)
{
    TestRead();
    TestUnit();
    TestValidator();
    TestOffsetRuns();
    TestUnitShapes();
    TestLeftOut();
    TestScripts();
    TestUnassigned();
    TestHanLeadBytes();
    TestRefusals();
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
