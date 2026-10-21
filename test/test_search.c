// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search (record mloc-0074) over the collation data file, as
// ICU4C's usearch finds: a match at the strength, accents and case
// ignored or not; a contraction never split; a match's ends on grapheme
// boundaries; the wildcard comparisons; a start inside the text backing
// up over a contraction and keeping a prefix's context; the identical
// strength asking for the same NFD; UTF-8 and UTF-16 alike; the
// refusals. conform_search checks random patterns and texts against
// ICU4C 79's usearch_search.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/search.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
static mlocCollator s_collator;
static mlocSearcher s_searcher;

static bool Make(const char* id, const char* pattern, mlocSearchComparison comparison)
{
    mlocLocale locale;
    mlocSearchOptions options = {comparison};
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
           mlocInitSearcher(&s_searcher, &s_collator, pattern, strlen(pattern), &options) ==
               mloc_success;
}

// Whether the pattern is found in the text from a position with these
// bounds (start -1 for none).
static bool Finds(const char* id, const char* pattern, const char* text, size_t from, int start,
                  int end)
{
    size_t s = 0;
    size_t e = 0;
    if (!Make(id, pattern, mloc_searchExact))
    {
        return false;
    }
    mlocResult r = mlocFindNext(&s_searcher, text, strlen(text), from, &s, &e);
    return start < 0 ? r == mloc_errorNotFound
                     : r == mloc_success && s == (size_t)start && e == (size_t)end;
}

static void TestStrengths(void)
{
    CHECK(Finds("en", "ab", "xAbcab", 0, 4, 6) && Finds("en-u-ks-level2", "ab", "xAbcab", 0, 1, 3),
          "case counts at the tertiary strength, not at the secondary");
    CHECK(Finds("en-u-ks-level1", "a",
                "x\xC3\xA1"
                "b",
                0, 1, 3) &&
              Finds("en", "a", "x\xC3\xA1", 0, -1, -1),
          "a primary search finds a in a precomposed a-acute, a tertiary one does not");
    CHECK(Finds("en-u-ks-level1", "a", "xa\xCC\x81", 0, 1, 4),
          "a match ends on the grapheme boundary after its accent");
    CHECK(Finds("en-u-ks-level1", "resume", "a R\xC3\xA9sum\xC3\xA9!", 0, 2, 10),
          "a word found whatever its accents and case");
}

static void TestContractions(void)
{
    CHECK(Finds("cs", "c", "cha", 0, -1, -1) && Finds("cs", "ch", "acha", 0, 1, 3) &&
              Finds("en", "c", "cha", 0, 0, 1),
          "Czech ch is one letter: c is not found in it");
    CHECK(Finds("cs", "h", "cha", 1, -1, -1) && Finds("cs", "a", "cha", 1, 2, 3),
          "a start inside ch backs up over it");
}

static void TestComparisons(void)
{
    size_t s = 0;
    size_t e = 0;
    bool any = Make("en", "a", mloc_searchPatternBaseWildcard) &&
               mlocFindNext(&s_searcher, "x\xC3\xA1", 3, 0, &s, &e) == mloc_success && s == 1 &&
               e == 3;
    bool pattern = Make("en", "\xC3\xA1", mloc_searchPatternBaseWildcard) &&
                   mlocFindNext(&s_searcher, "xa", 2, 0, &s, &e) == mloc_errorNotFound;
    bool both = Make("en", "\xC3\xA1", mloc_searchAnyBaseWildcard) &&
                mlocFindNext(&s_searcher, "xa", 2, 0, &s, &e) == mloc_success && s == 1 && e == 2;
    CHECK(any && pattern && both,
          "a pattern's base letter takes any accent; with any-base the text's takes the "
          "pattern's too");
    bool accent = Make("en", "\xC3\xA1", mloc_searchPatternBaseWildcard) &&
                  mlocFindNext(&s_searcher, "x\xC3\xA0", 3, 0, &s, &e) == mloc_errorNotFound &&
                  mlocFindNext(&s_searcher, "x\xC3\xA1", 3, 0, &s, &e) == mloc_success && s == 1 &&
                  e == 3;
    bool upper = Make("en", "A", mloc_searchPatternBaseWildcard) &&
                 mlocFindNext(&s_searcher, "xa", 2, 0, &s, &e) == mloc_errorNotFound &&
                 mlocFindNext(&s_searcher, "xA", 2, 0, &s, &e) == mloc_success && s == 1 && e == 2;
    CHECK(accent && upper, "a pattern's own accent or case is no wildcard");
}

