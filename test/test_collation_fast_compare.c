// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin compare (record mloc-0072) against the general path:
// random texts of fast characters, with a few it bails out on, compared
// through mlocCompare must order as their sort keys do (the keys never
// read the table), under tailorings with contractions and under every
// option the fast compare reads; and over the root's table it answers
// every pair of characters the table holds itself, as the general
// compare does, unless the settings make it bail out.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collation_reorder.h"
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

// Characters the root's table holds every one of, in families a pair
// swaps one for another within, so that later levels decide: accents
// and case (e acute's mini CE has the lowest high secondary), AE (an
// expansion at index 0), tab (the lowest long mini CE, variable when
// shifted), DEL (ignorable), spaces of one primary, dashes, U+FFFE and
// U+FFFF, and c and h, a contraction in cs.
static const char* const s_held[][6] = {
    {"e", "\xC3\xA9", "\xC3\xA8", "E", "\xC3\x89", "\xC3\x8A"},
    {"a", "A", "\xC3\xA6", "\xC3\x86", "\xC3\xA0", "\xC3\x80"},
    {"l", "L", "\xC5\x82", "\xC5\x81", "l", "L"},
    {"s", "S", "\xC3\x9F", "s", "S", "\xC5\xA1"},
    {"c", "C", "h", "H", "\xC4\x8D", "\xC4\x8C"},
    {"0", "5", "9", "0", "5", "9"},
    {" ", "\t", "\xE2\x80\x82", "\xE2\x80\x87", "\xE2\x80\xAF", "\x7F"},
    {"-", "\xE2\x80\x90", "\xE2\x80\x93", ",", ".", "'"},
    {"$", "+", "\xE2\x80\xA0", "\xE2\x80\xA2", "\xEF\xBF\xBE", "\xEF\xBF\xBF"}};

static CollationRoot s_root;

enum
{
    families = sizeof(s_held) / sizeof(s_held[0]),
};

// A text of held characters, and the same with one character swapped
// within its family, or a second text of its own every third pair.
static void HeldPair(int i, char* a, size_t* aLength, char* b, size_t* bLength)
{
    uint32_t count = Next(7);
    uint32_t swap = count == 0 ? 0 : Next(count);
    *aLength = 0;
    *bLength = 0;
    for (uint32_t k = 0; k < count; k++)
    {
        uint32_t family = Next(families);
        uint32_t member = Next(6);
        uint32_t other = i % 3 == 0 ? Next(families) : family;
        const char* x = s_held[family][member];
        const char* y = k == swap || i % 3 == 0 ? s_held[other][Next(6)] : x;
        for (; *x != '\0'; x++)
        {
            a[(*aLength)++] = *x;
        }
        for (; *y != '\0'; y++)
        {
            b[(*bLength)++] = *y;
        }
    }
}

// Over held characters under settings and a table: how many pairs the
// fast compare answers, -1 when one answer differs from the general
// compare's, -2 when it does not serve the settings.
static int AnsweredOver(const CollationSettings* s, const CollationRoot* unit)
{
    FastLatinOptions o;
    if (!mlocFastLatinOptions(&s_root, s, unit->fastLatin, unit->fastLatinCount, &o))
    {
        return -2;
    }
    static char a[maxText];
    static char b[maxText];
    int answered = 0;
    for (int i = 0; i < pairs; i++)
    {
        size_t aLength = 0;
        size_t bLength = 0;
        HeldPair(i, a, &aLength, b, &bLength);
        int fast = mlocFastLatinCompare(&o, (const uint8_t*)a, aLength, (const uint8_t*)b, bLength);
        int general = mlocCollationCompare(&s_root, s, (const uint8_t*)a, aLength,
                                           (const uint8_t*)b, bLength);
        if (fast != fastLatinBail && fast != general)
        {
            fprintf(stderr, "  \"%.*s\" and \"%.*s\": fast %d, general %d\n", (int)aLength, a,
                    (int)bLength, b, fast, general);
            return -1;
        }
        answered += fast != fastLatinBail;
    }
    return answered;
}

