// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A searcher whose stamp is right but whose stored counts are not (the
// library checks every index it stores): a ring of no elements or more
// than it holds, a pattern of no elements or more than it holds, an NFD
// longer than it holds, a comparison that is none, and a collator inside
// it that is not one or whose bool or kind bytes are forged, are refused
// by every find, which then reads none of them; a searcher the library
// made is found in, before and after.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "search_find.h"
#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/search.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static mlocDataSet s_data = {s_sources, 2};
static mlocSearcher s_made;
static mlocSearcher s_searcher;

static bool Make(const char* id)
{
    mlocLocale locale;
    mlocCollator collator;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitCollator(&s_data, &locale, nullptr, &collator) == mloc_success &&
           mlocInitSearcher(&collator, "ab", 2, nullptr, &s_made) == mloc_success;
}

// Whether every find gives the result on the searcher: forward and back,
// UTF-8 and UTF-16.
static bool AllGive(mlocResult expected)
{
    static const uint16_t units[] = {'x', 'x', 'a', 'b'};
    size_t start = 0;
    size_t end = 0;
    mlocResult r[] = {
        mlocSearcher_FindNext(&s_searcher, "xxab", 4, 0, &start, &end),
        mlocSearcher_FindNextUtf16(&s_searcher, units, 4, 0, &start, &end),
        mlocSearcher_FindPrevious(&s_searcher, "xxab", 4, 4, &start, &end),
        mlocSearcher_FindPreviousUtf16(&s_searcher, units, 4, 4, &start, &end),
    };
    bool all = true;
    for (size_t i = 0; i < sizeof(r) / sizeof(r[0]); i++)
    {
        all = all && r[i] == expected;
    }
    return all;
}

static Searcher* Fresh(void)
{
    s_searcher = s_made;
    return (Searcher*)&s_searcher;
}

static void TestCounts(void)
{
    CHECK(Make("en") && (Fresh(), AllGive(mloc_success)), "a searcher the library made finds");
    Fresh()->ring = 0;
    CHECK(AllGive(mloc_errorInvalid), "a ring of no elements is refused");
    Fresh()->ring = searchRing + 1;
    CHECK(AllGive(mloc_errorInvalid), "a ring past its room is refused");
    Fresh()->ring = searchRing;
    CHECK(AllGive(mloc_success), "a ring at its room finds");
    Fresh()->count = 0;
    CHECK(AllGive(mloc_errorInvalid), "a pattern of no elements is refused");
    Fresh()->count = searchMaxElements + 1;
    CHECK(AllGive(mloc_errorInvalid), "a pattern past its room is refused");
    Fresh()->nfdCount = searchMaxElements + 1;
    CHECK(AllGive(mloc_errorInvalid), "an NFD past its room is refused");
    Fresh()->nfdCount = searchMaxElements;
    CHECK(AllGive(mloc_success), "an NFD at its room is not read below the identical strength");
    Fresh()->comparison = mloc_searchAnyBaseWildcard + 1;
    CHECK(AllGive(mloc_errorInvalid), "a comparison the header does not name is refused");
    Fresh()->collator.stamp = 0;
    CHECK(AllGive(mloc_errorInvalid), "a collator that is not one is refused");
}

static void TestCollatorRanges(void)
{
    CHECK(Make("en-u-kr-grek") && Fresh()->collator.reordered == 1 && AllGive(mloc_success),
          "a Greek-first searcher finds");
    Fresh()->collator.reorder.rangeCount = collationMaxReorderRanges + 1;
    CHECK(AllGive(mloc_errorInvalid), "its collator's reordering past its ranges is refused");
}

// Its collator's bool and kind bytes, which every find reads: a settings
// bool byte of 2, which is undefined to read as a bool, and a strength the
// library has not, are refused.
static void TestCollatorBytes(void)
{
    CHECK(Make("en") && (Fresh(), AllGive(mloc_success)), "an English searcher finds");
    ((uint8_t*)Fresh())[offsetof(Searcher, collator.settings.shifted)] = 2;
    CHECK(AllGive(mloc_errorInvalid), "its collator's settings bool byte of 2 is refused");
    Fresh()->collator.settings.strength = collationIdentical + 1;
    CHECK(AllGive(mloc_errorInvalid), "its collator's strength past the levels is refused");
    Fresh();
    CHECK(AllGive(mloc_success), "put back, it finds again");
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
        mlocOpenData(s_file, size, &s_sources[1]) != mloc_success)
    {
        fputs("test_forged_searcher: cannot open " MLOC_COLLATION_FILE "\n", stderr);
        return 2;
    }
    TestCounts();
    TestCollatorRanges();
    TestCollatorBytes();
    return s_failures == 0 ? 0 : 1;
}