static void TestIdentical(void)
{
    static const uint16_t ring[] = {'x', 'a', 0x030A};
    static const uint16_t ringPattern[] = {0x00E5};
    static const uint16_t control[] = {'x', 'a', 0x0001, 'b'};
    static const uint16_t controlPattern[] = {'a', 'b'};
    mlocLocale locale;
    size_t s = 0;
    size_t e = 0;
    CHECK(Finds("en-u-ks-identic", "ab",
                "xa\x01"
                "b",
                0, -1, -1) &&
              Finds("en", "ab",
                    "xa\x01"
                    "b",
                    0, 1, 4) &&
              Finds("en-u-ks-identic", "\xC3\xA5", "xa\xCC\x8A", 0, 1, 4),
          "the identical strength compares NFD: an ignorable control counts, a decomposed "
          "letter does not");
    bool made = mlocParseLocale("en-u-ks-identic", 15, &locale).status == mloc_success &&
                mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success;
    bool composed =
        made &&
        mlocInitSearcherUtf16(&s_searcher, &s_collator, ringPattern, 1, nullptr) == mloc_success &&
        mlocFindNextUtf16(&s_searcher, ring, 3, 0, &s, &e) == mloc_success && s == 1 && e == 3;
    bool ignorable = made &&
                     mlocInitSearcherUtf16(&s_searcher, &s_collator, controlPattern, 2, nullptr) ==
                         mloc_success &&
                     mlocFindNextUtf16(&s_searcher, control, 4, 0, &s, &e) == mloc_errorNotFound;
    CHECK(composed && ignorable, "the identical check reads UTF-16 text too");
    static char longPattern[482];
    memset(longPattern, 1, sizeof(longPattern) - 1);
    longPattern[0] = 'a';
    bool tertiary = Make("en", longPattern, mloc_searchExact);
    bool identical = mlocParseLocale("en-u-ks-identic", 15, &locale).status == mloc_success &&
                     mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
                     mlocInitSearcher(&s_searcher, &s_collator, longPattern,
                                      sizeof(longPattern) - 1, nullptr) == mloc_errorCapacity;
    CHECK(tertiary && identical,
          "a pattern of one element and many ignorables fits, but not its NFD at the identical "
          "strength");
}

// A pattern of a code point repeated, in UTF-8.
static bool Repeated(const char* utf8, size_t count, mlocSearchComparison comparison,
                     mlocResult expected)
{
    static char pattern[2048];
    size_t size = strlen(utf8);
    mlocLocale locale;
    mlocSearchOptions options = {comparison};
    for (size_t i = 0; i < count * size; i++)
    {
        pattern[i] = utf8[i % size];
    }
    return mlocParseLocale("en", 2, &locale).status == mloc_success &&
           mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
           mlocInitSearcher(&s_searcher, &s_collator, pattern, count * size, &options) == expected;
}

static void TestCapacity(void)
{
    CHECK(Repeated("a", 480, mloc_searchExact, mloc_success) &&
              Repeated("a", 120, mloc_searchPatternBaseWildcard, mloc_success) &&
              Repeated("a", 121, mloc_searchPatternBaseWildcard, mloc_errorCapacity),
          "a searcher's ring holds 512 elements: a pattern's and 32, and 3 per code point "
          "with a wildcard");
    CHECK(Repeated("\xF0\x90\x80\x80", 69, mloc_searchPatternBaseWildcard, mloc_errorCapacity) &&
              Repeated("\xE1\x85\x9E", 54, mloc_searchAnyBaseWildcard, mloc_errorCapacity) &&
              Repeated("\xE3\x86\x86", 54, mloc_searchAnyBaseWildcard, mloc_errorCapacity),
          "with a wildcard a supplementary code point takes 6 places and a leading jamo 8");
}