static int Answered(const CollationSettings* s)
{
    return AnsweredOver(s, &s_root);
}

static void TestAnswers(void)
{
    CollationSettings s = mlocCollationDefaults(&s_root);
    CollationSettings shifted = s;
    shifted.shifted = true;
    shifted.strength = collationQuaternary;
    CollationSettings symbol = shifted;
    symbol.variableTop = mlocCollationLastPrimary(&s_root, collationGroupSymbol);
    CollationSettings level1 = s;
    level1.strength = collationPrimary;
    level1.caseLevel = true;
    CollationSettings level2 = s;
    level2.strength = collationSecondary;
    CollationSettings upper = s;
    upper.caseFirst = collationUpperFirst;
    CollationSettings lower = s;
    lower.caseFirst = collationLowerFirst;
    CollationSettings upperLevel = upper;
    upperLevel.caseLevel = true;
    CHECK(Answered(&s) == pairs && Answered(&shifted) == pairs && Answered(&symbol) == pairs,
          "every pair of held characters answered, as the general compare does: the root's "
          "settings, shifted to punctuation and to symbols");
    CHECK(Answered(&level1) == pairs && Answered(&level2) == pairs && Answered(&upper) == pairs &&
              Answered(&lower) == pairs && Answered(&upperLevel) == pairs,
          "and at the primary strength with the case level, the secondary strength, case first "
          "upper and lower, upper first with the case level");
    CollationSettings numeric = s;
    numeric.numeric = true;
    CollationSettings backwards = s;
    backwards.backwards = true;
    int digits = Answered(&numeric);
    int french = Answered(&backwards);
    FastLatinOptions plain;
    FastLatinOptions french2;
    bool served =
        mlocFastLatinOptions(&s_root, &s, s_root.fastLatin, s_root.fastLatinCount, &plain) &&
        mlocFastLatinOptions(&s_root, &backwards, s_root.fastLatin, s_root.fastLatinCount,
                             &french2);
    const uint8_t* e = (const uint8_t*)"e";
    const uint8_t* acute = (const uint8_t*)"\xC3\xA9";
    CHECK(digits > 0 && digits < pairs && french > 0 && french < pairs && served &&
              mlocFastLatinCompare(&plain, e, 1, acute, 2) == -1 &&
              mlocFastLatinCompare(&french2, e, 1, acute, 2) == fastLatinBail,
          "numeric ordering bails out on digits, backwards secondaries on secondaries that "
          "differ (e and e acute); the rest answered");
    // U+FFFE ranks above every variable at the quaternary level, as the
    // general compare and the sort keys have it; ICU4C's fast compare
    // puts it lowest.
    FastLatinOptions q;
    const uint8_t* a = (const uint8_t*)" \xE2\x80\x92\xEF\xBF\xBE";
    const uint8_t* b = (const uint8_t*)"\xC2\xA0\xEF\xBF\xBE\xE2\x80\x92";
    CHECK(mlocFastLatinOptions(&s_root, &shifted, s_root.fastLatin, s_root.fastLatinCount, &q) &&
              mlocFastLatinCompare(&q, a, 7, b, 8) == -1 &&
              mlocCollationCompare(&s_root, &shifted, a, 7, b, 8) == -1,
          "U+FFFE above a variable at the quaternary level");
    CollationSettings custom = shifted;
    custom.variableTop = 0x0A000000;
    CollationSettings identical = s;
    identical.strength = collationIdentical;
    FastLatinOptions o;
    CHECK(Answered(&custom) == -2 && Answered(&identical) == -2 &&
              !mlocFastLatinOptions(&s_root, &s, nullptr, 0, &o) &&
              !mlocFastLatinOptions(&s_root, &s, s_root.fastLatin,
                                    fastLatinHeader + fastLatinChars - 1, &o),
          "none for a variable top that ends no group, the identical strength, or no table");
}

