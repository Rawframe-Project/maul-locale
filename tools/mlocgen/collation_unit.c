// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root collation unit (collation_unit.h).

#include "collation_unit.h"

#include "collation_tables.h"
#include "table_gen.h"
#include "trie_gen.h"

#include "maul-unicode/normalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Words
{
    uint64_t* items;
    size_t count;
    size_t capacity;
} Words;

static size_t Push(Words* w, uint64_t value)
{
    if (w->count == w->capacity)
    {
        w->capacity = w->capacity == 0 ? 1024 : w->capacity * 2;
        uint64_t* grown = realloc(w->items, w->capacity * sizeof(uint64_t));
        if (grown == nullptr)
        {
            GenDie("collation unit", "out of memory");
        }
        w->items = grown;
    }
    w->items[w->count] = value;
    return w->count++;
}

typedef struct Builder
{
    Words ces;
    Words ce32s;
    Words contexts;
    Words listStarts;
    uint32_t* values;
    char* error;
    size_t errorSize;
} Builder;

bool CollationLeftOut(uint32_t codePoint)
{
    if ((codePoint >= 0xAC00 && codePoint <= 0xD7A3) ||
        (codePoint >= 0xD800 && codePoint <= 0xDFFF))
    {
        return true;
    }
    char text[4];
    size_t n = codePoint < 0x80 ? 1 : codePoint < 0x800 ? 2 : codePoint < 0x10000 ? 3 : 4;
    static const uint8_t lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
    for (size_t i = n; i-- > 1;)
    {
        text[i] = (char)(0x80 | ((codePoint >> (6 * (n - 1 - i))) & 0x3F));
    }
    text[0] = (char)(lead[n] | (codePoint >> (6 * (n - 1))));
    muniQuickCheck answer = muni_quickCheckYes;
    return muniCheckNormalization(text, n, muni_nfd, &answer).status == muni_success &&
           answer != muni_quickCheckYes;
}

// The CE32 of one element, when one holds it: simple, long primary or
// long secondary.
static bool OneCe32(uint64_t element, uint32_t* out)
{
    uint32_t p = (uint32_t)(element >> 32);
    uint32_t lower = (uint32_t)element;
    if ((p & 0xFFFF) == 0 && (lower & 0x00FF00FF) == 0 && ((lower >> 8) & 0xFF) < 0xC0)
    {
        *out = p | (lower >> 16 & 0xFF00) | (lower >> 8 & 0xFF);
        return true;
    }
    if ((p & 0xFF) == 0 && p != 0 && lower == collationCommonSecondaryTertiary)
    {
        *out = p | collationSpecialByte | collationTagLongPrimary;
        return true;
    }
    if (p == 0 && (lower & 0xFF) == 0)
    {
        *out = (lower & 0xFFFF0000) | (lower & 0xFF00) | collationSpecialByte |
               collationTagLongSecondary;
        return true;
    }
    return false;
}

static uint32_t Special(uint32_t tag, size_t index, uint32_t length)
{
    return (uint32_t)index << 13 | length << 8 | collationSpecialByte | tag;
}

// The CE32 of a sequence of elements: one element's own when it fits,
// else an expansion of CE32s when each fits one, else of elements.
static uint32_t Encode(Builder* b, const uint64_t* elements, uint32_t count)
{
    uint32_t one = 0;
    if (count == 1 && OneCe32(elements[0], &one))
    {
        return one;
    }
    bool small = true;
    for (uint32_t i = 0; i < count; i++)
    {
        small = small && OneCe32(elements[i], &one);
    }
    size_t index = small ? b->ce32s.count : b->ces.count;
    for (uint32_t i = 0; i < count; i++)
    {
        if (small)
        {
            (void)OneCe32(elements[i], &one);
            Push(&b->ce32s, one);
        }
        else
        {
            Push(&b->ces, elements[i]);
        }
    }
    return Special(small ? collationTagExpansion32 : collationTagExpansion, index, count);
}

// ICU's diffThreeBytePrimaries under one lead byte: how many steps of
// AddToPrimary lead from one primary to another. A second byte step is
// 254 third byte steps whether the lead byte compresses or not, so only
// the lead byte's own count, never needed here, depends on it.
static int64_t Distance(uint32_t p1, uint32_t p2)
{
    int64_t a = (int64_t)((p1 >> 8) & 0xFF) + 254 * (int64_t)((p1 >> 16) & 0xFF);
    int64_t b = (int64_t)((p2 >> 8) & 0xFF) + 254 * (int64_t)((p2 >> 16) & 0xFF);
    return b - a;
}

static bool IsLongPrimary(uint32_t ce32)
{
    return mlocCe32IsSpecial(ce32) && ce32 != collationImplicitCe32 &&
           mlocCe32Tag(ce32) == collationTagLongPrimary;
}