static void TestUtf16(void)
{
    static const uint16_t text[] = {'x', 0x00E1, 'b', 'a', 0x0301, 0xD83D, 0xDE00, 'a'};
    static const uint16_t pattern[] = {'a'};
    mlocLocale locale;
    size_t s = 0;
    size_t e = 0;
    bool ok =
        mlocParseLocale("en-u-ks-level1", 14, &locale).status == mloc_success &&
        mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
        mlocInitSearcherUtf16(&s_searcher, &s_collator, pattern, 1, nullptr) == mloc_success &&
        mlocFindNextUtf16(&s_searcher, text, 8, 0, &s, &e) == mloc_success && s == 1 && e == 2 &&
        mlocFindNextUtf16(&s_searcher, text, 8, 2, &s, &e) == mloc_success && s == 3 && e == 5 &&
        mlocFindNextUtf16(&s_searcher, text, 8, 6, &s, &e) == mloc_success && s == 7 && e == 8;
    CHECK(ok, "UTF-16 positions in units; a position inside a pair counts as its start");
    static const uint16_t pair[] = {'x', 0xD83D, 0xDE00};
    static const uint16_t lone[] = {0xD83D, 0xE000};
    bool inside =
        mlocInitSearcherUtf16(&s_searcher, &s_collator, pair + 1, 2, nullptr) == mloc_success &&
        mlocFindNextUtf16(&s_searcher, pair, 3, 2, &s, &e) == mloc_success && s == 1 && e == 3;
    bool alone =
        mlocInitSearcherUtf16(&s_searcher, &s_collator, lone, 1, nullptr) == mloc_success &&
        mlocFindNextUtf16(&s_searcher, lone, 2, 0, &s, &e) == mloc_success && s == 0 && e == 1 &&
        mlocFindNextUtf16(&s_searcher, lone, 2, 1, &s, &e) == mloc_errorNotFound;
    CHECK(inside && alone, "a pair is found from its trail; a lone lead before U+E000 is no pair");
    bool bytes = Make("en", "\xF0\x9F\x98\x80", mloc_searchExact);
    for (size_t from = 2; from <= 4; from++)
    {
        bytes = bytes &&
                mlocFindNext(&s_searcher,
                             "x\xF0\x9F\x98\x80"
                             "a",
                             6, from, &s, &e) == mloc_success &&
                s == 1 && e == 5;
    }
    bytes = bytes && mlocFindNext(&s_searcher,
                                  "x\xF0\x9F\x98\x80"
                                  "a",
                                  6, 5, &s, &e) == mloc_errorNotFound;
    CHECK(bytes, "a UTF-8 position inside a sequence counts as its start, after it does not");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    size_t s = 0;
    size_t e = 0;
    bool made = mlocParseLocale("en", 2, &locale).status == mloc_success &&
                mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success;
    mlocSearchOptions bad = {(mlocSearchComparison)3};
    CHECK(made && mlocInitSearcher(&s_searcher, &s_collator, "", 0, nullptr) == mloc_errorInvalid &&
              mlocInitSearcher(&s_searcher, &s_collator, "a", 1, &bad) == mloc_errorInvalid &&
              mlocInitSearcher(&s_searcher, &s_collator, nullptr, 1, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitSearcher(nullptr, &s_collator, "a", 1, nullptr) == mloc_errorInvalid,
          "an empty pattern, an unknown comparison and null pointers are refused");
    bool numeric = mlocParseLocale("en-u-kn", 7, &locale).status == mloc_success &&
                   mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
                   mlocInitSearcher(&s_searcher, &s_collator, "1", 1, nullptr) == mloc_errorInvalid;
    bool shifted = mlocParseLocale("en-u-ka-shifted", 15, &locale).status == mloc_success &&
                   mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
                   mlocInitSearcher(&s_searcher, &s_collator, " ", 1, nullptr) == mloc_errorInvalid;
    CHECK(numeric && shifted,
          "numeric ordering and a pattern of variable elements only when shifted are refused");
    static char longPattern[600];
    memset(longPattern, 'a', sizeof(longPattern));
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
              mlocInitSearcher(&s_searcher, &s_collator, longPattern, sizeof(longPattern),
                               nullptr) == mloc_errorCapacity,
          "a pattern of more elements than a searcher holds");
    mlocSearcher zero = {0};
    CHECK(Make("en", "a", mloc_searchExact) &&
              mlocFindNext(&s_searcher, "a", 1, 2, &s, &e) == mloc_errorInvalid &&
              mlocFindNext(&s_searcher, nullptr, 1, 0, &s, &e) == mloc_errorInvalid &&
              mlocFindNext(&s_searcher, "a", 1, 0, nullptr, &e) == mloc_errorInvalid &&
              mlocFindNext(&zero, "a", 1, 0, &s, &e) == mloc_errorInvalid &&
              mlocFindNext(&s_searcher, nullptr, 0, 0, &s, &e) == mloc_errorNotFound,
          "a position past the end, null pointers and a searcher not made are refused; an "
          "empty text has no match");
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
        fputs("test_search: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    TestStrengths();
    TestContractions();
    TestComparisons();
    TestIdentical();
    TestCapacity();
    TestUtf16();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
