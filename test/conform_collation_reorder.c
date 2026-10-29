// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Script reordering for a differential against ICU4C: with "samples",
// writes one code point for each range of the root's script starts, one
// whose text is a single primary. Otherwise reads the samples from a file
// and lines of reorder codes from the input (scripts as their four
// letters; space, punct, symbol, currency and digit for the groups) and
// writes for each line its result (0, -1 refused as invalid, -2 for
// capacity) and, when 0, each sample's primary after the reordering, in
// hex. The oracle tools/oracle/collation/icu4c_collation_reorder.cpp
// writes the same from ICU's sort keys.
//
// usage: conform_collation_reorder collation.mloc samples > samples.txt
//        conform_collation_reorder collation.mloc samples.txt < lists

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_iterator.h"
#include "collation_reorder.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <string.h>

enum
{
    maxSamples = 256,
    maxCodes = 64,
};

alignas(8) static uint8_t s_file[1 << 22];
static CollationRoot s_root;

static size_t Utf8(uint32_t c, uint8_t* out)
{
    if (c < 0x80)
    {
        out[0] = (uint8_t)c;
        return 1;
    }
    if (c < 0x800)
    {
        out[0] = (uint8_t)(0xC0 | c >> 6);
        out[1] = (uint8_t)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000)
    {
        out[0] = (uint8_t)(0xE0 | c >> 12);
        out[1] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
        out[2] = (uint8_t)(0x80 | (c & 0x3F));
        return 3;
    }
    out[0] = (uint8_t)(0xF0 | c >> 18);
    out[1] = (uint8_t)(0x80 | (c >> 12 & 0x3F));
    out[2] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
    out[3] = (uint8_t)(0x80 | (c & 0x3F));
    return 4;
}

// The code point's primary when its text has exactly one, else 0.
static uint32_t Primary(uint32_t c)
{
    uint8_t text[4];
    CollationIterator it;
    mlocCollationIteratorStart(&it, &s_root, text, Utf8(c, text));
    uint64_t element = 0;
    uint32_t primary = 0;
    int count = 0;
    while (mlocCollationNextElement(&it, &element))
    {
        if (element >> 32 != 0)
        {
            primary = (uint32_t)(element >> 32);
            count++;
        }
    }
    return count == 1 ? primary : 0;
}

static void Samples(void)
{
    static uint32_t first[256];
    for (uint32_t c = 0x21; c < 0x110000; c++)
    {
        // Surrogates have no text; Han's ideographs share one range.
        bool skipped = (c >= 0xD800 && c < 0xE000) || (c >= 0x3400 && c < 0xFA00);
        uint32_t high = skipped ? 0 : Primary(c) >> 16;
        for (uint32_t i = 1; high != 0 && i + 1 < s_root.startCount; i++)
        {
            if (mlocCollationStart(&s_root, i) <= high && high < mlocCollationStart(&s_root, i + 1))
            {
                first[i] = first[i] == 0 ? c : first[i];
                break;
            }
        }
    }
    for (uint32_t i = 1; i + 1 < s_root.startCount; i++)
    {
        if (first[i] != 0)
        {
            printf("%X\n", first[i]);
        }
    }
}

static uint32_t Code(const char* word)
{
    static const char* const groups[] = {"space", "punct", "symbol", "currency", "digit"};
    for (uint32_t i = 0; i < 5; i++)
    {
        if (strcmp(word, groups[i]) == 0)
        {
            return collationGroupSpace + i;
        }
    }
    size_t n = strlen(word);
    return n != 4 ? 0
                  : (uint32_t)(uint8_t)word[0] << 24 | (uint32_t)(uint8_t)word[1] << 16 |
                        (uint32_t)(uint8_t)word[2] << 8 | (uint8_t)word[3];
}

static int Lists(const char* samplesPath)
{
    static uint32_t samples[maxSamples];
    size_t sampleCount = 0;
    FILE* f = fopen(samplesPath, "r");
    unsigned value = 0;
    while (f != nullptr && sampleCount < maxSamples && fscanf(f, "%x", &value) == 1)
    {
        samples[sampleCount++] = value;
    }
    if (f != nullptr)
    {
        fclose(f);
    }
    static char line[4096];
    static CollationReorder reorder;
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        uint32_t codes[maxCodes];
        size_t count = 0;
        for (char* word = strtok(line, " \r\n"); word != nullptr && count < maxCodes;
             word = strtok(nullptr, " \r\n"))
        {
            codes[count++] = Code(word);
        }
        bool changes = false;
        mlocResult result = mlocCollationMakeReorder(&s_root, codes, count, &reorder, &changes);
        printf("%d", result == mloc_success ? 0 : result == mloc_errorInvalid ? -1 : -2);
        for (size_t i = 0; result == mloc_success && i < sampleCount; i++)
        {
            uint32_t p = Primary(samples[i]);
            printf(" %X", changes ? mlocCollationReorderPrimary(&reorder, p) : p);
        }
        printf("\n");
    }
    return 0;
}

int main(int argc, char** argv)
{
    FILE* f = argc == 3 ? fopen(argv[1], "rb") : nullptr;
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocData data;
    DataSpan span = {0};
    if (size == 0 || size == sizeof(s_file) || mlocOpenData(s_file, size, &data) != mloc_success)
    {
        fputs("usage: conform_collation_reorder collation.mloc samples|samples.txt\n", stderr);
        return 2;
    }
    DataView view = mlocViewOf(&data);
    if (!mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &s_root))
    {
        fputs("conform_collation_reorder: no root unit\n", stderr);
        return 2;
    }
    if (strcmp(argv[2], "samples") == 0)
    {
        Samples();
        return 0;
    }
    return Lists(argv[2]);
}
