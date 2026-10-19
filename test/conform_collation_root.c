// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root collation's elements, as the generator reads them from
// FractionalUCA.txt, against allkeys_CLDR.txt of the same CLDR release,
// the same root in UCA's integer weights (record mloc-0064). For every
// mapping of allkeys, ours has as many elements (an implicit pair
// [.FBxx][.xxxx] of allkeys being one), and the map from its weights to
// ours keeps their order and is one-to-one: the primaries; the
// secondaries under each primary; the tertiaries under each primary and
// secondary, since FractionalUCA.txt numbers a level within the one
// above it, and the tertiaries without the two case bits, which are a
// level of their own. Two shapes are set aside and counted: where
// allkeys follows an element with a variant's secondary element (011F
// and above), as for a CJK radical or a Meroitic hieroglyph's variant,
// FractionalUCA.txt has one element with its own secondary, whose
// primaries are still compared; and U+FFFD and U+FFFF take the
// runtime's fixed weights. CI runs it on
// the pinned release.
//
// usage: conform_collation_root <FractionalUCA.txt> <allkeys_CLDR.txt>

// The C runtime of Windows reports fopen as deprecated.
#define _CRT_SECURE_NO_WARNINGS

#include "collation_root.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Pair
{
    uint64_t ours;
    uint64_t theirs;
} Pair;

typedef struct Pairs
{
    Pair* items;
    size_t count;
    size_t capacity;
} Pairs;

static void Add(Pairs* p, uint64_t ours, uint64_t theirs)
{
    if (p->count == p->capacity)
    {
        p->capacity = p->capacity == 0 ? 4096 : p->capacity * 2;
        p->items = realloc(p->items, p->capacity * sizeof(Pair));
        if (p->items == nullptr)
        {
            exit(2);
        }
    }
    p->items[p->count++] = (Pair){ours, theirs};
}

static int ByOurs(const void* a, const void* b)
{
    const Pair* x = a;
    const Pair* y = b;
    return x->ours != y->ours       ? (x->ours < y->ours ? -1 : 1)
           : x->theirs != y->theirs ? (x->theirs < y->theirs ? -1 : 1)
                                    : 0;
}

// Whether the pairs are an order-preserving one-to-one map: sorted by
// ours, theirs rise exactly where ours rise.
static bool Monotone(Pairs* p, const char* level)
{
    qsort(p->items, p->count, sizeof(Pair), ByOurs);
    for (size_t i = 1; i < p->count; i++)
    {
        const Pair* a = &p->items[i - 1];
        const Pair* b = &p->items[i];
        bool sameOurs = a->ours == b->ours;
        bool sameTheirs = a->theirs == b->theirs;
        if (sameOurs != sameTheirs || (!sameOurs && b->theirs < a->theirs))
        {
            printf("%s: ours %016llX -> allkeys %016llX, then ours %016llX -> allkeys %016llX\n",
                   level, (unsigned long long)a->ours, (unsigned long long)a->theirs,
                   (unsigned long long)b->ours, (unsigned long long)b->theirs);
            return false;
        }
    }
    return true;
}

typedef struct Key
{
    uint32_t text[rootMaxText];
    uint32_t length;
    uint32_t primary[rootMaxElements];
    uint32_t secondary[rootMaxElements];
    uint32_t tertiary[rootMaxElements];
    uint32_t count;
} Key;

// "0041 0300 ; [.2485.0020.0008][.0000.0025.0002] # ..."
static bool ReadKey(const char* line, Key* k)
{
    *k = (Key){0};
    const char* s = line;
    char* end = nullptr;
    while (*s != ';')
    {
        unsigned long c = strtoul(s, &end, 16);
        if (end == s || k->length == rootMaxText)
        {
            return false;
        }
        k->text[k->length++] = (uint32_t)c;
        s = end;
        while (*s == ' ')
        {
            s++;
        }
    }
    for (s = strchr(s, '['); s != nullptr && k->count < rootMaxElements; s = strchr(s + 1, '['))
    {
        unsigned p = 0;
        unsigned sec = 0;
        unsigned ter = 0;
        if (sscanf(s + 2, "%x.%x.%x", &p, &sec, &ter) != 3)
        {
            return false;
        }
        // The second half of an implicit weight joins the first.
        if (k->count > 0 && p != 0 && sec == 0 && ter == 0 &&
            (k->primary[k->count - 1] >> 16) >= 0xFB00 &&
            (k->primary[k->count - 1] >> 16) <= 0xFBFF)
        {
            k->primary[k->count - 1] |= p;
            continue;
        }
        k->primary[k->count] = p << 16;
        k->secondary[k->count] = sec;
        k->tertiary[k->count] = ter;
        k->count++;
    }
    return k->count > 0;
}

// The count of a key's elements with each variant secondary (011F or
// above, primary 0) after a primary element folded into it, as
// FractionalUCA.txt writes them.
static uint32_t FoldedCount(const Key* k)
{
    uint32_t count = k->count;
    for (uint32_t i = 1; i < k->count; i++)
    {
        count -= k->primary[i] == 0 && k->primary[i - 1] != 0 && k->secondary[i] >= 0x011F;
    }
    return count;
}

