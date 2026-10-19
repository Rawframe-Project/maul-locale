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
// runtime's fixed weights. Then the unit built from the root reads back
// every mapping it keeps, and the element iterator gives, for every
// mapping spelled as text (a prefix mapping after its prefix's own
// elements), the elements the file lists, completely ignorable ones
// aside: the kept ones through the
// unit's paths, the left-out ones through NFD. CI runs it on the pinned
// release.
//
// usage: conform_collation_root <FractionalUCA.txt> <allkeys_CLDR.txt>

// mlocgen_core defines _CRT_SECURE_NO_WARNINGS for Windows' fopen.

#include "collation_iterator.h"
#include "collation_root.h"
#include "collation_tables.h"
#include "collation_unit.h"

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

// The elements a CE32 of the unit stands for, a context's default taken.
static uint32_t Expand(const CollationRoot* r, uint32_t ce32, uint32_t codePoint, uint64_t* out)
{
    if (mlocCe32IsSpecial(ce32) &&
        (mlocCe32Tag(ce32) == collationTagPrefix || mlocCe32Tag(ce32) == collationTagContraction))
    {
        ce32 = mlocCollationContext(r, mlocCollationListStart(r, mlocCe32Index(ce32)));
    }
    uint64_t one = mlocCe32Element(ce32);
    if (!mlocCe32IsSpecial(ce32) || one != 0)
    {
        out[0] = one;
        return 1;
    }
    uint32_t tag = mlocCe32Tag(ce32);
    uint32_t index = mlocCe32Index(ce32);
    uint32_t length = mlocCe32Length(ce32);
    if (tag == collationTagOffset)
    {
        out[0] = mlocCollationOffsetElement(mlocCollationCe(r, index), codePoint);
        return 1;
    }
    for (uint32_t i = 0; i < length && tag == collationTagExpansion32; i++)
    {
        out[i] = mlocCe32Element(mlocCollationCe32(r, index + i));
    }
    for (uint32_t i = 0; i < length && tag == collationTagExpansion; i++)
    {
        out[i] = mlocCollationCe(r, index + i);
    }
    return tag == collationTagExpansion32 || tag == collationTagExpansion ? length : 0;
}

// The CE32 a context list gives for a string, or 0 when none matches.
static uint32_t Lookup(const CollationRoot* r, uint32_t ce32, const uint32_t* text, uint32_t length)
{
    uint32_t index = mlocCollationListStart(r, mlocCe32Index(ce32));
    uint32_t count = mlocCollationContext(r, index + 1);
    uint32_t at = index + 2;
    for (uint32_t e = 0; e < count; e++)
    {
        uint32_t n = mlocCollationContext(r, at);
        bool same = n == length;
        for (uint32_t k = 0; same && k < n; k++)
        {
            same = mlocCollationContext(r, at + 1 + k) == text[k];
        }
        if (same)
        {
            return mlocCollationContext(r, at + 1 + n);
        }
        at += 2 + n;
    }
    return 0;
}

// Every mapping the unit keeps, its elements read back from the unit.
// The iterator's elements of code points spelled as UTF-8, without the
// completely ignorable ones, which no level sees: NFD can add some, as
// U+1D165's after U+1D15E's notehead.
static uint32_t Iterate(const CollationRoot* r, const uint32_t* text, uint32_t length,
                        uint64_t* out, uint32_t capacity)
{
    uint8_t bytes[4 * 2 * rootMaxText];
    size_t size = 0;
    for (uint32_t i = 0; i < length; i++)
    {
        uint32_t c = text[i];
        if (c < 0x80)
        {
            bytes[size++] = (uint8_t)c;
        }
        else if (c < 0x800)
        {
            bytes[size++] = (uint8_t)(0xC0 | c >> 6);
            bytes[size++] = (uint8_t)(0x80 | (c & 0x3F));
        }
        else if (c < 0x10000)
        {
            bytes[size++] = (uint8_t)(0xE0 | c >> 12);
            bytes[size++] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
            bytes[size++] = (uint8_t)(0x80 | (c & 0x3F));
        }
        else
        {
            bytes[size++] = (uint8_t)(0xF0 | c >> 18);
            bytes[size++] = (uint8_t)(0x80 | (c >> 12 & 0x3F));
            bytes[size++] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
            bytes[size++] = (uint8_t)(0x80 | (c & 0x3F));
        }
    }
    CollationIterator it;
    mlocCollationIteratorStart(&it, r, bytes, size);
    uint32_t n = 0;
    uint64_t e = 0;
    while (n < capacity && mlocCollationNextElement(&it, &e))
    {
        out[n] = e;
        n += e != 0;
    }
    return n;
}

