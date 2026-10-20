// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A built tailoring written as a unit (collation_tailoring.h).

#include "collation_tailoring.h"

#include "collation_encode.h"
#include "collation_tables.h"
#include "table_gen.h"
#include "trie_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Writer
{
    CollationWords ces;
    CollationWords ce32s;
    CollationWords contexts;
    CollationWords listStarts;
    const CollationMappings* m;
    uint32_t mapped;
} Writer;

typedef struct Entry
{
    uint32_t text[collationMaxContext];
    uint32_t length;
    uint32_t ce32;
} Entry;

// A code point's mappings of one prefix.
typedef struct Group
{
    const uint32_t* prefix;
    uint32_t prefixLength;
    uint32_t ce32;
} Group;

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

static uint32_t Encode(Writer* w, const CollationMapping* mapping)
{
    return CollationEncode(&w->ces, &w->ce32s, w->m->elements + mapping->elementStart,
                           mapping->elementCount);
}

// A context list: its default, then its entries in order; the CE32 of
// the tag naming it.
static uint32_t List(Writer* w, uint32_t tag, uint32_t fallback, Entry* entries, size_t count)
{
    qsort(entries, count, sizeof(Entry), ByText);
    size_t number = CollationPush(&w->listStarts, w->contexts.count);
    CollationPush(&w->contexts, fallback);
    CollationPush(&w->contexts, count);
    for (size_t i = 0; i < count; i++)
    {
        CollationPush(&w->contexts, entries[i].length);
        for (uint32_t k = 0; k < entries[i].length; k++)
        {
            CollationPush(&w->contexts, entries[i].text[k]);
        }
        CollationPush(&w->contexts, entries[i].ce32);
    }
    return CollationSpecial(tag, number, 0);
}

static bool SamePrefix(const uint32_t* a, uint32_t aLength, const uint32_t* b, uint32_t bLength)
{
    return aLength == bLength && (aLength == 0 || memcmp(a, b, aLength * sizeof *a) == 0);
}

// Whether a shorter prefix is the end of a longer one.
static bool EndsWith(const Group* longer, const Group* shorter)
{
    uint32_t n = shorter->prefixLength;
    return n == 0 || memcmp(longer->prefix + longer->prefixLength - n, shorter->prefix,
                            n * sizeof *shorter->prefix) == 0;
}

static int ByPrefixLength(const void* x, const void* y)
{
    const Group* a = x;
    const Group* b = y;
    if (a->prefixLength != b->prefixLength)
    {
        return a->prefixLength < b->prefixLength ? -1 : 1;
    }
    for (uint32_t i = 0; i < a->prefixLength; i++)
    {
        if (a->prefix[i] != b->prefix[i])
        {
            return a->prefix[i] < b->prefix[i] ? -1 : 1;
        }
    }
    return 0;
}

// One prefix's CE32: its mapping alone, or a list of its contractions
// whose default is that mapping or the longest shorter prefix's CE32.
static bool GroupCe32(Writer* w, const CollationMapping* const* list, size_t count, Group* groups,
                      size_t g, Entry* entries)
{
    Group* group = &groups[g];
    const CollationMapping* alone = nullptr;
    size_t n = 0;
    for (size_t i = 0; i < count; i++)
    {
        const CollationMapping* t = list[i];
        if (!SamePrefix(t->prefix, t->prefixLength, group->prefix, group->prefixLength))
        {
            continue;
        }
        if (t->textLength == 1)
        {
            alone = t;
            continue;
        }
        entries[n] = (Entry){.length = t->textLength - 1, .ce32 = Encode(w, t)};
        memcpy(entries[n].text, t->text + 1, (t->textLength - 1) * sizeof *t->text);
        n++;
    }
    if (n == 0)
    {
        group->ce32 = alone != nullptr ? Encode(w, alone) : collationFallbackCe32;
        return alone != nullptr;
    }
    uint32_t fallback = collationFallbackCe32;
    if (alone != nullptr)
    {
        fallback = Encode(w, alone);
    }
    else
    {
        for (size_t h = g; h-- > 0 && fallback == collationFallbackCe32;)
        {
            if (groups[h].prefixLength < group->prefixLength && EndsWith(group, &groups[h]))
            {
                fallback = groups[h].ce32;
            }
        }
    }
    group->ce32 = List(w, collationTagContraction, fallback, entries, n);
    return fallback != collationFallbackCe32;
}