// Over cs's own table, whose c starts ch: c's list read before every
// held character, punctuation and U+FFFE among them.
static void TestTailoredAnswers(void)
{
    DataView view = mlocViewOf(&s_sources[1]);
    uint32_t locale = 0;
    DataSpan unit = {0};
    DataSpan body = {0};
    CollationTailoring cs;
    bool found = mlocFindDataLocale(&view, "cs", 2, &locale) &&
                 mlocFindDataUnit(&view, kindCollationTailorings, locale, &unit);
    if (found)
    {
        CollationTypes types = mlocReadCollationTypes(unit);
        found = mlocFindCollationType(&types, "standard", 8, &body) &&
                mlocReadCollationTailoring(body, &cs) && cs.view.fastLatin != nullptr;
    }
    CollationSettings s = mlocCollationDefaults(&s_root);
    s.tailoring = found ? &cs.view : nullptr;
    CollationSettings shifted = s;
    shifted.shifted = true;
    shifted.strength = collationQuaternary;
    CHECK(found && AnsweredOver(&s, &cs.view) == pairs && AnsweredOver(&shifted, &cs.view) == pairs,
          "cs's table answers every pair, ch among them, as the general compare does");
}

// Reorderings (script codes as four letters, Grek and Latn): one that
// leaves the groups before Latin in their order is
// served; digits after Latin bail out; Latin before the groups is not.
static void TestReorderings(void)
{
    static CollationReorder greek;
    static CollationReorder digitsLast;
    static CollationReorder latinFirst;
    static const uint32_t greekCodes[] = {0x4772656Bu};
    static const uint32_t digitCodes[] = {0x4C61746Eu, collationGroupDigit};
    static const uint32_t latinCodes[] = {0x4C61746Eu, collationGroupSpace};
    bool changes = false;
    bool made =
        mlocCollationMakeReorder(&s_root, greekCodes, 1, &greek, &changes) == mloc_success &&
        mlocCollationMakeReorder(&s_root, digitCodes, 2, &digitsLast, &changes) == mloc_success &&
        mlocCollationMakeReorder(&s_root, latinCodes, 2, &latinFirst, &changes) == mloc_success;
    CollationSettings s = mlocCollationDefaults(&s_root);
    CollationSettings g = s;
    g.reorder = &greek;
    CollationSettings d = s;
    d.reorder = &digitsLast;
    CollationSettings l = s;
    l.reorder = &latinFirst;
    int digits = made ? Answered(&d) : 0;
    // Superscript three shares 3's primary: with digits after Latin it
    // bails out too, where ICU4C orders it before Z.
    FastLatinOptions o;
    bool served =
        made && mlocFastLatinOptions(&s_root, &d, s_root.fastLatin, s_root.fastLatinCount, &o);
    CHECK(served &&
              mlocFastLatinCompare(&o, (const uint8_t*)"Z", 1, (const uint8_t*)"\xC2\xB3", 2) ==
                  fastLatinBail &&
              mlocFastLatinCompare(&o, (const uint8_t*)"Z", 1, (const uint8_t*)"Y", 1) == 1 &&
              mlocCollationCompare(&s_root, &d, (const uint8_t*)"Z", 1, (const uint8_t*)"\xC2\xB3",
                                   2) == -1,
          "a superscript digit bails out under digits after Latin, which order it after Z");
    CHECK(made && Answered(&g) == pairs && digits > 0 && digits < pairs && Answered(&l) == -2,
          "Greek first served; digits after Latin bail out on digits; Latin before spaces not "
          "served");
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
    DataView view = mlocViewOf(&s_sources[1]);
    DataSpan span = {0};
    if (!mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &s_root))
    {
        fputs("test_collation_fast_compare: no root\n", stderr);
        return 2;
    }
    TestTailorings();
    TestOptions();
    TestAnswers();
    TestTailoredAnswers();
    TestReorderings();
    return s_failures == 0 ? 0 : 1;
}