// Every mapping through the iterator; the counts of those that differ,
// kept and left out.
static bool CheckIterator(const RootTable* t, const CollationRoot* r)
{
    enum
    {
        capacity = 4 * rootMaxElements,
    };
    size_t wrong[2] = {0, 0};
    size_t seen[2] = {0, 0};
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        uint32_t text[2 * rootMaxText];
        uint32_t length = 0;
        for (uint32_t k = 0; k < m->prefixLength; k++)
        {
            text[length++] = m->prefix[k];
        }
        uint64_t want[capacity];
        uint32_t wanted = Iterate(r, text, length, want, capacity);
        for (uint32_t k = 0; k < m->textLength; k++)
        {
            text[length++] = m->text[k];
        }
        for (uint32_t k = 0; k < m->elementCount && wanted < capacity; k++)
        {
            want[wanted] = m->elements[k];
            wanted += m->elements[k] != 0;
        }
        uint64_t got[capacity];
        uint32_t n = Iterate(r, text, length, got, capacity);
        int out = CollationLeftOut(m->text[0]) ? 1 : 0;
        seen[out]++;
        if (n != wanted || memcmp(got, want, n * 8) != 0)
        {
            if (wrong[out]++ < 8)
            {
                printf("iterator: U+%04X%s (%u code points) gives otherwise\n",
                       (unsigned)m->text[0], out ? ", left out," : "", (unsigned)length);
            }
        }
    }
    uint64_t fffd[2];
    static const uint32_t replacement[] = {0xFFFD, 0xFFFF};
    bool fixed = Iterate(r, replacement, 1, fffd, 2) == 1 &&
                 fffd[0] == (0xFFFD000000000000ULL | collationCommonSecondaryTertiary) &&
                 Iterate(r, replacement + 1, 1, fffd, 2) == 1 &&
                 fffd[0] == (0xFFFF000000000000ULL | collationCommonSecondaryTertiary);
    printf("iterator: %zu kept mappings, %zu otherwise; %zu left out, %zu otherwise; U+FFFD "
           "and U+FFFF %s\n",
           seen[0], wrong[0], seen[1], wrong[1], fixed ? "fixed" : "wrong");
    return wrong[0] == 0 && wrong[1] == 0 && fixed;
}

static bool CheckUnit(const RootTable* t)
{
    size_t size = 0;
    char error[256] = "";
    uint8_t* bytes = CollationRootUnit(t, &size, error, sizeof(error));
    CollationRoot r;
    if (bytes == nullptr || !mlocReadCollationRoot((DataSpan){bytes, (uint32_t)size}, &r))
    {
        printf("unit: %s\n", bytes == nullptr ? error : "does not read back");
        free(bytes);
        return false;
    }
    size_t kept = 0;
    size_t wrong = 0;
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        if (CollationLeftOut(m->text[0]))
        {
            continue;
        }
        uint32_t ce32 = mlocTrieValue(&r.trie, m->text[0]);
        if (m->prefixLength > 0 || m->textLength > 1)
        {
            uint32_t context[rootMaxText];
            uint32_t n = m->prefixLength > 0 ? m->prefixLength : m->textLength - 1;
            for (uint32_t k = 0; k < n; k++)
            {
                context[k] = m->prefixLength > 0 ? m->prefix[n - 1 - k] : m->text[1 + k];
            }
            ce32 = Lookup(&r, ce32, context, n);
        }
        uint64_t elements[rootMaxElements];
        uint32_t count = Expand(&r, ce32, m->text[0], elements);
        kept++;
        if (count != m->elementCount || memcmp(elements, m->elements, count * 8) != 0)
        {
            if (wrong++ < 5)
            {
                printf("unit: U+%04X reads back otherwise\n", (unsigned)m->text[0]);
            }
        }
    }
    printf("unit: %zu bytes, %zu mappings kept, %zu read back otherwise, trie %u bytes, %u "
           "elements, %u expansion words, %u context words\n",
           size, kept, wrong, r.trie.size, r.ceCount, r.ce32Count, r.contextCount);
    bool iterated = CheckIterator(t, &r);
    free(bytes);
    return wrong == 0 && iterated;
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
    ok = CheckUnit(&t) && ok;
    free(s_single);
    RootFree(&t);
    return ok ? 0 : 1;
}
