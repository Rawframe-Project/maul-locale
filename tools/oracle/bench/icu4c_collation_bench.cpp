// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// ICU4C baseline for Maul Locale: collation's work, the same
// inputs as maul-locale's bench/bench_collation.c. Best of five runs after
// a warm-up of a tenth, nanoseconds per call. Collators, the index and
// the UTF-16 forms are made before the timing. A compare walks 64 pairs
// of eight words in a fixed order (i = (i + 3) % 64, the pair i / 8 and
// i % 8), as the library's benchmark case does. ICU's sort keys take UTF-16 only,
// so its key case reads the words' UTF-16 forms. Given a case name, only
// that case is timed.
//   g++ -O2 -I$ICU/include icu4c_collation_bench.cpp -L$ICU/lib \
//       -licui18n -licuuc -licudata -Wl,-rpath,$ICU/lib -o icu4c_collation
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unicode/alphaindex.h>
#include <unicode/coll.h>
#include <unicode/stsearch.h>
#include <unicode/tblcoll.h>
#include <unicode/ucol.h>
#include <unicode/unistr.h>
#include <unicode/usearch.h>
#include <unicode/ustring.h>

using namespace icu;

static volatile int sink;

static double now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static const char* only;

template <typename F> static void Time(const char* name, long iters, F body)
{
    if (only != nullptr && strcmp(only, name) != 0)
    {
        return;
    }
    for (long i = 0; i < iters / 10; ++i)
    {
        body();
    }
    double best = 1e30;
    for (int run = 0; run < 5; ++run)
    {
        double start = now();
        for (long i = 0; i < iters; ++i)
        {
            body();
        }
        double ns = (now() - start) / (double)iters;
        best = ns < best ? ns : best;
    }
    printf("%-26s %10.1f ns\n", name, best);
}

// The words of each case; the same in bench_collation.c.
static const char* const s_de[8] = {"\xC3\x84pfel",
                                    "apfel",
                                    "Zebra",
                                    "\xC3\xBC"
                                    "ber",
                                    "Ufer",
                                    "Stra\xC3\x9F"
                                    "e",
                                    "Strasse",
                                    "\xC3\x96l"};
static const char* const s_fr[8] = {
    "c\xC3\xB4te", "cot\xC3\xA9",         "c\xC3\xB4t\xC3\xA9",  "cote",
    "Cote",        "\xC3\xA9l\xC3\xA8ve", "\xC3\xA9lev\xC3\xA9", "\xC3\x89lise"};
static const char* const s_zh[8] = {"\xE5\x8C\x97\xE4\xBA\xAC", "\xE4\xB8\x8A\xE6\xB5\xB7",
                                    "\xE5\xB9\xBF\xE5\xB7\x9E", "\xE6\xB7\xB1\xE5\x9C\xB3",
                                    "\xE4\xB8\xAD\xE5\x9B\xBD", "\xE4\xBA\xBA\xE6\xB0\x91",
                                    "\xE9\x98\xBF\xE5\xA7\xA8", "\xE4\xB8\x80\xE4\xB8\xAA"};
static const char* const s_ko[8] = {"\xEA\xB0\x80\xEB\x82\x98", "\xED\x95\x9C\xEA\xB5\xAD",
                                    "\xEC\x84\x9C\xEC\x9A\xB8", "\xEB\xB6\x80\xEC\x82\xB0",
                                    "\xEA\xB9\x80\xEC\xB9\x98", "\xEC\x82\xAC\xEB\x9E\x8C",
                                    "\xED\x95\x99\xEA\xB5\x90", "\xEA\xB0\x80\xEC\xA1\xB1"};
// Eight file names sharing a 32-byte prefix, differing at its end.
static const char* const s_prefix[8] = {"Rechnungen/2026/Oktober/Kunde-A-\xC3\x84pfel",
                                        "Rechnungen/2026/Oktober/Kunde-A-apfel",
                                        "Rechnungen/2026/Oktober/Kunde-A-Zebra",
                                        "Rechnungen/2026/Oktober/Kunde-A-\xC3\xBC"
                                        "ber",
                                        "Rechnungen/2026/Oktober/Kunde-A-Ufer",
                                        "Rechnungen/2026/Oktober/Kunde-A-Stra\xC3\x9F"
                                        "e",
                                        "Rechnungen/2026/Oktober/Kunde-A-Strasse",
                                        "Rechnungen/2026/Oktober/Kunde-A-\xC3\x96l"};

