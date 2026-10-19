// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The general compare against CLDR's CollationTest files (records
// mloc-0067 and mloc-0069): every line, read as code points, compares no
// greater than the next at the quaternary strength (the identical one
// after --identical), non-ignorable or, after --shifted, shifted with the
// variable top at the end of punctuation, the root read from the
// collation data file. As ICU4X's
// conformance test does, a line holding a surrogate is passed over, since
// UTF-8 cannot carry one; the next line is compared with the last read.
//
// usage: conform_collation collation.mloc [--shifted] [--identical] CollationTest_CLDR_*.txt...

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

// A line's code points as UTF-8; false for a surrogate or no code point.
static bool Text(const char* line, uint8_t* out, size_t* lengthOut)
{
    size_t n = 0;
    const char* p = line;
    for (;;)
    {
        while (*p == ' ')
        {
            p++;
        }
        char* end = nullptr;
        unsigned long c = strtoul(p, &end, 16);
        if (end == p)
        {
            break;
        }
        p = end;
        if ((c >= 0xD800 && c <= 0xDFFF) || c > 0x10FFFF)
        {
            return false;
        }
        if (c < 0x80)
        {
            out[n++] = (uint8_t)c;
        }
        else if (c < 0x800)
        {
            out[n++] = (uint8_t)(0xC0 | c >> 6);
            out[n++] = (uint8_t)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out[n++] = (uint8_t)(0xE0 | c >> 12);
            out[n++] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (uint8_t)(0x80 | (c & 0x3F));
        }
        else
        {
            out[n++] = (uint8_t)(0xF0 | c >> 18);
            out[n++] = (uint8_t)(0x80 | (c >> 12 & 0x3F));
            out[n++] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (uint8_t)(0x80 | (c & 0x3F));
        }
    }
    *lengthOut = n;
    return n > 0;
}

static bool CheckFile(const CollationRoot* root, const CollationSettings* settings,
                      const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == nullptr)
    {
        printf("%s: cannot read\n", path);
        return false;
    }
    static char line[4096];
    static uint8_t texts[2][1024];
    size_t lengths[2] = {0, 0};
    int current = 0;
    bool have = false;
    size_t lines = 0;
    size_t skipped = 0;
    size_t wrong = 0;
    size_t ties = 0;
    while (fgets(line, sizeof(line), f) != nullptr)
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
        {
            continue;
        }
        lines++;
        if (!Text(line, texts[current], &lengths[current]))
        {
            skipped++;
            continue;
        }
        int previous = 1 - current;
        int order = have ? mlocCollationCompare(root, settings, texts[previous], lengths[previous],
                                                texts[current], lengths[current])
                         : -1;
        ties += order == 0;
        if (order > 0)
        {
            if (wrong++ < 10)
            {
                printf("%s: line %zu sorts before the one above: %s", path, lines, line);
            }
        }
        have = true;
        current = previous;
    }
    fclose(f);
    printf("%s: %zu lines, %zu with a surrogate passed over, %zu equal to the one above, "
           "%zu out of order\n",
           path, lines, skipped, ties, wrong);
    return wrong == 0 && lines > skipped;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fputs("usage: conform_collation collation.mloc [--shifted] [--identical] "
              "CollationTest_CLDR_*.txt...\n",
              stderr);
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    size_t size = f == nullptr ? 0 : fread(s_file, 1, sizeof(s_file), f);
    if (f != nullptr)
    {
        fclose(f);
    }
    mlocData data;
    DataSpan span = {0};
    CollationRoot root;
    DataView view;
    bool open =
        size > 0 && size < sizeof(s_file) && mlocOpenData(&data, s_file, size) == mloc_success;
    if (open)
    {
        view = mlocViewOf(&data);
    }
    if (!open || !mlocFindDataUnit(&view, kindCollationRoot, dataNoLocale, &span) ||
        !mlocReadCollationRoot(span, &root))
    {
        fprintf(stderr, "conform_collation: cannot read the root from %s\n", argv[1]);
        return 2;
    }
    CollationSettings settings = mlocCollationDefaults(&root);
    settings.strength = collationQuaternary;
    int first = 2;
    for (; first < argc && strncmp(argv[first], "--", 2) == 0; first++)
    {
        settings.shifted = settings.shifted || strcmp(argv[first], "--shifted") == 0;
        settings.strength =
            strcmp(argv[first], "--identical") == 0 ? collationIdentical : settings.strength;
    }
    bool ok = first < argc;
    for (int i = first; i < argc; i++)
    {
        ok = CheckFile(&root, &settings, argv[i]) && ok;
    }
    return ok ? 0 : 1;
}
