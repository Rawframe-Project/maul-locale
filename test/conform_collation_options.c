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
    CollationSettings d = mlocCollationDefaults(&root);
    const struct
    {
        uint32_t strength;
        bool shifted;
        bool backwards;
        uint32_t caseFirst;
        bool caseLevel;
    } sets[] = {
        {collationTertiary, false, false, collationCaseFirstOff, false},
        {collationTertiary, false, true, collationCaseFirstOff, false},
        {collationTertiary, false, false, collationUpperFirst, false},
        {collationTertiary, false, false, collationLowerFirst, false},
        {collationPrimary, false, false, collationCaseFirstOff, true},
        {collationSecondary, false, false, collationUpperFirst, true},
        {collationTertiary, false, false, collationUpperFirst, true},
        {collationQuaternary, true, false, collationCaseFirstOff, false},
        {collationQuaternary, true, true, collationLowerFirst, false},
        {collationIdentical, false, false, collationCaseFirstOff, false},
        {collationIdentical, true, true, collationUpperFirst, true},
    };
    static char line[8192];
    static uint8_t a[4096];
    static uint8_t b[4096];
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
            printf(i == 0 ? "%d" : "\t%d", mlocCollationCompare(&root, &s, a, aLength, b, bLength));
        }
        printf("\n");
    }
    return 0;
}