static void Compare(const char* name, const char* locale, const char* const* words)
{
    UErrorCode e = U_ZERO_ERROR;
    UCollator* col = ucol_open(locale, &e);
    int32_t lens[8];
    for (int k = 0; k < 8; ++k)
    {
        lens[k] = (int32_t)strlen(words[k]);
    }
    unsigned i = 0;
    Time(name, 5000000,
         [&]
         {
             i = (i + 3) % 64;
             UErrorCode s = U_ZERO_ERROR;
             sink = ucol_strcollUTF8(col, words[i / 8], lens[i / 8], words[i % 8], lens[i % 8], &s);
         });
    ucol_close(col);
}

int main(int argc, char** argv)
{
    only = argc > 1 ? argv[1] : nullptr;
    UErrorCode e = U_ZERO_ERROR;
    Time("collator construct de", 20000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             UCollator* c = ucol_open("de", &s);
             ucol_close(c);
         });
    Time("collator construct zh", 20000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             UCollator* c = ucol_open("zh", &s);
             ucol_close(c);
         });
    Compare("compare de", "de", s_de);
    Compare("compare fr", "fr", s_fr);
    Compare("compare zh", "zh", s_zh);
    Compare("compare ko", "ko", s_ko);
    Compare("compare long prefix de", "de", s_prefix);

    UCollator* de = ucol_open("de", &e);
    UChar u16[8][64];
    int32_t n16[8];
    for (int k = 0; k < 8; ++k)
    {
        u_strFromUTF8(u16[k], 64, &n16[k], s_de[k], -1, &e);
    }
    unsigned i = 0;
    Time("compare de UTF-16", 5000000,
         [&]
         {
             i = (i + 3) % 64;
             sink = ucol_strcoll(de, u16[i / 8], n16[i / 8], u16[i % 8], n16[i % 8]);
         });
    uint8_t key[256];
    Time("sort key de", 5000000,
         [&]
         {
             i = (i + 1) % 8;
             sink = ucol_getSortKey(de, u16[i], n16[i], key, (int32_t)sizeof(key));
         });

    AlphabeticIndex* builder = new AlphabeticIndex(Locale("de"), e);
    Time("index build de", 2000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             AlphabeticIndex::ImmutableIndex* made = builder->buildImmutableIndex(s);
             sink = made->getBucketCount();
             delete made;
         });
    AlphabeticIndex::ImmutableIndex* index = builder->buildImmutableIndex(e);
    UnicodeString names[8];
    for (int k = 0; k < 8; ++k)
    {
        names[k] = UnicodeString::fromUTF8(s_de[k]);
    }
    Time("index bucket de", 2000000,
         [&]
         {
             i = (i + 1) % 8;
             UErrorCode s = U_ZERO_ERROR;
             sink = index->getBucketIndex(names[i], s);
         });

    // A word found at primary strength in a sentence, its last match
    // near the end; the search object made before the timing.
    UnicodeString text = UnicodeString::fromUTF8("Die Stra\xC3\x9F"
                                                 "e am Ufer f\xC3\xBChrt \xC3\xBC"
                                                 "ber die Br\xC3\xBC"
                                                 "cke zur STRASSE");
    UnicodeString pattern = UnicodeString::fromUTF8("strasse");
    RuleBasedCollator* searchCollator =
        (RuleBasedCollator*)Collator::createInstance(Locale("de"), e);
    searchCollator->setStrength(Collator::PRIMARY);
    StringSearch search(pattern, text, searchCollator, nullptr, e);
    Time("search first de", 1000000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             search.reset();
             sink = search.first(s);
         });
    Time("search all de", 500000,
         [&]
         {
             UErrorCode s = U_ZERO_ERROR;
             search.reset();
             int found = 0;
             for (int32_t at = search.first(s); at != USEARCH_DONE; at = search.next(s))
             {
                 found++;
             }
             sink = found;
         });
    printf("  status %s\n", u_errorName(e));
    delete index;
    delete builder;
    delete searchCollator;
    ucol_close(de);
    return 0;
}
