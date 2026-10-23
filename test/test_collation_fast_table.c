// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin table builder on small roots of FractionalUCA.txt's form,
// where it meets the limits no CLDR data reaches: the head's group tops
// and the first short primary's case bits, the letters overflowing the
// short primaries (digits then taking long ones) and overflowing them
// still (no table), secondaries and tertiaries past their room, second
// elements in another group or with weights the format cannot hold, a
// prefix, and contractions with a longer one after them.

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_fast_latin.h"
#include "collation_fast_table.h"
#include "collation_mappings.h"
#include "collation_root.h"
#include "collation_unit.h"
#include "test_harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* s_path = "fast_table.txt";

static const char s_head[] = "[UCA version = 18.0.1]\n"
                             "[Unified_Ideograph 4E00]\n"
                             "FDD1 00A0;\t[03 02, 05, 05]\n"
                             "FDD1 201C;\t[05 02, 05, 05]\n"
                             "FDD1 263A;\t[07 02, 05, 05]\n"
                             "FDD1 20AC;\t[09 02, 05, 05]\n"
                             "FDD1 0034;\t[0B 02, 05, 05]\n"
                             "FDD1 0061;\t[2A 02, 05, 05]\n"
                             "FDD1 03B1;\t[2C 02, 05, 05]\n"
                             "FDD1 FDD0;\t[FE 04 02, 05, 05]\n"
                             "0020;\t[03 05, 05, 05]\n"
                             "002C;\t[05 05, 05, 05]\n"
                             "002B;\t[07 05, 05, 05]\n"
                             "0024;\t[09 05, 05, 05]\n";

// Code points that are their own NFD, to give primaries of their own.
static const uint32_t s_letters[] = {
    0x61, 0x62, 0x63, 0x64, 0x65, 0x66,  0x67,  0x68,  0x69,  0x6A,  0x6B,  0x6C, 0x6D,
    0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73,  0x74,  0x75,  0x76,  0x77,  0x78,  0x79, 0x7A,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46,  0x47,  0x48,  0x49,  0x4A,  0x4B,  0x4C, 0x4D,
    0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53,  0x54,  0x55,  0x56,  0x57,  0x58,  0x59, 0x5A,
    0xDF, 0xE6, 0xF0, 0xF8, 0xFE, 0x111, 0x127, 0x131, 0x142, 0x14B, 0x153, 0x167};

// Signs that are their own NFD, for runs of secondaries.
static const uint32_t s_signs[] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8,
                                   0xA9, 0xAB, 0xAC, 0xAE, 0xAF, 0xB0, 0xB1, 0xB4,
                                   0xB5, 0xB6, 0xBB, 0xBF, 0xD7, 0xF7};

typedef struct Built
{
    RootTable table;
    CollationMappings mappings;
    uint8_t* unit;
    CollationRoot root;
    uint16_t* words;
    size_t count;
} Built;

static char s_text[1 << 14];
static size_t s_length;

static void Add(const char* format, uint32_t a, uint32_t b, uint32_t c)
{
    s_length += (size_t)snprintf(s_text + s_length, sizeof(s_text) - s_length, format, a, b, c);
}

// The head and n letters with rising primaries from 2A 10; with digits,
// ten digits with primaries of their own, 0B then from 0B 12. The combining
// marks are one secondary, so that precomposed letters take no primary
// of their own; Greek starts at 2C, after Latin.
static void Letters(size_t n, bool digits)
{
    s_length = 0;
    s_length = (size_t)snprintf(s_text, sizeof(s_text), "%s", s_head);
    for (uint32_t c = 0x300; c <= 0x36F; c++)
    {
        Add("%04X;\t[, 85, 05]\n", c, 0, 0);
    }
    for (size_t i = 0; i < n; i++)
    {
        Add("%04X;\t[2A %02X, 05, 05]\n", s_letters[i], (uint32_t)(0x10 + 2 * i), 0);
    }
    // '0' at the digits' first primary itself.
    for (uint32_t i = 0; digits && i < 10; i++)
    {
        Add(i == 0 ? "%04X;\t[0B, 05, 05]\n" : "%04X;\t[0B %02X, 05, 05]\n", 0x30 + i, 0x10 + 2 * i,
            0);
    }
}

// The table built over s_text; false when the root does not read.
static bool Build(Built* b)
{
    memset(b, 0, sizeof(*b));
    char error[160] = "";
    FILE* f = fopen(s_path, "wb");
    if (f == nullptr)
    {
        return false;
    }
    fputs(s_text, f);
    fclose(f);
    size_t size = 0;
    bool ok = RootRead(s_path, &b->table, error, sizeof(error)) &&
              (b->unit = CollationRootUnit(&b->table, &size, error, sizeof(error))) != nullptr &&
              mlocReadCollationRoot((DataSpan){b->unit, (uint32_t)size}, &b->root) &&
              CollationMappingsInit(&b->mappings, &b->table);
    if (!ok)
    {
        fprintf(stderr, "  the root: %s\n", error);
        return false;
    }
    b->words = CollationFastLatinTable(&b->mappings, &b->root, &b->count);
    return true;
}