// Sets an offset over a range where it saves trie blocks, as ICU's
// maybeSetPrimaryRange does for its blocks: a step from 2 to 127, and
// three block boundaries crossed, or one or two with four code points
// on either side.
static void MaybeRange(Builder* b, const RootTable* t, uint32_t start, uint32_t end, int64_t step)
{
    uint32_t primary = b->values[start] & 0xFFFFFF00;
    uint32_t delta = (end >> 6) - (start >> 6);
    if (step < 2 || step > 0x7F ||
        !(delta >= 3 || (delta > 0 && (start & 63) <= 60 && (end & 63) >= 3)))
    {
        return;
    }
    bool compressible = t->compressible[primary >> 24] != 0;
    size_t index = Push(&b->ces, (uint64_t)primary << 32 | (uint64_t)start << 8 |
                                     (compressible ? 0x80 : 0) | (uint64_t)step);
    for (uint32_t c = start; c <= end; c++)
    {
        b->values[c] = Special(collationTagOffset, index, 0);
    }
}

// Runs of code points from U+0180 whose long primaries rise by one step
// under one lead byte, as ICU's genuca finds them, become offsets.
static void OffsetRanges(Builder* b, const RootTable* t)
{
    uint32_t first = 0;
    uint32_t last = 0;
    int64_t step = -1;
    bool open = false;
    for (uint32_t c = 0x180; c <= trieMaxCodePoint + 1; c++)
    {
        uint32_t ce32 = c <= trieMaxCodePoint ? b->values[c] : collationImplicitCe32;
        bool longPrimary = IsLongPrimary(ce32);
        uint32_t p = ce32 & 0xFFFFFF00;
        uint32_t lastP = b->values[last] & 0xFFFFFF00;
        bool joins = false;
        if (open && longPrimary && c == last + 1 && p > lastP && (p >> 24) == (lastP >> 24))
        {
            int64_t d = Distance(lastP, p);
            joins = first == last ? d >= 2 : d == step;
            step = first == last && joins ? d : step;
        }
        if (joins)
        {
            last = c;
            continue;
        }
        if (open && first != last)
        {
            MaybeRange(b, t, first, last, step);
        }
        open = longPrimary;
        first = last = c;
        step = -1;
    }
}

static int Fail(Builder* b, const char* what, uint32_t codePoint)
{
    snprintf(b->error, b->errorSize, "U+%04X: %s", (unsigned)codePoint, what);
    return 0;
}

// Han: one offset element a range, its CE32 at every code point of it.
static void HanRanges(Builder* b, const RootTable* t)
{
    for (size_t i = 0; i < t->hanRangeCount; i++)
    {
        uint32_t start = t->hanRanges[2 * i];
        uint32_t end = t->hanRanges[2 * i + 1];
        uint64_t first = RootHanElement(t, start);
        size_t index = Push(&b->ces, (first & 0xFFFFFFFF00000000ULL) | (uint64_t)start << 8 | 2);
        for (uint32_t c = start; c <= end; c++)
        {
            b->values[c] = Special(collationTagOffset, index, 0);
        }
    }
}

typedef struct Entry
{
    uint32_t text[collationMaxContext];
    uint32_t length;
    uint32_t ce32;
} Entry;

static int ByText(const void* x, const void* y)
{
    const Entry* a = x;
    const Entry* b = y;
    for (uint32_t i = 0; i < a->length && i < b->length; i++)
    {
        if (a->text[i] != b->text[i])
        {
            return a->text[i] < b->text[i] ? -1 : 1;
        }
    }
    return a->length == b->length ? 0 : a->length < b->length ? -1 : 1;
}

// A context list for the code point: its default, then its entries in
// order; the trie value becomes its tag with the list's number.
static void Context(Builder* b, uint32_t codePoint, uint32_t tag, Entry* entries, size_t count)
{
    qsort(entries, count, sizeof(Entry), ByText);
    size_t number = Push(&b->listStarts, b->contexts.count);
    Push(&b->contexts, b->values[codePoint]);
    Push(&b->contexts, count);
    for (size_t i = 0; i < count; i++)
    {
        Push(&b->contexts, entries[i].length);
        for (uint32_t k = 0; k < entries[i].length; k++)
        {
            Push(&b->contexts, entries[i].text[k]);
        }
        Push(&b->contexts, entries[i].ce32);
    }
    b->values[codePoint] = Special(tag, number, 0);
}

// A mapping's place among those of one code point's contexts.
typedef struct Member
{
    uint32_t codePoint;
    uint32_t mapping;
} Member;

static int ByCodePoint(const void* x, const void* y)
{
    const Member* a = x;
    const Member* b = y;
    return a->codePoint != b->codePoint ? (a->codePoint < b->codePoint ? -1 : 1)
           : a->mapping != b->mapping   ? (a->mapping < b->mapping ? -1 : 1)
                                        : 0;
}

// The entry of one mapping in its code point's context list.
static bool MakeEntry(Builder* b, const RootMapping* m, bool prefixes, Entry* e)
{
    uint32_t length = prefixes ? m->prefixLength : m->textLength - 1;
    if (length > collationMaxContext || (prefixes && m->textLength != 1))
    {
        return false;
    }
    *e = (Entry){.length = length, .ce32 = Encode(b, m->elements, m->elementCount)};
    for (uint32_t k = 0; k < length; k++)
    {
        // A prefix is matched backwards from the code point.
        e->text[k] = prefixes ? m->prefix[length - 1 - k] : m->text[1 + k];
    }
    return true;
}

