// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Search's processed elements (record mloc-0074) for a differential
// against ICU4C's UCollationPCE: reads lines of a locale tag (its -u-
// keys set the options), a tab and a text's code points in hex, and
// writes for each line its processed elements over the text's UTF-16
// form, each as "element:low:high" in hex and units, as the oracle
// docs/locale/research/oracle/collation/icu4c_search_elements.cpp
// writes them.
//
// usage: conform_search_elements collation.mloc < lines

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collator.h"
#include "search_elements.h"

#include "maul-locale/data.h"
#include "maul-locale/locale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static char s_line[1 << 16];
static uint16_t s_units[1 << 15];

// The code points in hex as UTF-16 units; their count.
static size_t Units(char* hex)
{
    size_t n = 0;
    for (char* p = strtok(hex, " \n"); p != nullptr && n + 2 <= sizeof(s_units) / 2;
         p = strtok(nullptr, " \n"))
    {
        uint32_t c = (uint32_t)strtoul(p, nullptr, 16);
        if (c >= 0x10000)
        {
            s_units[n++] = (uint16_t)(0xD7C0 + (c >> 10));
            s_units[n++] = (uint16_t)(0xDC00 | (c & 0x3FF));
        }
        else
        {
            s_units[n++] = (uint16_t)c;
        }
    }
    return n;
}

int main(int argc, char** argv)
{
    FILE* f = argc == 2 ? fopen(argv[1], "rb") : nullptr;
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocOpenBuiltinData(&s_sources[0]);
    if (size == 0 || size == sizeof(s_file) ||
        mlocOpenData(&s_sources[1], s_file, size) != mloc_success)
    {
        fputs("usage: conform_search_elements collation.mloc < lines\n", stderr);
        return 2;
    }
    mlocDataSet data = {s_sources, 2};
    while (fgets(s_line, sizeof(s_line), stdin) != nullptr)
    {
        char* tab = strchr(s_line, '\t');
        mlocLocale locale;
        mlocCollator collator;
        Collator c;
        if (tab == nullptr ||
            mlocParseLocale(s_line, (size_t)(tab - s_line), &locale).status != mloc_success ||
            mlocInitCollator(&collator, &data, &locale, nullptr) != mloc_success ||
            !mlocReadCollator(&collator, &c))
        {
            printf("error\n");
            continue;
        }
        size_t n = Units(tab + 1);
        SearchElements e;
        mlocSearchElementsStart(&e, &c.root, &c.settings, mlocNfdUtf16(s_units, n));
        uint32_t low = 0;
        uint32_t high = 0;
        const char* gap = "";
        for (uint64_t p = mlocSearchNextElement(&e, &low, &high); p != searchEnd;
             p = mlocSearchNextElement(&e, &low, &high))
        {
            printf("%s%llx:%x:%x", gap, (unsigned long long)p, low, high);
            gap = " ";
        }
        printf("\n");
    }
    return 0;
}
