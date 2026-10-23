// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The alphabetic index (record mloc-0075) for a differential against
// ICU4C's ImmutableIndex: reads lines of a locale tag, a maximum label
// count (0 for the default), 1 to set the special labels to "U", "I" and
// "O" (else 0), and names (each a name's code points in hex joined by
// "_", space-separated), tab-separated, and writes the index's buckets
// and each name's bucket as
// tools/oracle/collation/icu4c_index.cpp writes them. The
// UTF-16 find must agree with the UTF-8 one.
//
// usage: conform_index collation.mloc < lines

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "maul-locale/data.h"
#include "maul-locale/index.h"
#include "maul-locale/locale.h"
#include "maul-unicode/encoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];
static mlocData s_sources[2];
static char s_line[1 << 20];
static uint8_t s_index[1 << 20];

static void PrintHex(const char* text, size_t length)
{
    const char* separator = "";
    for (size_t at = 0; at < length;)
    {
        uint32_t cp = 0;
        size_t size = 0;
        if (muniDecodeUtf8(text + at, length - at, &cp, &size) != muni_success)
        {
            printf("?");
            return;
        }
        printf("%s%X", separator, (unsigned)cp);
        separator = "_";
        at += size;
    }
}

// A name in hex as UTF-8 and UTF-16; false when it does not fit.
static bool Name(const char* hex, char* utf8, size_t* length, uint16_t* utf16, size_t* units)
{
    *length = 0;
    *units = 0;
    for (const char* p = hex; *p != '\0';)
    {
        char* end = nullptr;
        uint32_t cp = (uint32_t)strtoul(p, &end, 16);
        size_t size = 0;
        if (*length + 4 > 1024 || *units + 2 > 1024 ||
            muniEncodeUtf8(cp, utf8 + *length, &size) != muni_success)
        {
            return false;
        }
        *length += size;
        if (cp >= 0x10000)
        {
            utf16[(*units)++] = (uint16_t)(0xD7C0 + (cp >> 10));
            utf16[(*units)++] = (uint16_t)(0xDC00 | (cp & 0x3FF));
        }
        else
        {
            utf16[(*units)++] = (uint16_t)cp;
        }
        p = *end == '_' ? end + 1 : end;
    }
    return true;
}

static void PrintBuckets(const void* index)
{
    uint32_t count = 0;
    (void)mlocGetIndexBucketCount(index, &count);
    for (uint32_t b = 0; b < count; b++)
    {
        mlocIndexBucket bucket;
        if (mlocGetIndexBucket(index, b, &bucket) != mloc_success)
        {
            printf(" ?");
            continue;
        }
        printf("%s%d:", b == 0 ? "" : " ", (int)bucket.type);
        PrintHex(bucket.label, bucket.length);
    }
}

static void PrintNames(const void* index, char* names)
{
    bool first = true;
    for (char* name = strtok(names, " "); name != nullptr; name = strtok(nullptr, " "))
    {
        char utf8[1024];
        uint16_t utf16[1024];
        size_t length = 0;
        size_t units = 0;
        uint32_t b8 = 0;
        uint32_t b16 = 0;
        bool ok = Name(name, utf8, &length, utf16, &units) &&
                  mlocFindIndexBucket(index, utf8, length, &b8) == mloc_success &&
                  mlocFindIndexBucketUtf16(index, utf16, units, &b16) == mloc_success;
        if (ok && b8 == b16)
        {
            printf("%s%u", first ? "" : " ", (unsigned)b8);
        }
        else
        {
            printf("%s%s", first ? "" : " ", ok ? "differ" : "error");
        }
        first = false;
    }
}

static void Line(const mlocDataSet* data, char** field, int n)
{
    mlocLocale locale;
    mlocCollator collator;
    mlocIndexOptions options = {.maxLabelCount = (uint32_t)strtoul(field[1], nullptr, 10)};
    if (strtol(field[2], nullptr, 10) == 1)
    {
        options = (mlocIndexOptions){options.maxLabelCount, "U", 1, "I", 1, "O", 1};
    }
    size_t size = 0;
    if (mlocParseLocale(field[0], strlen(field[0]), &locale).status != mloc_success ||
        mlocInitCollator(&collator, data, &locale, nullptr) != mloc_success ||
        mlocBuildIndex(&collator, data, &locale, &options, s_index, sizeof(s_index), &size) !=
            mloc_success)
    {
        printf("%s\terror\n", field[0]);
        return;
    }
    printf("%s\t", field[0]);
    PrintBuckets(s_index);
    printf("\t");
    if (n == 4)
    {
        PrintNames(s_index, field[3]);
    }
    printf("\n");
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
        fputs("usage: conform_index collation.mloc < lines\n", stderr);
        return 2;
    }
    mlocDataSet data = {s_sources, 2};
    while (fgets(s_line, sizeof(s_line), stdin) != nullptr)
    {
        s_line[strcspn(s_line, "\r\n")] = '\0';
        char* field[4] = {s_line, nullptr, nullptr, nullptr};
        int n = 1;
        for (char* p = s_line; *p != '\0' && n < 4; p++)
        {
            if (*p == '\t')
            {
                *p = '\0';
                field[n++] = p + 1;
            }
        }
        if (n < 3)
        {
            printf("%s\terror\n", s_line);
            continue;
        }
        Line(&data, field, n);
    }
    return 0;
}
