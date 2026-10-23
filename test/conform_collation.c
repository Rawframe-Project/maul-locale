// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The general compare against CLDR's CollationTest files: every line, read
// as code points, compares no greater than the next at the quaternary
// strength (the identical one after --identical), non-ignorable or, after
// --shifted, shifted with the variable top at the end of punctuation, the
// root read from the collation data file. As ICU4X's conformance test
// does, a line holding a surrogate is passed over, since UTF-8 cannot
// carry one; the next line is compared with the last read. The two lines'
// sort keys must order them as the compare does, ties included, and the
// fast Latin compare must too wherever it does not bail out; it counts its
// answers. Each line's UTF-16 form must compare as its UTF-8 form, in both
// compares, and give the same key bytes.
//
// usage: conform_collation collation.mloc [--shifted] [--identical] CollationTest_CLDR_*.txt...

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "collation_compare.h"
#include "collation_fast_compare.h"
#include "collation_key.h"
#include "collation_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

alignas(8) static uint8_t s_file[1 << 22];

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

// The order of two sort keys, as memcmp gives it over the shorter.
static int KeyOrder(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    int r = memcmp(a, b, aLength < bLength ? aLength : bLength);
    r = r != 0 ? r : (aLength > bLength) - (aLength < bLength);
    return (r > 0) - (r < 0);
}

// A well-formed UTF-8 text's UTF-16 form; its length in units.
static size_t ToUtf16(const uint8_t* text, size_t length, uint16_t* out)
{
    size_t n = 0;
    for (size_t i = 0; i < length;)
    {
        uint32_t c = text[i];
        size_t extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        c = extra == 0 ? c : c & (0x3Fu >> extra);
        for (size_t k = 1; k <= extra; k++)
        {
            c = c << 6 | (text[i + k] & 0x3Fu);
        }
        i += 1 + extra;
        if (c >= 0x10000)
        {
            out[n++] = (uint16_t)(0xD7C0 + (c >> 10));
            out[n++] = (uint16_t)(0xDC00 | (c & 0x3FF));
        }
        else
        {
            out[n++] = (uint16_t)c;
        }
    }
    return n;
}

// Whether the line's UTF-16 form orders against the one above as its
// UTF-8 form does, in the general and the fast compare, and keys alike.
static bool SameInUtf16(const CollationRoot* root, const CollationSettings* settings,
                        const FastLatinOptions* fast, const uint16_t* a, size_t aLength,
                        const uint16_t* b, size_t bLength, int order, const uint8_t* key,
                        size_t keyLength)
{
    static uint8_t key16[4096];
    int order16 = mlocCollationCompareText(root, settings, mlocNfdUtf16(a, aLength),
                                           mlocNfdUtf16(b, bLength));
    int fast16 =
        fast != nullptr ? mlocFastLatinCompareUtf16(fast, a, aLength, b, bLength) : fastLatinBail;
    size_t n =
        mlocCollationSortKeyText(root, settings, mlocNfdUtf16(b, bLength), key16, sizeof(key16));
    return order16 == order && (fast16 == fastLatinBail || fast16 == order) && n == keyLength &&
           memcmp(key16, key, n) == 0;
}

static bool CheckFile(const CollationRoot* root, const CollationSettings* settings,
                      const FastLatinOptions* fast, const char* path)
{
    FILE* f = fopen(path, "rb");
    if (f == nullptr)
    {
        printf("%s: cannot read\n", path);
        return false;
    }
    static char line[4096];
    static uint8_t texts[2][1024];
    static uint8_t keys[2][4096];
    size_t lengths[2] = {0, 0};
    size_t keyLengths[2] = {0, 0};
    size_t unitLengths[2] = {0, 0};
    int current = 0;
    bool have = false;
    size_t lines = 0;
    size_t skipped = 0;
    size_t wrong = 0;
    size_t ties = 0;
    size_t fastAnswers = 0;
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
        // The sort keys must order the two lines as the compare does.
        keyLengths[current] = mlocCollationSortKey(root, settings, texts[current], lengths[current],
                                                   keys[current], sizeof(keys[0]));
        int keyOrder = have ? KeyOrder(keys[previous], keyLengths[previous], keys[current],
                                       keyLengths[current])
                            : -1;
        int fastOrder = have && fast != nullptr
                            ? mlocFastLatinCompare(fast, texts[previous], lengths[previous],
                                                   texts[current], lengths[current])
                            : fastLatinBail;
        fastAnswers += fastOrder != fastLatinBail;
        static uint16_t units[2][1024];
        unitLengths[current] = ToUtf16(texts[current], lengths[current], units[current]);
        bool same16 =
            !have || SameInUtf16(root, settings, fast, units[previous], unitLengths[previous],
                                 units[current], unitLengths[current], order, keys[current],
                                 keyLengths[current]);
        ties += order == 0;
        if (order > 0 || keyOrder != order || (fastOrder != fastLatinBail && fastOrder != order) ||
            !same16)
        {
            if (wrong++ < 10)
            {
                printf("%s: line %zu sorts before the one above (compare %d, keys %d, fast %d, "
                       "UTF-16 alike %d): %s",
                       path, lines, order, keyOrder, fastOrder, same16, line);
            }
        }
        have = true;
        current = previous;
    }
    fclose(f);
    printf("%s: %zu lines, %zu with a surrogate passed over, %zu equal to the one above, "
           "%zu out of order; %zu answered by the fast compare\n",
           path, lines, skipped, ties, wrong, fastAnswers);
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
    // The identical strength has no fast compare.
    FastLatinOptions fast;
    bool fastServes =
        mlocFastLatinOptions(&root, &settings, root.fastLatin, root.fastLatinCount, &fast);
    bool ok = first < argc && (fastServes || settings.strength == collationIdentical);
    for (int i = first; i < argc; i++)
    {
        ok = CheckFile(&root, &settings, fastServes ? &fast : nullptr, argv[i]) && ok;
    }
    return ok ? 0 : 1;
}
