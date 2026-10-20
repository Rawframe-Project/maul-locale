// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin compare (record mloc-0072) against the general path:
// random texts of fast characters, with a few it bails out on, compared
// through mlocCompare must order as their sort keys do (the keys never
// read the table), under tailorings with contractions and under every
// option the fast compare reads; and over the root's table it answers
// most such pairs itself, as the general compare does.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"

#include "maul-locale/collate.h"
#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxText = 12 * 3,
    pairs = 2000,
};

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
static uint64_t s_seed = 0x9E3779B97F4A7C15ULL;

// UTF-8 of the characters texts are made of: letters that contract in
// some tailoring, accented and special letters, punctuation, digits,
// spaces, and a few the table bails out on (a combining acute, Cyrillic
// a, the middle dot after l, U+FFFE).
static const char* const s_alphabet[] = {"a",
                                         "b",
                                         "c",
                                         "d",
                                         "e",
                                         "g",
                                         "h",
                                         "i",
                                         "l",
                                         "n",
                                         "o",
                                         "q",
                                         "r",
                                         "s",
                                         "t",
                                         "u",
                                         "y",
                                         "z",
                                         "A",
                                         "C",
                                         "D",
                                         "H",
                                         "L",
                                         "S",
                                         "Z",
                                         "0",
                                         "1",
                                         "9",
                                         " ",
                                         "-",
                                         ",",
                                         ".",
                                         "'",
                                         "$",
                                         "\xC3\xA9",
                                         "\xC3\xA5",
                                         "\xC3\xA4",
                                         "\xC3\xB6",
                                         "\xC3\xBC",
                                         "\xC3\x9F",
                                         "\xC3\xA6",
                                         "\xC3\xB8",
                                         "\xC3\xB1",
                                         "\xC3\xA7",
                                         "\xC3\x89",
                                         "\xC4\x8D",
                                         "\xC5\x82",
                                         "\xC5\x91",
                                         "\xC4\xB1",
                                         "\xC5\x8B",
                                         "\xC4\xB3",
                                         "\xE2\x80\x90",
                                         "\xE2\x80\x99",
                                         "\xE2\x80\xA6",
                                         "\xCC\x81",
                                         "\xD0\xB0",
                                         "\xC2\xB7",
                                         "\xEF\xBF\xBE"};

static uint32_t Next(uint32_t bound)
{
    s_seed ^= s_seed << 13;
    s_seed ^= s_seed >> 7;
    s_seed ^= s_seed << 17;
    return (uint32_t)(s_seed % bound);
}

static size_t Text(char* out)
{
    size_t length = 0;
    uint32_t count = Next(9);
    size_t letters = sizeof(s_alphabet) / sizeof(s_alphabet[0]);
    for (uint32_t i = 0; i < count; i++)
    {
        for (const char* c = s_alphabet[Next((uint32_t)letters)]; *c != '\0'; c++)
        {
            out[length++] = *c;
        }
    }
    return length;
}

static int Sign(int value)
{
    return (value > 0) - (value < 0);
}

// The order of two keys as memcmp gives it, the shorter first on a tie.
static int KeyOrder(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    int c = memcmp(a, b, aLength < bLength ? aLength : bLength);
    return c != 0 ? Sign(c) : (aLength > bLength) - (aLength < bLength);
}

// Whether every pair orders by compare as by keys under the tag.
static bool Agrees(const char* tag)
{
    mlocLocale locale;
    mlocCollator c;
    if (mlocParseLocale(tag, strlen(tag), &locale).status != mloc_success ||
        mlocInitCollator(&c, &s_data, &locale, nullptr) != mloc_success)
    {
        fprintf(stderr, "  %s: no collator\n", tag);
        return false;
    }
    static char a[maxText];
    static char b[maxText];
    static uint8_t ka[1024];
    static uint8_t kb[1024];
    for (int i = 0; i < pairs; i++)
    {
        size_t aLength = Text(a);
        size_t bLength = Text(b);
        if (i % 2 == 1)
        {
            // b as a with one character's bytes changed, so that later
            // levels decide.
            memcpy(b, a, aLength);
            bLength = aLength;
            if (aLength > 0)
            {
                b[Next((uint32_t)aLength)] = s_alphabet[Next(30)][0];
            }
        }
        int order = 2;
        size_t na = 0;
        size_t nb = 0;
        bool made = mlocCompare(&c, a, aLength, b, bLength, &order) == mloc_success &&
                    mlocGetSortKey(&c, a, aLength, ka, sizeof(ka), &na) == mloc_success &&
                    mlocGetSortKey(&c, b, bLength, kb, sizeof(kb), &nb) == mloc_success;
        if (!made || order != KeyOrder(ka, na, kb, nb))
        {
            fprintf(stderr, "  %s: \"%.*s\" and \"%.*s\": compare %d\n", tag, (int)aLength, a,
                    (int)bLength, b, order);
            return false;
        }
    }
    return true;
}