static void Free(Built* b)
{
    free(b->words);
    free(b->unit);
    CollationMappingsFree(&b->mappings);
    RootFree(&b->table);
}

// A character's word, 0 without a table.
static uint32_t Word(const Built* b, uint32_t c)
{
    uint32_t index = c < fastLatinLimit ? c : c - fastPunctStart + fastLatinLimit;
    return b->words == nullptr ? 0 : b->words[fastLatinHeader + index];
}

static void TestBasic(void)
{
    Letters(26, true);
    Built b;
    bool built = Build(&b) && b.words != nullptr;
    static const uint16_t head[] = {0x205, 0xC00, 0xC08, 0xC10, 0xC18};
    CHECK(built && memcmp(b.words, head, sizeof(head)) == 0,
          "the head: version 2, each group's top long mini primary");
    CHECK(built && Word(&b, ' ') == 0xC00 && Word(&b, ',') == 0xC08 && Word(&b, '$') == 0xC18,
          "space, punctuation and currency as long mini primaries");
    CHECK(built && Word(&b, '0') == (fastMinShort | fastCommonSec | fastLowerCase) &&
              Word(&b, '9') > Word(&b, '0') && Word(&b, 'a') > Word(&b, '9') &&
              Word(&b, 'z') > Word(&b, 'a'),
          "digits from the first short primary, its case bits too, then the letters");
    Free(&b);
}

static void TestOverflow(void)
{
    Letters(52, true);
    Built b;
    bool built = Build(&b) && b.words != nullptr;
    CHECK(built && Word(&b, '0') >= fastMinLong && Word(&b, '0') <= fastMaxLong &&
              Word(&b, 'a') >= fastMinShort,
          "62 short primaries overflow: digits take long ones, the letters short");
    Free(&b);
    Letters(64, false);
    CHECK(Build(&b) && b.words == nullptr && b.count == 0,
          "64 letters overflow the short primaries even so: no table");
    Free(&b);
    Letters(59, false);
    bool fit = Build(&b) && b.words != nullptr;
    Free(&b);
    Letters(60, false);
    CHECK(fit && Build(&b) && b.words == nullptr,
          "59 short primaries fit, 60 do not (0x1000 to 0xF800)");
    Free(&b);
    // With digits long, two digits expand; a digit then punctuation, two
    // groups, bails out.
    Letters(52, true);
    Add("00B1;\t[0B 30, 05, 05][0B 32, 05, 05]\n00F7;\t[0B 34, 05, 05][05 40, 05, 05]\n", 0, 0, 0);
    built = Build(&b) && b.words != nullptr;
    CHECK(built && Word(&b, 0xB1) >= fastExpansion && Word(&b, 0xB1) < fastMinLong &&
              Word(&b, 0xF7) == fastBailOut,
          "long digits: two expand, a digit then punctuation bails out");
    Free(&b);
}

static void TestSecondaries(void)
{
    Letters(4, false);
    // Eight signs on a's primary, secondaries 06 to 0D: six fit after
    // the common one.
    for (uint32_t i = 0; i < 8; i++)
    {
        Add("%04X;\t[2A 10, %02X, 05]\n", s_signs[i], 6 + i, 0);
    }
    // Nine on b's primary and secondary, tertiaries 06 to 0E: seven fit.
    for (uint32_t i = 0; i < 9; i++)
    {
        Add("%04X;\t[2A 12, 05, %02X]\n", s_signs[8 + i], 6 + i, 0);
    }
    Built b;
    bool built = Build(&b) && b.words != nullptr;
    bool ok = built;
    for (uint32_t i = 0; ok && i < 8; i++)
    {
        uint32_t w = Word(&b, s_signs[i]);
        ok = i < 6 ? (w & fastShortPrimaryMask) == (Word(&b, 'a') & fastShortPrimaryMask)
                   : w == fastBailOut;
    }
    CHECK(ok, "six secondaries after the common one, the rest bail out");
    ok = built;
    for (uint32_t i = 0; ok && i < 9; i++)
    {
        uint32_t w = Word(&b, s_signs[8 + i]);
        ok = i < 7 ? (w & fastShortPrimaryMask) == (Word(&b, 'b') & fastShortPrimaryMask)
                   : w == fastBailOut;
    }
    CHECK(ok, "seven tertiaries after the common one, the rest bail out");
    Free(&b);
}

