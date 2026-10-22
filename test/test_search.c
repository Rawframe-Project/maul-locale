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

// As Finds, walking back from the position.
static bool FindsBack(const char* id, const char* pattern, const char* text, size_t from, int start,
                      int end)
{
    size_t s = 0;
    size_t e = 0;
    if (!Make(id, pattern, mloc_searchExact))
    {
        return false;
    }
    mlocResult r = mlocFindPrevious(&s_searcher, text, strlen(text), from, &s, &e);
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

// A pattern of a string repeated, in UTF-8.
static bool Repeated(const char* id, const char* utf8, size_t count,
                     mlocSearchComparison comparison, mlocResult expected)
{
    static char pattern[2048];
    size_t size = strlen(utf8);
    mlocLocale locale;
    mlocSearchOptions options = {comparison};
    for (size_t i = 0; i < count * size; i++)
    {
        pattern[i] = utf8[i % size];
    }
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
           mlocInitSearcher(&s_searcher, &s_collator, pattern, count * size, &options) == expected;
}

static void TestIdentical(void)
{
    static const uint16_t text[] = {'x', 0x17A2, 'x', 0x17A3};
    static const uint16_t pattern[] = {0x17A3};
    mlocLocale locale;
    size_t s = 0;
    size_t e = 0;
    CHECK(Finds("km-u-ks-identic", "\xE1\x9E\xA3", "x\xE1\x9E\xA2x\xE1\x9E\xA3", 0, 5, 8) &&
              Finds("km-u-ks-level4", "\xE1\x9E\xA3", "x\xE1\x9E\xA2x\xE1\x9E\xA3", 0, 1, 4) &&
              Finds("en-u-ks-identic", "\xC3\xA5", "xa\xCC\x8A", 0, 1, 4),
          "the identical strength compares NFD: Khmer U+17A2 and U+17A3, equal in the "
          "tailoring, differ; a decomposed letter does not");
    CHECK(
        mlocParseLocale("km-u-ks-identic", 15, &locale).status == mloc_success &&
            mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
            mlocInitSearcherUtf16(&s_searcher, &s_collator, pattern, 1, nullptr) == mloc_success &&
            mlocFindNextUtf16(&s_searcher, text, 4, 0, &s, &e) == mloc_success && s == 3 && e == 4,
        "the identical check reads UTF-16 text too");
    CHECK(Repeated("cs", "ch", 241, mloc_searchExact, mloc_success) &&
              Repeated("cs-u-ks-identic", "ch", 240, mloc_searchExact, mloc_success) &&
              Repeated("cs-u-ks-identic", "ch", 241, mloc_searchExact, mloc_errorCapacity),
          "Czech ch is one element and two code points: at the identical strength the "
          "pattern's NFD must fit too");
}

static void TestCapacity(void)
{
    CHECK(Repeated("en", "a", 480, mloc_searchExact, mloc_success) &&
              Repeated("en", "a", 120, mloc_searchPatternBaseWildcard, mloc_success) &&
              Repeated("en", "a", 121, mloc_searchPatternBaseWildcard, mloc_errorCapacity),
          "a searcher's ring holds 512 elements: a pattern's and 32, and 3 per code point "
          "with a wildcard");
    CHECK(Repeated("en", "\xF0\x90\x80\x80", 69, mloc_searchPatternBaseWildcard,
                   mloc_errorCapacity) &&
              Repeated("en", "\xE1\x85\x9E", 54, mloc_searchAnyBaseWildcard, mloc_errorCapacity) &&
              Repeated("en", "\xE3\x86\x86", 54, mloc_searchAnyBaseWildcard, mloc_errorCapacity),
          "with a wildcard a supplementary code point takes 6 places and a leading jamo 8");
}

// A match of 120 letters whose first ones carry four accents each,
// accents in all: a pattern-base match spans 120 + accents elements.
static bool AcrossRing(mlocSearchComparison comparison, size_t accents, mlocResult expected)
{
    static const char marks[4][3] = {"\xCC\x81", "\xCC\x80", "\xCC\x82", "\xCC\x83"};
    static char pattern[120];
    static char text[1200];
    size_t size = 0;
    size_t left = accents;
    memset(pattern, 'a', sizeof pattern);
    for (int letter = 0; letter < 120; letter++)
    {
        text[size++] = 'a';
        for (int m = 0; m < 4 && left > 0; m++, left--)
        {
            text[size++] = marks[m][0];
            text[size++] = marks[m][1];
        }
    }
    mlocLocale locale;
    mlocSearchOptions options = {comparison};
    size_t start = 0;
    size_t end = 0;
    bool made = mlocParseLocale("en", 2, &locale).status == mloc_success &&
                mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
                mlocInitSearcher(&s_searcher, &s_collator, pattern, sizeof pattern, &options) ==
                    mloc_success;
    mlocResult result = mlocFindNext(&s_searcher, text, size, 0, &start, &end);
    return made && result == expected && (expected != mloc_success || (start == 0 && end == size));
}

static void TestRingEdge(void)
{
    // ICU4C 79 finds the 512-element match whole; at 513 it returns one
    // cut at its buffer's end, which the searcher refuses.
    CHECK(AcrossRing(mloc_searchPatternBaseWildcard, 392, mloc_success) &&
              AcrossRing(mloc_searchAnyBaseWildcard, 392, mloc_success) &&
              AcrossRing(mloc_searchPatternBaseWildcard, 393, mloc_errorCapacity) &&
              AcrossRing(mloc_searchAnyBaseWildcard, 393, mloc_errorCapacity),
          "a match fills the ring's 512 elements; one more is refused");
}

static void TestStartContext(void)
{
    // ja's length mark after KA is a prefix rule's, read as A at the
    // first level: a search from the mark keeps KA as its context.
    CHECK(Finds("ja", "\xE3\x83\xBC", "\xE3\x82\xAB\xE3\x83\xBC", 3, -1, -1) &&
              Finds("ja-u-ks-level1", "\xE3\x82\xA2", "\xE3\x82\xAB\xE3\x83\xBC", 3, 3, 6),
          "a search from inside a text reads the prefix before its start, as ICU4C 79 does");
    // An exact-size text searched from its end reads nothing past it.
    static const char text[2] = {'x', 'a'};
    size_t s = 0;
    size_t e = 0;
    CHECK(Make("en", "a", mloc_searchExact) &&
              mlocFindNext(&s_searcher, text, sizeof text, sizeof text, &s, &e) ==
                  mloc_errorNotFound,
          "a search from the text's end finds nothing");
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
    CHECK(Finds("en", "a",
                "xa\x80"
                "a",
                2, 3, 4),
          "a stray continuation byte is a code point of its own: a search from it starts there");
}

static void TestBackward(void)
{
    CHECK(FindsBack("en", "a", "xaba", 4, 3, 4) && FindsBack("en", "a", "xaba", 3, 1, 2) &&
              FindsBack("en", "a", "xaba", 1, -1, -1) && FindsBack("en", "ab", "xAbcab", 6, 4, 6),
          "walking back finds the match nearest before the position");
    CHECK(FindsBack("en-u-ks-level1", "a",
                    "xa\xCC\x81"
                    "b",
                    5, 1, 4) &&
              FindsBack("en-u-ks-level1", "a",
                        "xa\xCC\x81"
                        "b",
                        2, 1, 2),
          "a match takes its accent, but none past the position: a walk from inside a "
          "cluster cuts it there, as ICU4C 79 does");
    CHECK(FindsBack("cs", "c", "cha", 3, -1, -1) && FindsBack("cs", "ch", "acha", 4, 1, 3),
          "walking back, a contraction is one element too");
    static const uint16_t text[] = {'x', 'a', 0xD83D, 0xDE00, 'a'};
    static const uint16_t pattern[] = {'a'};
    mlocLocale locale;
    size_t s = 0;
    size_t e = 0;
    bool utf16 =
        mlocParseLocale("en", 2, &locale).status == mloc_success &&
        mlocInitCollator(&s_collator, &s_data, &locale, nullptr) == mloc_success &&
        mlocInitSearcherUtf16(&s_searcher, &s_collator, pattern, 1, nullptr) == mloc_success &&
        mlocFindPreviousUtf16(&s_searcher, text, 5, 5, &s, &e) == mloc_success && s == 4 &&
        e == 5 && mlocFindPreviousUtf16(&s_searcher, text, 5, 4, &s, &e) == mloc_success &&
        s == 1 && e == 2;
    CHECK(utf16, "UTF-16 positions in units walking back, past a pair");
    static char marks[1 + 2 * 130];
    marks[0] = 'a';
    for (size_t i = 0; i < 130; i++)
    {
        marks[1 + 2 * i] = (char)0xCC;
        marks[2 + 2 * i] = (char)0x81;
    }
    CHECK(Make("en", "b", mloc_searchExact) &&
              mlocFindPrevious(&s_searcher, marks, sizeof marks, sizeof marks, &s, &e) ==
                  mloc_errorCapacity &&
              mlocFindPrevious(&s_searcher, marks, sizeof marks, sizeof marks + 1, &s, &e) ==
                  mloc_errorInvalid &&
              mlocFindPrevious(nullptr, marks, sizeof marks, 0, &s, &e) == mloc_errorInvalid,
          "walking back over more unsafe code points than a piece holds is refused, as are a "
          "position past the end and a null searcher");
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
    TestRingEdge();
    TestStartContext();
    TestUtf16();
    TestBackward();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
