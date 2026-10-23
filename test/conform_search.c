// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation search (record mloc-0074) for a differential against
// ICU4C's usearch: reads lines of a locale tag, a pattern's and a
// text's code points in hex, a position in UTF-16 units and a
// comparison (0, 1 or 2), tab-separated, with a sixth field 1 for a
// backward find, and writes for each line the match's start and end in
// units, "none" or "error", as the oracle
// tools/oracle/collation/icu4c_search.cpp writes them.
// The UTF-8 find must agree, its bytes mapped to units.
//
// usage: conform_search collation.mloc < lines

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static char s_line[1 << 16];

typedef struct Text
{
    uint16_t units[4096];
    size_t length;
    char bytes[8192];
    size_t size;
    // The unit each byte offset falls on (a code point's bytes, its first
    // unit).
    size_t unitOf[8193];
} Text;

static Text s_pattern;
static Text s_text;

static void Encode(Text* t, uint32_t c)
{
    size_t unit = t->length;
    t->unitOf[t->size] = unit;
    if (c >= 0x10000)
    {
        t->units[t->length++] = (uint16_t)(0xD7C0 + (c >> 10));
        t->units[t->length++] = (uint16_t)(0xDC00 | (c & 0x3FF));
    }
    else
    {
        t->units[t->length++] = (uint16_t)c;
    }
    char* b = t->bytes + t->size;
    size_t n = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    static const uint8_t lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
    for (size_t i = n; i-- > 1;)
    {
        b[i] = (char)(0x80 | (c & 0x3F));
        c >>= 6;
    }
    b[0] = (char)(lead[n] | c);
    for (size_t i = 1; i < n; i++)
    {
        t->unitOf[t->size + i] = unit;
    }
    t->size += n;
    t->unitOf[t->size] = t->length;
}

static void Read(Text* t, char* hex)
{
    t->length = 0;
    t->size = 0;
    t->unitOf[0] = 0;
    for (char* p = strtok(hex, " "); p != nullptr && t->length + 2 < 4096 && t->size + 4 < 8192;
         p = strtok(nullptr, " "))
    {
        Encode(t, (uint32_t)strtoul(p, nullptr, 16));
    }
}

static void Print(mlocResult r, size_t start, size_t end)
{
    if (r == mloc_success)
    {
        printf("%zu %zu\n", start, end);
    }
    else
    {
        printf(r == mloc_errorNotFound ? "none\n" : "error\n");
    }
}

// One line's find, in UTF-16 and in UTF-8 at the same position.
static void Search(const mlocCollator* collator, char** field, int n)
{
    mlocSearcher searcher;
    mlocSearcher searcher8;
    size_t from = (size_t)strtoul(field[3], nullptr, 10);
    bool back = n == 6 && strtol(field[5], nullptr, 10) == 1;
    mlocSearchOptions options = {(mlocSearchComparison)strtol(field[4], nullptr, 10)};
    Read(&s_pattern, field[1]);
    Read(&s_text, field[2]);
    mlocResult r =
        mlocInitSearcherUtf16(&searcher, collator, s_pattern.units, s_pattern.length, &options);
    mlocResult r8 =
        mlocInitSearcher(&searcher8, collator, s_pattern.bytes, s_pattern.size, &options);
    if (r != mloc_success || r8 != r)
    {
        printf(r8 != r ? "differ\n" : "error\n");
        return;
    }
    size_t start = 0;
    size_t end = 0;
    r = back ? mlocFindPreviousUtf16(&searcher, s_text.units, s_text.length, from, &start, &end)
             : mlocFindNextUtf16(&searcher, s_text.units, s_text.length, from, &start, &end);
    // The same position in bytes: the last byte on that unit or
    // before, which the find moves to its code point's start, as it
    // moves a unit inside a pair.
    size_t byteFrom = 0;
    for (size_t b = 0; b <= s_text.size; b++)
    {
        byteFrom = s_text.unitOf[b] <= from ? b : byteFrom;
    }
    size_t start8 = 0;
    size_t end8 = 0;
    r8 = back ? mlocFindPrevious(&searcher8, s_text.bytes, s_text.size, byteFrom, &start8, &end8)
              : mlocFindNext(&searcher8, s_text.bytes, s_text.size, byteFrom, &start8, &end8);
    if (r8 != r ||
        (r == mloc_success && (s_text.unitOf[start8] != start || s_text.unitOf[end8] != end)))
    {
        printf("differ\n");
        return;
    }
    Print(r, start, end);
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
        fputs("usage: conform_search collation.mloc < lines\n", stderr);
        return 2;
    }
    mlocDataSet data = {s_sources, 2};
    while (fgets(s_line, sizeof(s_line), stdin) != nullptr)
    {
        char* field[6] = {s_line};
        int n = 1;
        for (char* p = s_line; *p != '\0' && n < 6; p++)
        {
            if (*p == '\t')
            {
                *p = '\0';
                field[n++] = p + 1;
            }
        }
        mlocLocale locale;
        mlocCollator collator;
        if (n < 5 || mlocParseLocale(field[0], strlen(field[0]), &locale).status != mloc_success ||
            mlocInitCollator(&collator, &data, &locale, nullptr) != mloc_success)
        {
            printf("error\n");
            continue;
        }
        Search(&collator, field, n);
    }
    return 0;
}
