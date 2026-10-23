// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The tailoring builder's lookup with no tailored mapping, against the
// runtime's iterator over the root unit built from the same
// FractionalUCA.txt: the same elements, completely ignorable ones aside,
// for every code point alone, every mapping's prefix and text, and random
// strings of the mappings' code points and the marks among them. CI runs
// it on the pinned release.
//
// usage: conform_collation_mappings <FractionalUCA.txt> [RANDOM]

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_iterator.h"
#include "collation_mappings.h"
#include "collation_root.h"
#include "collation_unit.h"

#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxText = 64,
};

static size_t Utf8(const uint32_t* text, size_t length, char* out)
{
    size_t n = 0;
    for (size_t i = 0; i < length; i++)
    {
        uint32_t c = text[i];
        if (c < 0x80)
        {
            out[n++] = (char)c;
        }
        else if (c < 0x800)
        {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
        else
        {
            out[n++] = (char)(0xF0 | c >> 18);
            out[n++] = (char)(0x80 | (c >> 12 & 0x3F));
            out[n++] = (char)(0x80 | (c >> 6 & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    return n;
}

// NFD code points of a well-formed UTF-8 text; 0 when too long.
static size_t Nfd(const char* text, size_t length, uint32_t* out)
{
    char nfd[4 * maxText * 4];
    size_t needed = 0;
    muniTextResult r =
        muniNormalize(text, length, muni_nfd, muni_convertReplace, nfd, sizeof(nfd), &needed);
    if (r.status != muni_success)
    {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < needed && n < 4 * maxText;)
    {
        unsigned char b = (unsigned char)nfd[i];
        size_t extra = b < 0x80 ? 0 : b < 0xE0 ? 1 : b < 0xF0 ? 2 : 3;
        uint32_t c = extra == 0 ? b : b & (0x3F >> extra);
        for (size_t k = 1; k <= extra; k++)
        {
            c = c << 6 | ((unsigned char)nfd[i + k] & 0x3F);
        }
        out[n++] = c;
        i += 1 + extra;
    }
    return n;
}

typedef struct Check
{
    const CollationRoot* root;
    CollationMappings* mappings;
    size_t seen;
    size_t wrong;
} Check;

// One string through both; false when they differ.
static bool Same(Check* k, const uint32_t* text, size_t length)
{
    char utf8[4 * maxText * 4];
    size_t size = Utf8(text, length, utf8);
    uint32_t nfd[4 * maxText];
    size_t n = Nfd(utf8, size, nfd);
    if (n == 0)
    {
        return true;
    }
    CollationIterator it;
    mlocCollationIteratorStart(&it, k->root, (const uint8_t*)utf8, size);
    uint64_t theirs[collationMaxExpansion * 4];
    uint32_t count = 0;
    uint64_t e = 0;
    while (mlocCollationNextElement(&it, &e))
    {
        if (e != 0 && count < collationMaxExpansion)
        {
            theirs[count] = e;
        }
        count += e != 0;
    }
    uint64_t ours[collationMaxExpansion];
    uint32_t mine = CollationMappingsElements(k->mappings, nfd, (uint32_t)n, 0, ours, 0);
    k->seen++;
    uint32_t kept = count < collationMaxExpansion ? count : collationMaxExpansion;
    bool same = mine == count && memcmp(ours, theirs, kept * sizeof *ours) == 0;
    if (!same && k->wrong++ < 10)
    {
        printf("  differs:");
        for (size_t i = 0; i < n; i++)
        {
            printf(" %04X", nfd[i]);
        }
        printf(" (%u elements against %u)\n", mine, count);
    }
    return same;
}

static uint32_t s_seed = 1;

static uint32_t Random(uint32_t below)
{
    s_seed = s_seed * 1103515245U + 12345U;
    return (s_seed >> 8) % below;
}

static void RandomStrings(Check* k, const RootTable* t, size_t count)
{
    uint32_t* marks = malloc(t->count * sizeof *marks);
    size_t markCount = 0;
    for (size_t i = 0; marks != nullptr && i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        for (uint32_t j = 0; j < m->textLength; j++)
        {
            if (muniGetCombiningClass(m->text[j]) != 0 && markCount < t->count)
            {
                marks[markCount++] = m->text[j];
            }
        }
    }
    for (size_t s = 0; markCount != 0 && s < count; s++)
    {
        uint32_t text[maxText];
        size_t length = 0;
        uint32_t pieces = 1 + Random(5);
        for (uint32_t p = 0; p < pieces; p++)
        {
            const RootMapping* m = &t->mappings[Random((uint32_t)t->count)];
            for (uint32_t j = 0; j < m->prefixLength && length < maxText; j++)
            {
                text[length++] = m->prefix[j];
            }
            for (uint32_t j = 0; j < m->textLength && length < maxText; j++)
            {
                text[length++] = m->text[j];
            }
            for (uint32_t j = Random(3); j > 0 && length < maxText; j--)
            {
                text[length++] = marks[Random((uint32_t)markCount)];
            }
        }
        Same(k, text, length);
    }
    free(marks);
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <FractionalUCA.txt> [RANDOM]\n", argv[0]);
        return 2;
    }
    size_t randomCount = argc > 2 ? strtoul(argv[2], nullptr, 10) : 300000;
    RootTable t = {0};
    char error[256] = "";
    if (!RootRead(argv[1], &t, error, sizeof(error)))
    {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    size_t size = 0;
    uint8_t* bytes = CollationRootUnit(&t, &size, error, sizeof(error));
    CollationRoot root;
    CollationMappings mappings;
    if (bytes == nullptr || !mlocReadCollationRoot((DataSpan){bytes, (uint32_t)size}, &root) ||
        !CollationMappingsInit(&mappings, &t))
    {
        fprintf(stderr, "%s\n", bytes == nullptr ? error : "the root does not read");
        free(bytes);
        RootFree(&t);
        return 1;
    }
    Check k = {&root, &mappings, 0, 0};
    for (uint32_t c = 0; c <= 0x10FFFF; c++)
    {
        if (c < 0xD800 || c > 0xDFFF)
        {
            Same(&k, &c, 1);
        }
    }
    size_t alone = k.seen;
    for (size_t i = 0; i < t.count; i++)
    {
        const RootMapping* m = &t.mappings[i];
        uint32_t text[2 * collationMaxContext + 1];
        memcpy(text, m->prefix, m->prefixLength * sizeof *text);
        memcpy(text + m->prefixLength, m->text, m->textLength * sizeof *text);
        Same(&k, text, m->prefixLength + m->textLength);
    }
    size_t mapped = k.seen - alone;
    RandomStrings(&k, &t, randomCount);
    printf("mappings: %zu code points, %zu mappings, %zu random strings; %zu differ\n", alone,
           mapped, k.seen - alone - mapped, k.wrong);
    CollationMappingsFree(&mappings);
    free(bytes);
    RootFree(&t);
    return k.wrong == 0 ? 0 : 1;
}