// The trie value of one tailored code point.
static bool CodePoint(Writer* w, uint32_t codePoint, uint32_t* valueOut)
{
    const CollationMappings* m = w->m;
    size_t count = 0;
    for (int32_t i = CollationMappingsFirst(m, codePoint); i >= 0; i = m->mappings[i].next)
    {
        count++;
    }
    const CollationMapping** list = (const CollationMapping**)malloc((count + 1) * sizeof *list);
    Group* groups = malloc((count + 1) * sizeof *groups);
    Entry* entries = malloc((count + 1) * sizeof *entries);
    if (list == nullptr || groups == nullptr || entries == nullptr)
    {
        GenDie("collation tailoring", "out of memory");
    }
    size_t n = 0;
    size_t groupCount = 0;
    for (int32_t i = CollationMappingsFirst(m, codePoint); i >= 0; i = m->mappings[i].next)
    {
        const CollationMapping* t = &m->mappings[i];
        list[n++] = t;
        bool seen = false;
        for (size_t g = 0; g < groupCount && !seen; g++)
        {
            seen = SamePrefix(groups[g].prefix, groups[g].prefixLength, t->prefix, t->prefixLength);
        }
        if (!seen)
        {
            groups[groupCount++] = (Group){t->prefix, t->prefixLength, 0};
        }
    }
    qsort(groups, groupCount, sizeof(Group), ByPrefixLength);
    bool ok = groupCount > 0 && groups[0].prefixLength == 0;
    for (size_t g = 0; ok && g < groupCount; g++)
    {
        ok = GroupCe32(w, list, n, groups, g, entries);
    }
    if (ok && groupCount == 1)
    {
        *valueOut = groups[0].ce32;
    }
    else if (ok)
    {
        for (size_t g = 1; g < groupCount; g++)
        {
            entries[g - 1] = (Entry){.length = groups[g].prefixLength, .ce32 = groups[g].ce32};
            for (uint32_t k = 0; k < groups[g].prefixLength; k++)
            {
                // A prefix is matched backwards from the code point.
                entries[g - 1].text[k] = groups[g].prefix[groups[g].prefixLength - 1 - k];
            }
        }
        *valueOut = List(w, collationTagPrefix, groups[0].ce32, entries, groupCount - 1);
    }
    free((void*)list);
    free(groups);
    free(entries);
    return ok;
}

// The rules' settings as the head's word (mlocCollatorOptions' numbers).
static uint32_t Settings(const CollationRules* r)
{
    uint32_t s = 0;
    s |= r->strength == ruleUnset ? 0 : r->strength;
    s |= (r->alternate == ruleUnset ? 0 : r->alternate + 1U) << 3;
    s |= (r->maxVariable == ruleUnset ? 0 : r->maxVariable + 1U) << 5;
    s |= (r->caseFirst == ruleUnset ? 0 : r->caseFirst + 1U) << 8;
    s |= (r->caseLevel == ruleUnset ? 0 : r->caseLevel + 1U) << 10;
    s |= (r->numeric == ruleUnset ? 0 : r->numeric + 1U) << 12;
    s |= (r->backwards == ruleUnset ? 0 : r->backwards + 1U) << 14;
    return s | (r->reorderGiven ? collationReorderGiven : 0);
}

