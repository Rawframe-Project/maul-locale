// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The index labels in the data (record mloc-0075) for a differential
// against ICU4C's AlphabeticIndex: reads a locale tag a line and writes
// the tag, a tab and the labels the main chain finds, each label's code
// points in hex joined by "_", space-separated in code point order, as
// docs/locale/research/oracle/collation/icu4c_index_labels.cpp writes
// them; "error" for a tag that does not parse or labels not found.
//
// usage: conform_index_labels collation.mloc < tags

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_index.h"

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-unicode/encoding.h"

#include <stdio.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];

static void Print(const uint8_t* text, uint32_t length)
{
    const char* separator = "";
    for (size_t at = 0; at < length;)
    {
        uint32_t cp = 0;
        size_t size = 0;
        if (muniDecodeUtf8((const char*)text + at, length - at, &cp, &size) != muni_success)
        {
            printf("?");
            return;
        }
        printf("%s%X", separator, (unsigned)cp);
        separator = "_";
        at += size;
    }
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
        fputs("usage: conform_index_labels collation.mloc < tags\n", stderr);
        return 2;
    }
    mlocDataSet data = {s_sources, 2};
    char line[256];
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        line[strcspn(line, "\r\n")] = '\0';
        mlocLocale locale;
        IndexLabels labels;
        if (mlocParseLocale(line, strlen(line), &locale).status != mloc_success ||
            !mlocFindIndexLabels(&data, &locale, &labels))
        {
            printf("%s\terror\n", line);
            continue;
        }
        printf("%s\t", line);
        uint32_t at = 4;
        const uint8_t* text = nullptr;
        uint32_t length = 0;
        for (uint32_t i = 0; mlocNextIndexLabel(&labels, &at, &text, &length); i++)
        {
            printf("%s", i == 0 ? "" : " ");
            Print(text, length);
        }
        printf("\n");
    }
    return 0;
}