static void TestHighSecondaries(void)
{
    Letters(4, false);
    // 22 signs, each a and a secondary element 80 up: twenty of those fit.
    for (uint32_t i = 0; i < 22; i++)
    {
        Add("%04X;\t[2A 10, 05, 05][, %02X, 05]\n", s_signs[i], 0x80 + i, 0);
    }
    Built b;
    bool built = Build(&b) && b.words != nullptr;
    bool ok = built;
    for (uint32_t i = 0; ok && i < 22; i++)
    {
        uint32_t w = Word(&b, s_signs[i]);
        ok = i < 20 ? (w & fastShortPrimaryMask) == (Word(&b, 'a') & fastShortPrimaryMask) &&
                          (w & fastSecondaryMask) >= fastMinSecHigh
                    : w == fastBailOut;
    }
    CHECK(ok, "twenty secondary elements merged into their letter's mini CE, the rest bail out");
    Free(&b);
}

static void TestSecondElements(void)
{
    Letters(4, false);
    Add("%04X;\t[2A 30, 05, 05][05 10, 05, 05]\n", 0xDF, 0, 0);
    Add("%04X;\t[2A 32, 05, 05][2A 34, 05, 05]\n", 0xE6, 0, 0);
    Add("%04X;\t[05 20, 05, 05][07 20, 05, 05]\n", 0xF0, 0, 0);
    Add("%04X;\t[05 22, 05, 05][05 24, 05, 05]\n", 0xF8, 0, 0);
    Add("%04X;\t[05 26, 05, 05][05 28, 06, 05]\n", 0xFE, 0, 0);
    Add("%04X;\t[05 2A, 06, 05]\n", 0x3B, 0, 0);
    Add("%04X;\t[2A 36, 05, 05][2A 38, 05, 03]\n", 0x111, 0, 0);
    Add("%04X;\t[2A 3A, 05, 05][, 05, 05][, 05, 05]\n", 0x127, 0, 0);
    Add("%04X;\t[07 22, 05, 05][05 2C, 05, 05]\n", 0x142, 0, 0);
    Add("%04X;\t[05 2E, 05, 05][2A 3C, 05, 05]\n", 0x14B, 0, 0);
    Built b;
    bool built = Build(&b) && b.words != nullptr;
    uint32_t two = Word(&b, 0xE6);
    uint32_t punct = Word(&b, 0xF8);
    CHECK(built && Word(&b, 0xDF) == fastBailOut && two >= fastExpansion && two < fastMinLong &&
              Word(&b, 0xF0) == fastBailOut && punct >= fastExpansion && punct < fastMinLong,
          "a letter then punctuation, and punctuation then a symbol, bail out; two letters and "
          "two punctuation marks expand");
    CHECK(built && Word(&b, 0xFE) == fastBailOut && Word(&b, ';') == fastBailOut &&
              Word(&b, 0x111) == fastBailOut && Word(&b, 0x127) == fastBailOut,
          "a long primary with an uncommon secondary, first or second, a tertiary below the "
          "common one, and three elements bail out");
    CHECK(built && Word(&b, 0x142) == fastBailOut && Word(&b, 0x14B) == fastBailOut,
          "a symbol then punctuation, and punctuation then a letter, bail out");
    Free(&b);
}

static void TestContexts(void)
{
    Letters(26, false);
    Add("006C | 00B7;\t[, 85, 05]\n00B7;\t[05 30, 05, 05]\n", 0, 0, 0);
    Add("0063 0068;\t[2A 50, 05, 05]\n", 0, 0, 0);
    Add("0064 007A;\t[2A 52, 05, 05]\n0064 007A 0073;\t[2A 54, 05, 05]\n", 0, 0, 0);
    Built b;
    bool built = Build(&b) && b.words != nullptr;
    uint32_t c = Word(&b, 'c');
    uint32_t d = Word(&b, 'd');
    // The list's entries after its default: (character, length) heads.
    bool ch = false;
    bool dz = false;
    if (built && c >= fastContraction && c < fastExpansion && d >= fastContraction &&
        d < fastExpansion)
    {
        uint32_t at = fastLatinHeader + fastLatinChars + (c & fastIndexMask);
        at += b.words[at] >> fastContractionLengthShift;
        ch = (b.words[at] & fastContractionCharMask) == 'h' &&
             b.words[at] >> fastContractionLengthShift == 2;
        at = fastLatinHeader + fastLatinChars + (d & fastIndexMask);
        at += b.words[at] >> fastContractionLengthShift;
        dz = (b.words[at] & fastContractionCharMask) == 'z' &&
             b.words[at] >> fastContractionLengthShift == 1;
    }
    CHECK(built && Word(&b, 0xB7) == fastBailOut &&
              (Word(&b, 'l') < fastContraction || Word(&b, 'l') >= fastExpansion),
          "a character after a prefix bails out; its prefix is no contraction");
    CHECK(ch && dz, "c's list names h; d's bails out on z, which dzs goes on past");
    Free(&b);
}

int main(void)
{
    TestBasic();
    TestOverflow();
    TestSecondaries();
    TestHighSecondaries();
    TestSecondElements();
    TestContexts();
    remove(s_path);
    return s_failures == 0 ? 0 : 1;
}