static uint8_t* Write(const Writer* w, const uint32_t* values, const CollationRules* rules,
                      size_t* sizeOut)
{
    size_t trieSize = 0;
    uint8_t* trie = TrieBuild(values, &trieSize);
    size_t padded = (trieSize + 7) / 8 * 8;
    size_t reorderCount = rules == nullptr ? 0 : rules->reorderCount;
    size_t size = collationHead + padded + w->ces.count * 8 + w->ce32s.count * 4 +
                  (w->listStarts.count + 1) * 4 + w->contexts.count * 4 + reorderCount * 4;
    uint8_t* out = GenAllocate(size);
    GenPut(out, w->ce32s.count, 4);
    GenPut(out + 4, w->ces.count, 4);
    GenPut(out + 8, w->contexts.count, 4);
    GenPut(out + 12, w->listStarts.count, 4);
    GenPut(out + 16, rules == nullptr ? 0 : Settings(rules), 4);
    GenPut(out + 20, reorderCount, 4);
    GenPut(out + 24, w->mapped, 4);
    memcpy(out + collationHead, trie, trieSize);
    uint8_t* p = out + collationHead + padded;
    for (size_t i = 0; i < w->ces.count; i++, p += 8)
    {
        GenPut(p, w->ces.items[i], 8);
    }
    for (size_t i = 0; i < w->ce32s.count; i++, p += 4)
    {
        GenPut(p, w->ce32s.items[i], 4);
    }
    for (size_t i = 0; i < w->listStarts.count; i++, p += 4)
    {
        GenPut(p, w->listStarts.items[i], 4);
    }
    GenPut(p, w->contexts.count, 4);
    p += 4;
    for (size_t i = 0; i < w->contexts.count; i++, p += 4)
    {
        GenPut(p, w->contexts.items[i], 4);
    }
    for (size_t i = 0; i < reorderCount; i++, p += 4)
    {
        GenPut(p, rules->reorder[i], 4);
    }
    free(trie);
    *sizeOut = size;
    return out;
}

uint8_t* CollationTailoringUnit(const CollationMappings* mappings, const CollationRules* rules,
                                size_t* sizeOut, char* error, size_t errorSize)
{
    Writer w = {.m = mappings};
    uint32_t* values = GenAllocate((size_t)trieCodePoints * 4);
    for (uint32_t c = 0; c <= trieMaxCodePoint; c++)
    {
        values[c] = collationFallbackCe32;
    }
    bool ok = true;
    for (size_t s = 0; ok && s < mappings->slotCount; s++)
    {
        const CollationMappingSlot* slot = &mappings->slots[s];
        if (slot->head != 0)
        {
            ok = CodePoint(&w, slot->codePoint, &values[slot->codePoint]);
            w.mapped++;
            if (!ok)
            {
                snprintf(error, errorSize, "U+%04X: a prefix with no mapping to fall back on",
                         slot->codePoint);
            }
        }
    }
    uint8_t* out = ok ? Write(&w, values, rules, sizeOut) : nullptr;
    free(values);
    free(w.ces.items);
    free(w.ce32s.items);
    free(w.contexts.items);
    free(w.listStarts.items);
    return out;
}

static int ByCode(const void* x, const void* y)
{
    return strcmp(((const CollationTypeBody*)x)->code, ((const CollationTypeBody*)y)->code);
}

// Writes a name's bytes, no NUL after, and returns their count.
static size_t PutName(uint8_t* to, const char* name)
{
    size_t length = strlen(name);
    for (size_t i = 0; i < length; i++)
    {
        to[i] = (uint8_t)name[i];
    }
    return length;
}

uint8_t* CollationTypesUnit(CollationTypeBody* types, size_t count, const char* defaultCode,
                            size_t* sizeOut)
{
    if (count > 0)
    {
        qsort(types, count, sizeof(CollationTypeBody), ByCode);
    }
    size_t names = strlen(defaultCode);
    for (size_t i = 0; i < count; i++)
    {
        names += strlen(types[i].code);
    }
    size_t namesAt = collationTypesHead + count * collationTypeSize;
    size_t at = (namesAt + names + 7) / 8 * 8;
    size_t size = at;
    for (size_t i = 0; i < count; i++)
    {
        size += (types[i].size + 7) / 8 * 8;
    }
    uint8_t* out = GenAllocate(size);
    size_t name = PutName(out + namesAt, defaultCode);
    GenPut(out, count, 4);
    GenPut(out + 4, name, 4);
    for (size_t i = 0; i < count; i++)
    {
        uint8_t* entry = out + collationTypesHead + i * collationTypeSize;
        size_t length = PutName(out + namesAt + name, types[i].code);
        GenPut(entry, name, 4);
        GenPut(entry + 4, length, 4);
        GenPut(entry + 8, at, 4);
        GenPut(entry + 12, types[i].size, 4);
        name += length;
        memcpy(out + at, types[i].body, types[i].size);
        at += (types[i].size + 7) / 8 * 8;
    }
    *sizeOut = size;
    return out;
}