// Our mapping for a key's text: of the whole text, or with its first
// code point as the prefix.
static const RootMapping* Find(const RootTable* t, const Key* k)
{
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        if (m->prefixLength == 0 && m->textLength == k->length &&
            memcmp(m->text, k->text, k->length * 4) == 0)
        {
            return m;
        }
        if (m->prefixLength == 1 && k->length == 1 + m->textLength && m->prefix[0] == k->text[0] &&
            memcmp(m->text, k->text + 1, m->textLength * 4) == 0)
        {
            return m;
        }
    }
    return nullptr;
}

// A lookup from a lone code point to its mapping, built once.
static const RootMapping** s_single;

static const RootMapping* FindFast(const RootTable* t, const Key* k)
{
    if (k->length == 1)
    {
        return s_single[k->text[0]];
    }
    return Find(t, k);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        fputs("usage: conform_collation_root <FractionalUCA.txt> <allkeys_CLDR.txt>\n", stderr);
        return 2;
    }
    RootTable t;
    char error[256];
    if (!RootRead(argv[1], &t, error, sizeof(error)))
    {
        fprintf(stderr, "%s\n", error);
        return 2;
    }
    s_single = calloc(0x110000, sizeof(*s_single));
    FILE* f = fopen(argv[2], "rb");
    if (s_single == nullptr || f == nullptr)
    {
        fputs("cannot read allkeys\n", stderr);
        return 2;
    }
    for (size_t i = t.count; i-- > 0;)
    {
        const RootMapping* m = &t.mappings[i];
        if (m->prefixLength == 0 && m->textLength == 1)
        {
            s_single[m->text[0]] = m;
        }
    }
    Pairs levels[3] = {{0}};
    static char line[4096];
    size_t keys = 0;
    size_t missing = 0;
    size_t counts = 0;
    size_t folded = 0;
    while (fgets(line, sizeof(line), f) != nullptr)
    {
        Key k;
        if (line[0] == '#' || line[0] == '@' || line[0] == '\n' || !ReadKey(line, &k))
        {
            continue;
        }
        keys++;
        const RootMapping* m = FindFast(&t, &k);
        RootMapping han = {0};
        if (m == nullptr && k.length == 1 && RootHanElement(&t, k.text[0]) != 0)
        {
            han.elementCount = 1;
            han.elements[0] = RootHanElement(&t, k.text[0]);
            m = &han;
        }
        // A prefix mapping gives the elements of what follows the prefix;
        // allkeys writes the prefix's own elements before them.
        RootMapping joined = {0};
        if (m != nullptr && m->prefixLength == 1 && s_single[m->prefix[0]] != nullptr)
        {
            const RootMapping* before = s_single[m->prefix[0]];
            joined.elementCount = before->elementCount + m->elementCount;
            memcpy(joined.elements, before->elements, before->elementCount * 8);
            memcpy(joined.elements + before->elementCount, m->elements, m->elementCount * 8);
            m = &joined;
        }
        RootMapping fixed = {0};
        if (m == nullptr && k.length == 1 && (k.text[0] == 0xFFFD || k.text[0] == 0xFFFF))
        {
            fixed.elementCount = 1;
            fixed.elements[0] = (uint64_t)(k.text[0] << 16) << 32 | 0x05000500;
            m = &fixed;
        }
        if (m == nullptr)
        {
            missing++;
            printf("no mapping: %s", line);
            continue;
        }
        if (m->elementCount != k.count && m->elementCount == FoldedCount(&k))
        {
            // Their primaries still pair with ours, the folded elements
            // left out.
            for (uint32_t i = 0, e = 0; i < k.count; i++)
            {
                bool fold =
                    i > 0 && k.primary[i] == 0 && k.primary[i - 1] != 0 && k.secondary[i] >= 0x011F;
                if (!fold)
                {
                    Add(&levels[0], m->elements[e++] >> 32, k.primary[i]);
                }
            }
            folded++;
            continue;
        }
        if (m->elementCount != k.count)
        {
            counts++;
            printf("%u elements against %u: %s", m->elementCount, k.count, line);
            continue;
        }
        for (uint32_t e = 0; e < k.count; e++)
        {
            uint64_t ce = m->elements[e];
            uint64_t p = ce >> 32;
            uint64_t sec = ce >> 16 & 0xFFFF;
            // The case bits are a level of their own, apart from the
            // tertiary weight unless case first or a case level asks.
            uint64_t ter = ce & 0x3F3F;
            Add(&levels[0], p, k.primary[e]);
            Add(&levels[1], p << 16 | sec, (uint64_t)k.primary[e] << 16 | k.secondary[e]);
            Add(&levels[2], p << 32 | sec << 16 | ter,
                (uint64_t)k.primary[e] << 32 | (uint64_t)k.secondary[e] << 16 | k.tertiary[e]);
        }
    }
    fclose(f);
    bool ok = missing == 0 && counts == 0;
    static const char* const names[] = {"primary", "secondary", "tertiary"};
    for (int l = 0; l < 3; l++)
    {
        ok = Monotone(&levels[l], names[l]) && ok;
        free(levels[l].items);
    }
    printf("%zu mappings of allkeys, %zu folded pairs set aside, %zu missing, %zu of another "
           "length: %s\n",
           keys, folded, missing, counts, ok ? "the same order at every level" : "FAILED");
    free(s_single);
    RootFree(&t);
    return ok ? 0 : 1;
}