// The contexts starting with, or ending on, each code point, gathered
// from the mappings; 0 with a message for one the unit cannot hold.
static int Contexts(Builder* b, const RootTable* t, bool prefixes)
{
    Member* members = GenAllocate(t->count * sizeof(Member) + sizeof(Member));
    size_t count = 0;
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        bool mine = prefixes ? m->prefixLength > 0 : m->prefixLength == 0 && m->textLength > 1;
        if (mine && !CollationLeftOut(m->text[0]))
        {
            members[count++] = (Member){m->text[0], (uint32_t)i};
        }
    }
    qsort(members, count, sizeof(Member), ByCodePoint);
    Entry* entries = GenAllocate(count * sizeof(Entry) + sizeof(Entry));
    int ok = 1;
    for (size_t start = 0; ok && start < count;)
    {
        uint32_t c = members[start].codePoint;
        size_t n = 0;
        for (; start + n < count && members[start + n].codePoint == c; n++)
        {
            if (!MakeEntry(b, &t->mappings[members[start + n].mapping], prefixes, &entries[n]))
            {
                ok = Fail(b, "a context the unit cannot hold", c);
                break;
            }
        }
        uint32_t held = b->values[c];
        if (ok && mlocCe32IsSpecial(held) &&
            (mlocCe32Tag(held) == collationTagPrefix ||
             mlocCe32Tag(held) == collationTagContraction) &&
            held != collationImplicitCe32)
        {
            ok = Fail(b, "a prefix and a contraction on one code point", c);
        }
        if (ok)
        {
            Context(b, c, prefixes ? collationTagPrefix : collationTagContraction, entries, n);
        }
        start += n;
    }
    free(entries);
    free(members);
    return ok;
}

static uint8_t* Write(const Builder* b, size_t* sizeOut)
{
    size_t trieSize = 0;
    uint8_t* trie = TrieBuild(b->values, &trieSize);
    size_t padded = (trieSize + 7) / 8 * 8;
    size_t size = collationHead + padded + b->ces.count * 8 + b->ce32s.count * 4 +
                  (b->listStarts.count + 1) * 4 + b->contexts.count * 4;
    uint8_t* out = GenAllocate(size);
    GenPut(out + 4, b->ce32s.count, 4);
    GenPut(out + 8, b->ces.count, 4);
    GenPut(out + 12, b->contexts.count, 4);
    GenPut(out + 16, b->listStarts.count, 4);
    memcpy(out + collationHead, trie, trieSize);
    uint8_t* p = out + collationHead + padded;
    for (size_t i = 0; i < b->ces.count; i++, p += 8)
    {
        GenPut(p, b->ces.items[i], 8);
    }
    for (size_t i = 0; i < b->ce32s.count; i++, p += 4)
    {
        GenPut(p, b->ce32s.items[i], 4);
    }
    for (size_t i = 0; i < b->listStarts.count; i++, p += 4)
    {
        GenPut(p, b->listStarts.items[i], 4);
    }
    GenPut(p, b->contexts.count, 4);
    p += 4;
    for (size_t i = 0; i < b->contexts.count; i++, p += 4)
    {
        GenPut(p, b->contexts.items[i], 4);
    }
    free(trie);
    *sizeOut = size;
    return out;
}

uint8_t* CollationRootUnit(const RootTable* table, size_t* sizeOut, char* error, size_t errorSize)
{
    Builder b = {.error = error, .errorSize = errorSize};
    b.values = GenAllocate((size_t)trieCodePoints * 4);
    for (uint32_t c = 0; c <= trieMaxCodePoint; c++)
    {
        b.values[c] = collationImplicitCe32;
    }
    HanRanges(&b, table);
    b.values[0xFFFD] = collationFffdCe32;
    b.values[0xFFFF] = collationFfffCe32;
    for (size_t i = 0; i < table->count; i++)
    {
        const RootMapping* m = &table->mappings[i];
        if (m->elementCount > collationMaxExpansion)
        {
            free(b.values);
            Fail(&b, "more elements than an expansion holds", m->text[0]);
            return nullptr;
        }
        if (m->prefixLength == 0 && m->textLength == 1 && !CollationLeftOut(m->text[0]))
        {
            b.values[m->text[0]] = Encode(&b, m->elements, m->elementCount);
        }
    }
    OffsetRanges(&b, table);
    uint8_t* out = nullptr;
    if (Contexts(&b, table, false) && Contexts(&b, table, true))
    {
        out = Write(&b, sizeOut);
        GenPut(out, (uint64_t)table->version[0] << 16 | table->version[1] << 8 | table->version[2],
               4);
    }
    free(b.values);
    free(b.ces.items);
    free(b.ce32s.items);
    free(b.contexts.items);
    free(b.listStarts.items);
    return out;
}