static void TestTailorings(void)
{
    static const char* const tags[] = {
        "und", "en", "cs", "da",    "de-u-co-phonebk", "es-u-co-trad",  "hu", "vi-u-co-trad", "sv",
        "pl",  "lt", "cy", "fr-CA", "en-US-POSIX",     "de-u-co-search"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++)
    {
        ok = Agrees(tags[i]) && ok;
    }
    CHECK(ok, "tailorings with contractions, French backwards, POSIX and search without a table");
}

static void TestOptions(void)
{
    static const char* const tags[] = {"en-u-ka-shifted",
                                       "en-u-ka-shifted-kv-space",
                                       "en-u-ka-shifted-kv-symbol",
                                       "en-u-ka-shifted-kv-currency",
                                       "en-u-ks-level1",
                                       "en-u-ks-level2",
                                       "en-u-ks-level4",
                                       "en-u-ka-shifted-ks-level4",
                                       "en-u-kf-upper",
                                       "en-u-kf-lower",
                                       "en-u-kc",
                                       "en-u-kc-ks-level1",
                                       "en-u-kc-kf-upper",
                                       "en-u-kb",
                                       "en-u-kn",
                                       "en-u-kr-latn-digit",
                                       "en-u-kr-grek-latn",
                                       "en-u-kr-punct-latn",
                                       "da-u-kf-lower",
                                       "en-u-ks-identic"};
    bool ok = true;
    for (size_t i = 0; i < sizeof(tags) / sizeof(tags[0]); i++)
    {
        ok = Agrees(tags[i]) && ok;
    }
    CHECK(ok, "every option the fast compare reads: shifted to each group, strengths, case "
              "first and level, backwards, numeric, reorderings, identical");
}

// Over the root's table with the root's settings, the fast compare
// answers most pairs of fast characters and agrees with the general one.
static void TestAnswers(void)
{
    DataView view = mlocViewOf(&s_sources[1]);
    DataSpan span = {0};
    CollationRoot root;
    FastLatinOptions o;
    CollationSettings s = {0};
    bool ready = mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) &&
                 mlocReadCollationRoot(span, &root);
    if (ready)
    {
        s = mlocCollationDefaults(&root);
        ready = mlocFastLatinOptions(&root, &s, root.fastLatin, root.fastLatinCount, &o);
    }
    CHECK(ready, "the root's table serves the root's settings");
    if (!ready)
    {
        return;
    }
    static char a[maxText];
    static char b[maxText];
    int answered = 0;
    bool same = true;
    for (int i = 0; i < pairs; i++)
    {
        size_t aLength = Text(a);
        size_t bLength = Text(b);
        int fast = mlocFastLatinCompare(&o, (const uint8_t*)a, aLength, (const uint8_t*)b, bLength);
        int general =
            mlocCollationCompare(&root, &s, (const uint8_t*)a, aLength, (const uint8_t*)b, bLength);
        answered += fast != fastLatinBail;
        same = same && (fast == fastLatinBail || fast == general);
    }
    CHECK(same && answered > pairs / 4, "the fast compare answers as the general one, often");
    s.strength = collationIdentical;
    CHECK(!mlocFastLatinOptions(&root, &s, root.fastLatin, root.fastLatinCount, &o) &&
              !mlocFastLatinOptions(&root, &(CollationSettings){.strength = collationTertiary},
                                    nullptr, 0, &o),
          "no fast compare at the identical strength or without a table");
}

int main(void)
{
    FILE* f = fopen(MLOC_COLLATION_FILE, "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(&s_sources[1], s_file, size) != mloc_success)
    {
        fputs("test_collation_fast_compare: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    TestTailorings();
    TestOptions();
    TestAnswers();
    return s_failures == 0 ? 0 : 1;
}
