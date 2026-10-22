// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation part's benchmark cases (bench_collation.h).

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "bench_collation.h"

#include "maul-locale/collate.h"
#include "maul-locale/data.h"
#include "maul-locale/index.h"
#include "maul-locale/search.h"

#include <stdio.h>
#include <string.h>

#ifndef MLOC_COLLATION_FILE
#define MLOC_COLLATION_FILE "src/generated/collation.mloc"
#endif

// The words of each case; the same in the ICU4C baseline. A compare
// walks 64 pairs in a fixed order (i = (i + 3) % 64, the pair i / 8 and
// i % 8), as research 06's case does.
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
static const char s_text[] = "Die Stra\xC3\x9F"
                             "e am Ufer f\xC3\xBChrt \xC3\xBC"
                             "ber die Br\xC3\xBC"
                             "cke zur STRASSE";

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
static mlocLocale s_deLocale;
static mlocCollator s_collators[4];
static mlocSearcher s_searcher;
static uint8_t s_index[1 << 14];
static uint8_t s_built[1 << 14];
static uint16_t s_utf16[8][32];
static size_t s_utf16Length[8];
static size_t s_length[8];
static unsigned s_pair;
static volatile int s_sink;

static void Init(const char* tag)
{
    mlocLocale locale;
    mlocCollator collator;
    s_sink += (int)mlocParseLocale(tag, strlen(tag), &locale).status;
    s_sink += (int)mlocInitCollator(&collator, &s_data, &locale, nullptr);
}

static void InitDe(void)
{
    Init("de");
}

static void InitZh(void)
{
    Init("zh");
}

static void Compare(const mlocCollator* c, const char* const* words)
{
    s_pair = (s_pair + 3) % 64;
    const char* a = words[s_pair / 8];
    const char* b = words[s_pair % 8];
    int order = 0;
    s_sink += (int)mlocCompare(c, a, strlen(a), b, strlen(b), &order);
    s_sink += order;
}

static void CompareDe(void)
{
    Compare(&s_collators[0], s_de);
}

static void CompareFr(void)
{
    Compare(&s_collators[1], s_fr);
}

static void CompareZh(void)
{
    Compare(&s_collators[2], s_zh);
}

static void CompareKo(void)
{
    Compare(&s_collators[3], s_ko);
}

static void ComparePrefix(void)
{
    Compare(&s_collators[0], s_prefix);
}

static void CompareUtf16(void)
{
    s_pair = (s_pair + 3) % 64;
    unsigned a = s_pair / 8;
    unsigned b = s_pair % 8;
    int order = 0;
    s_sink += (int)mlocCompareUtf16(&s_collators[0], s_utf16[a], s_utf16Length[a], s_utf16[b],
                                    s_utf16Length[b], &order);
    s_sink += order;
}

static void SortKey(void)
{
    s_pair = (s_pair + 1) % 8;
    uint8_t key[256];
    size_t length = 0;
    s_sink += (int)mlocGetSortKey(&s_collators[0], s_de[s_pair], s_length[s_pair], key, sizeof(key),
                                  &length);
    s_sink += (int)length;
}

static void IndexBuild(void)
{
    size_t size = 0;
    s_sink += (int)mlocBuildIndex(&s_collators[0], &s_data, &s_deLocale, nullptr, s_built,
                                  sizeof(s_built), &size);
    s_sink += (int)size;
}

static void IndexBucket(void)
{
    s_pair = (s_pair + 1) % 8;
    uint32_t bucket = 0;
    s_sink += (int)mlocFindIndexBucket(s_index, s_de[s_pair], s_length[s_pair], &bucket);
    s_sink += (int)bucket;
}

static void SearchFirst(void)
{
    size_t start = 0;
    size_t end = 0;
    s_sink += (int)mlocFindNext(&s_searcher, s_text, sizeof(s_text) - 1, 0, &start, &end);
    s_sink += (int)start;
}

static void SearchAll(void)
{
    size_t start = 0;
    size_t end = 0;
    int found = 0;
    for (size_t from = 0;
         mlocFindNext(&s_searcher, s_text, sizeof(s_text) - 1, from, &start, &end) == mloc_success;
         from = end)
    {
        found++;
    }
    s_sink += found;
}

// A word's UTF-16 form; its length in units.
static size_t Utf16(const char* text, uint16_t* out)
{
    size_t n = 0;
    for (const unsigned char* p = (const unsigned char*)text; *p != 0;)
    {
        uint32_t c = *p++;
        if (c >= 0xE0)
        {
            c = (c & 0x0F) << 12 | (uint32_t)(p[0] & 0x3F) << 6 | (p[1] & 0x3F);
            p += 2;
        }
        else if (c >= 0xC0)
        {
            c = (c & 0x1F) << 6 | (p[0] & 0x3F);
            p++;
        }
        out[n++] = (uint16_t)c;
    }
    return n;
}

static bool Make(const char* tag, mlocCollator* out)
{
    mlocLocale locale;
    return mlocParseLocale(tag, strlen(tag), &locale).status == mloc_success &&
           mlocInitCollator(out, &s_data, &locale, nullptr) == mloc_success;
}

bool PrepareCollationBench(void)
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
        return false;
    }
    for (size_t i = 0; i < 8; i++)
    {
        s_length[i] = strlen(s_de[i]);
        s_utf16Length[i] = Utf16(s_de[i], s_utf16[i]);
    }
    mlocCollator primary;
    mlocLocale searchLocale;
    size_t indexSize = 0;
    return Make("de", &s_collators[0]) && Make("fr", &s_collators[1]) &&
           Make("zh", &s_collators[2]) && Make("ko", &s_collators[3]) &&
           mlocParseLocale("de", 2, &s_deLocale).status == mloc_success &&
           mlocBuildIndex(&s_collators[0], &s_data, &s_deLocale, nullptr, s_index, sizeof(s_index),
                          &indexSize) == mloc_success &&
           mlocParseLocale("de-u-ks-level1", 14, &searchLocale).status == mloc_success &&
           mlocInitCollator(&primary, &s_data, &searchLocale, nullptr) == mloc_success &&
           mlocInitSearcher(&s_searcher, &primary, "strasse", 7, nullptr) == mloc_success;
}

const BenchCase g_collationCases[] = {
    {"collator-init-de", InitDe, 500000},       {"collator-init-zh", InitZh, 500000},
    {"compare-de", CompareDe, 5000000},         {"compare-fr", CompareFr, 5000000},
    {"compare-zh", CompareZh, 5000000},         {"compare-ko", CompareKo, 5000000},
    {"compare-prefix", ComparePrefix, 5000000}, {"compare-de-utf16", CompareUtf16, 5000000},
    {"sort-key-de", SortKey, 5000000},          {"index-build-de", IndexBuild, 2000},
    {"index-bucket-de", IndexBucket, 2000000},  {"search-first-de", SearchFirst, 1000000},
    {"search-all-de", SearchAll, 500000},
};
const size_t g_collationCaseCount = sizeof(g_collationCases) / sizeof(g_collationCases[0]);
