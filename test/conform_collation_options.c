// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The collation options (record mloc-0069) for a differential against
// ICU4C: reads lines of two UTF-8 texts in hex, separated by a tab, and
// writes for each line the order (-1, 0 or 1) under each option set,
// tab-separated, in the order the oracle
// docs/locale/research/oracle/collation/icu4c_collation_options.cpp
// writes them.
//
// usage: conform_collation_options collation.mloc < pairs

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_compare.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Script(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (d))

alignas(8) static uint8_t s_file[1 << 19];

static size_t Bytes(const char* hex, size_t n, uint8_t* out)
{
    size_t k = 0;
    for (size_t i = 0; i + 1 < n; i += 2)
    {
        char pair[3] = {hex[i], hex[i + 1], 0};
        out[k++] = (uint8_t)strtoul(pair, nullptr, 16);
    }
    return k;
}

int main(int argc, char** argv)
{
    FILE* f = argc == 2 ? fopen(argv[1], "rb") : nullptr;
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocData data;
    DataSpan span = {0};
    CollationRoot root;
    if (size == 0 || size == sizeof(s_file) || mlocOpenData(&data, s_file, size) != mloc_success)
    {
        fputs("usage: conform_collation_options collation.mloc < pairs\n", stderr);
        return 2;
    }
    DataView view = mlocViewOf(&data);
    if (!mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &root))
    {
        fputs("conform_collation_options: no root unit\n", stderr);
        return 2;
    }
    // Maul Unicode's packed script codes.
    enum : uint32_t
    {
        greek = Script('G', 'r', 'e', 'k'),
        latin = Script('L', 'a', 't', 'n'),
        cyrillic = Script('C', 'y', 'r', 'l'),
        han = Script('H', 'a', 'n', 'i'),
        unknown = Script('Z', 'z', 'z', 'z'),
    };
    CollationSettings d = mlocCollationDefaults(&root);
    const struct
    {
        uint32_t strength;
        bool shifted;
        bool backwards;
        uint32_t caseFirst;
        bool caseLevel;
        bool numeric;
        // Reorder codes, 0-ended.
        uint32_t reorder[5];
    } sets[] = {
        {collationTertiary, false, false, collationCaseFirstOff, false, false, {0}},
        {collationTertiary, false, true, collationCaseFirstOff, false, false, {0}},
        {collationTertiary, false, false, collationUpperFirst, false, false, {0}},
        {collationTertiary, false, false, collationLowerFirst, false, false, {0}},
        {collationPrimary, false, false, collationCaseFirstOff, true, false, {0}},
        {collationSecondary, false, false, collationUpperFirst, true, false, {0}},
        {collationTertiary, false, false, collationUpperFirst, true, false, {0}},
        {collationQuaternary, true, false, collationCaseFirstOff, false, false, {0}},
        {collationQuaternary, true, true, collationLowerFirst, false, false, {0}},
        {collationIdentical, false, false, collationCaseFirstOff, false, false, {0}},
        {collationIdentical, true, true, collationUpperFirst, true, false, {0}},
        {collationTertiary, false, false, collationCaseFirstOff, false, true, {0}},
        {collationQuaternary, true, false, collationUpperFirst, false, true, {0}},
        {collationIdentical, false, true, collationCaseFirstOff, true, true, {0}},
        {collationTertiary, false, false, collationCaseFirstOff, false, false, {greek}},
        {collationTertiary, false, false, collationCaseFirstOff, false, false, {latin, han}},
        {collationTertiary,
         false,
         false,
         collationCaseFirstOff,
         false,
         true,
         {collationGroupDigit, collationGroupPunctuation}},
        {collationTertiary,
         false,
         false,
         collationCaseFirstOff,
         false,
         false,
         {cyrillic, unknown, latin}},
        {collationQuaternary,
         true,
         false,
         collationCaseFirstOff,
         false,
         false,
         {collationGroupSpace, han, unknown, collationGroupDigit}},
    };
    static CollationReorder reorders[32];
    static char line[131072];
    static uint8_t a[65536];
    static uint8_t b[65536];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        char* tab = strchr(line, '\t');
        if (tab == nullptr)
        {
            continue;
        }
        size_t aLength = Bytes(line, (size_t)(tab - line), a);
        size_t bLength = Bytes(tab + 1, strcspn(tab + 1, "\r\n"), b);
        for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); i++)
        {
            CollationSettings s = d;
            s.strength = sets[i].strength;
            s.shifted = sets[i].shifted;
            s.backwards = sets[i].backwards;
            s.caseFirst = sets[i].caseFirst;
            s.caseLevel = sets[i].caseLevel;
            s.numeric = sets[i].numeric;
            size_t codes = 0;
            while (codes < 5 && sets[i].reorder[codes] != 0)
            {
                codes++;
            }
            bool changes = false;
            if (mlocCollationMakeReorder(&root, sets[i].reorder, codes, &reorders[i], &changes) !=
                mloc_success)
            {
                fprintf(stderr, "conform_collation_options: set %zu refused\n", i);
                return 1;
            }
            s.reorder = changes ? &reorders[i] : nullptr;
            printf(i == 0 ? "%d" : "\t%d", mlocCollationCompare(&root, &s, a, aLength, b, bLength));
        }
        printf("\n");
    }
    return 0;
}
