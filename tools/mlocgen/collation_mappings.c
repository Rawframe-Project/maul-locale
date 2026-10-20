// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A tailoring's mappings over the root (collation_mappings.h). The
// lookup follows the runtime's iterator (src/collation_iterator.c) over
// lists gathered from the root or the tailoring, and ICU4C's
// CollationDataBuilder where a prefix has contractions only.

#include "collation_mappings.h"

#include "collation_unit.h"

#include "maul-unicode/properties.h"

#include <stdlib.h>
#include <string.h>

enum
{
    firstSlots = 256,
};

static void* Grow(void* items, size_t* capacity, size_t needed, size_t size, size_t first)
{
    if (needed <= *capacity)
    {
        return items;
    }
    size_t grown = *capacity == 0 ? first : *capacity;
    while (grown < needed)
    {
        grown *= 2;
    }
    void* moved = realloc(items, grown * size);
    if (moved != nullptr)
    {
        *capacity = grown;
    }
    return moved;
}

static int ByCodePoint(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return x < y ? -1 : x > y;
}

int CollationMappingsInit(CollationMappings* m, const RootTable* root)
{
    *m = (CollationMappings){.root = root};
    m->rootIndex = malloc((root->count + 1) * sizeof *m->rootIndex);
    m->slots = calloc(firstSlots, sizeof *m->slots);
    if (m->rootIndex == nullptr || m->slots == nullptr)
    {
        CollationMappingsFree(m);
        return 0;
    }
    for (size_t i = 0; i < root->count; i++)
    {
        uint32_t c = root->mappings[i].text[0];
        if (!CollationLeftOut(c))
        {
            m->rootIndex[m->rootIndexCount++] = (uint64_t)c << 32 | i;
        }
    }
    qsort(m->rootIndex, m->rootIndexCount, sizeof *m->rootIndex, ByCodePoint);
    m->slotCount = firstSlots;
    return 1;
}

void CollationMappingsFree(CollationMappings* m)
{
    free(m->rootIndex);
    free(m->mappings);
    free(m->slots);
    free(m->elements);
    free(m->candidates);
    *m = (CollationMappings){0};
}

static size_t SlotOf(const CollationMappings* m, uint32_t codePoint)
{
    size_t i = (codePoint * 2654435761U) & (m->slotCount - 1);
    while (m->slots[i].head != 0 && m->slots[i].codePoint != codePoint)
    {
        i = (i + 1) & (m->slotCount - 1);
    }
    return i;
}

int32_t CollationMappingsFirst(const CollationMappings* m, uint32_t codePoint)
{
    return m->slots[SlotOf(m, codePoint)].head - 1;
}

// Doubles the slots once they are half used.
static int Rehash(CollationMappings* m)
{
    if (2 * (m->slotsUsed + 1) <= m->slotCount)
    {
        return 1;
    }
    CollationMappingSlot* old = m->slots;
    size_t oldCount = m->slotCount;
    CollationMappingSlot* slots = calloc(2 * oldCount, sizeof *slots);
    if (slots == nullptr)
    {
        return 0;
    }
    m->slots = slots;
    m->slotCount = 2 * oldCount;
    for (size_t i = 0; i < oldCount; i++)
    {
        if (old[i].head != 0)
        {
            m->slots[SlotOf(m, old[i].codePoint)] = old[i];
        }
    }
    free(old);
    return 1;
}

// The element the root gives a code point by rule.
static uint64_t RuleElement(const RootTable* t, uint32_t codePoint)
{
    uint64_t han = RootHanElement(t, codePoint);
    if (han != 0)
    {
        return han;
    }
    uint32_t p = codePoint == 0xFFFD   ? 0xFFFD0000
                 : codePoint == 0xFFFF ? 0xFFFF0000
                                       : mlocCollationUnassignedPrimary(codePoint);
    return (uint64_t)p << 32 | collationCommonSecondaryTertiary;
}

static int Push(CollationMappings* m, size_t* count, CollationCandidate c)
{
    CollationCandidate* grown =
        Grow(m->candidates, &m->candidateCapacity, *count + 1, sizeof *m->candidates, 64);
    if (grown == nullptr)
    {
        return 0;
    }
    m->candidates = grown;
    m->candidates[(*count)++] = c;
    return 1;
}

// The root's mappings of a code point, the rule's element as its own
// when it has none alone; their count, or 0 when out of memory.
static size_t GatherRoot(CollationMappings* m, uint32_t codePoint)
{
    size_t count = 0;
    bool alone = false;
    size_t lo = 0;
    size_t hi = m->rootIndexCount;
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        if (m->rootIndex[mid] >> 32 < codePoint)
        {
            lo = mid + 1;
        }
        else
        {
            hi = mid;
        }
    }
    for (size_t i = lo; i < m->rootIndexCount && m->rootIndex[i] >> 32 == codePoint; i++)
    {
        const RootMapping* r = &m->root->mappings[(uint32_t)m->rootIndex[i]];
        alone = alone || (r->prefixLength == 0 && r->textLength == 1);
        if (!Push(m, &count,
                  (CollationCandidate){r->prefix, r->prefixLength, r->text, r->textLength,
                                       r->elements, r->elementCount}))
        {
            return 0;
        }
    }
    if (!alone)
    {
        m->ruleElement = RuleElement(m->root, codePoint);
        m->ruleText = codePoint;
        if (!Push(m, &count, (CollationCandidate){nullptr, 0, &m->ruleText, 1, &m->ruleElement, 1}))
        {
            return 0;
        }
    }
    return count;
}

// The mappings a code point is read through: the tailoring's, or the
// root's.
static size_t Gather(CollationMappings* m, uint32_t codePoint)
{
    int32_t i = CollationMappingsFirst(m, codePoint);
    if (i < 0)
    {
        return GatherRoot(m, codePoint);
    }
    size_t count = 0;
    for (; i >= 0; i = m->mappings[i].next)
    {
        const CollationMapping* t = &m->mappings[i];
        if (!Push(m, &count,
                  (CollationCandidate){t->prefix, t->prefixLength, t->text, t->textLength,
                                       m->elements + t->elementStart, t->elementCount}))
        {
            return 0;
        }
    }
    return count;
}

static bool SameText(const uint32_t* a, uint32_t aLength, const uint32_t* b, uint32_t bLength)
{
    return aLength == bLength && (aLength == 0 || memcmp(a, b, aLength * sizeof *a) == 0);
}

static int AppendMapping(CollationMappings* m, uint32_t codePoint, const CollationCandidate* c)
{
    CollationMapping* grown =
        Grow(m->mappings, &m->capacity, m->count + 1, sizeof *m->mappings, 256);
    uint64_t* pool = grown == nullptr
                         ? nullptr
                         : Grow(m->elements, &m->elementCapacity,
                                m->elementCount + c->elementCount + 1, sizeof *m->elements, 1024);
    if (grown != nullptr)
    {
        m->mappings = grown;
    }
    if (pool == nullptr)
    {
        return 0;
    }
    m->elements = pool;
    CollationMapping* t = &m->mappings[m->count];
    *t = (CollationMapping){.prefixLength = c->prefixLength,
                            .textLength = c->textLength,
                            .elementStart = (uint32_t)m->elementCount,
                            .elementCount = c->elementCount};
    if (c->prefixLength != 0)
    {
        memcpy(t->prefix, c->prefix, c->prefixLength * sizeof *c->prefix);
    }
    memcpy(t->text, c->text, c->textLength * sizeof *c->text);
    if (c->elementCount != 0)
    {
        memcpy(m->elements + m->elementCount, c->elements, c->elementCount * sizeof *c->elements);
    }
    m->elementCount += c->elementCount;
    size_t slot = SlotOf(m, codePoint);
    t->next = m->slots[slot].head - 1;
    if (m->slots[slot].head == 0)
    {
        m->slots[slot].codePoint = codePoint;
        m->slotsUsed++;
    }
    m->slots[slot].head = (int32_t)++m->count;
    return 1;
}

// ICU's copyFromBaseCE32 on a code point's first mapping: the root's
// whole list when it or the new mapping has a context.
static int CopyRoot(CollationMappings* m, uint32_t codePoint, bool context)
{
    size_t count = GatherRoot(m, codePoint);
    if (count == 0)
    {
        return 0;
    }
    for (size_t i = 0; i < count && !context; i++)
    {
        context = m->candidates[i].prefixLength != 0 || m->candidates[i].textLength > 1;
    }
    if (!context)
    {
        return 1;
    }
    // The candidates point into the root, which stays put as they are
    // copied; the rule's element is copied before the next gathering.
    CollationCandidate* copied = malloc(count * sizeof *copied);
    if (copied == nullptr)
    {
        return 0;
    }
    memcpy(copied, m->candidates, count * sizeof *copied);
    int ok = 1;
    for (size_t i = count; ok && i-- > 0;)
    {
        ok = Rehash(m) && AppendMapping(m, codePoint, &copied[i]);
    }
    free(copied);
    return ok;
}

int CollationMappingsAdd(CollationMappings* m, const uint32_t* prefix, uint32_t prefixLength,
                         const uint32_t* text, uint32_t textLength, const uint64_t* elements,
                         uint32_t elementCount)
{
    if (textLength == 0 || textLength > collationMaxContext + 1 ||
        prefixLength > collationMaxContext || elementCount > collationMaxExpansion)
    {
        return 0;
    }
    uint32_t c = text[0];
    if (CollationMappingsFirst(m, c) < 0 && !CopyRoot(m, c, prefixLength != 0 || textLength > 1))
    {
        return 0;
    }
    for (int32_t i = CollationMappingsFirst(m, c); i >= 0; i = m->mappings[i].next)
    {
        CollationMapping* t = &m->mappings[i];
        if (SameText(t->prefix, t->prefixLength, prefix, prefixLength) &&
            SameText(t->text, t->textLength, text, textLength))
        {
            uint64_t* pool = Grow(m->elements, &m->elementCapacity,
                                  m->elementCount + elementCount + 1, sizeof *m->elements, 1024);
            if (pool == nullptr)
            {
                return 0;
            }
            m->elements = pool;
            if (elementCount != 0)
            {
                memcpy(m->elements + m->elementCount, elements, elementCount * sizeof *elements);
            }
            t->elementStart = (uint32_t)m->elementCount;
            t->elementCount = elementCount;
            m->elementCount += elementCount;
            return 1;
        }
    }
    CollationCandidate added = {prefix, prefixLength, text, textLength, elements, elementCount};
    return Rehash(m) && AppendMapping(m, c, &added);
}

// The code points not yet read, with their classes, and those given.
typedef struct Reading
{
    uint32_t* rest;
    uint8_t* classes;
    uint32_t restCount;
    uint32_t* given;
    uint32_t givenCount;
} Reading;

static void Take(Reading* r, uint32_t index)
{
    for (uint32_t i = index + 1; i < r->restCount; i++)
    {
        r->rest[i - 1] = r->rest[i];
        r->classes[i - 1] = r->classes[i];
    }
    r->restCount--;
}

// Whether the code points given end with the prefix.
static bool EndsWith(const Reading* r, const CollationCandidate* c)
{
    if (c->prefixLength == 0 || c->prefixLength > r->givenCount)
    {
        return c->prefixLength == 0;
    }
    const uint32_t* end = r->given + r->givenCount - c->prefixLength;
    return memcmp(end, c->prefix, c->prefixLength * sizeof *end) == 0;
}

// Whether a contraction of the prefix length starts with the code points
// grown after the first; *exactOut the one that is them, if any.
static bool Starts(const CollationCandidate* list, size_t count, const bool* in,
                   uint32_t prefixLength, const uint32_t* grown, uint32_t length,
                   const CollationCandidate** exactOut)
{
    bool starts = false;
    for (size_t e = 0; e < count; e++)
    {
        const CollationCandidate* c = &list[e];
        if (in[e] && c->prefixLength == prefixLength && c->textLength > length &&
            memcmp(c->text + 1, grown, length * sizeof *grown) == 0)
        {
            starts = true;
            *exactOut = c->textLength - 1 == length ? c : *exactOut;
        }
    }
    return starts;
}

// The longest contraction among the candidates of one prefix length that
// the code points after spell (the runtime's MatchContraction), its code
// points taken and given; null when none matches.
static const CollationCandidate* Contraction(const CollationCandidate* list, size_t count,
                                             const bool* in, uint32_t prefixLength, Reading* r)
{
    uint32_t grown[collationMaxContext];
    uint32_t from[collationMaxContext];
    uint32_t length = 0;
    uint32_t bestFrom[collationMaxContext];
    uint32_t bestLength = 0;
    const CollationCandidate* best = nullptr;
    uint8_t skipped = 0;
    for (uint32_t i = 0; length < collationMaxContext && i < r->restCount; i++)
    {
        uint8_t k = r->classes[i];
        if (k == 0 && skipped != 0)
        {
            break;
        }
        if (k != 0 && k <= skipped)
        {
            continue;
        }
        grown[length] = r->rest[i];
        from[length++] = i;
        const CollationCandidate* exact = nullptr;
        if (!Starts(list, count, in, prefixLength, grown, length, &exact))
        {
            length--;
            if (k == 0)
            {
                break;
            }
            skipped = k;
            continue;
        }
        if (exact != nullptr)
        {
            best = exact;
            bestLength = length;
            memcpy(bestFrom, from, length * sizeof *from);
        }
    }
    // Given in their order, then taken from the back.
    for (uint32_t j = 0; j < bestLength; j++)
    {
        r->given[r->givenCount++] = r->rest[bestFrom[j]];
    }
    for (uint32_t j = bestLength; j-- > 0;)
    {
        Take(r, bestFrom[j]);
    }
    return best;
}

// The mapping one code point is read through, its prefix and contraction
// matched; null when out of memory.
static const CollationCandidate* Read(CollationMappings* m, Reading* r, uint32_t codePoint)
{
    size_t count = Gather(m, codePoint);
    if (count == 0)
    {
        return nullptr;
    }
    const CollationCandidate* list = m->candidates;
    // The candidates whose prefix the code points before end with: one
    // prefix for each length.
    bool* in = malloc(count * sizeof *in);
    if (in == nullptr)
    {
        return nullptr;
    }
    for (size_t e = 0; e < count; e++)
    {
        in[e] = EndsWith(r, &list[e]);
    }
    r->given[r->givenCount++] = codePoint;
    const CollationCandidate* found = nullptr;
    for (int32_t length = collationMaxContext; found == nullptr && length >= 0; length--)
    {
        const CollationCandidate* alone = nullptr;
        bool any = false;
        bool contractions = false;
        for (size_t e = 0; e < count; e++)
        {
            if (in[e] && list[e].prefixLength == (uint32_t)length)
            {
                any = true;
                contractions = contractions || list[e].textLength > 1;
                alone = list[e].textLength == 1 ? &list[e] : alone;
            }
        }
        if (any)
        {
            found = contractions ? Contraction(list, count, in, (uint32_t)length, r) : nullptr;
            found = found != nullptr ? found : alone;
        }
    }
    free(in);
    return found;
}

uint32_t CollationMappingsElements(CollationMappings* m, const uint32_t* text, uint32_t length,
                                   uint32_t start, uint64_t* out, uint32_t outLength)
{
    Reading r = {
        .rest = malloc((length + 1) * sizeof *r.rest),
        .classes = malloc(length + 1),
        .given = malloc((length + 1) * sizeof *r.given),
    };
    if (r.rest == nullptr || r.classes == nullptr || r.given == nullptr)
    {
        free(r.rest);
        free(r.classes);
        free(r.given);
        return 0;
    }
    if (start != 0)
    {
        memcpy(r.given, text, start * sizeof *text);
    }
    r.givenCount = start;
    for (uint32_t i = start; i < length; i++)
    {
        r.rest[r.restCount] = text[i];
        r.classes[r.restCount++] = muniGetCombiningClass(text[i]);
    }
    while (r.restCount != 0)
    {
        uint32_t c = r.rest[0];
        Take(&r, 0);
        const CollationCandidate* found = Read(m, &r, c);
        for (uint32_t i = 0; found != nullptr && i < found->elementCount; i++)
        {
            if (found->elements[i] != 0)
            {
                if (outLength < collationMaxExpansion)
                {
                    out[outLength] = found->elements[i];
                }
                outLength++;
            }
        }
    }
    free(r.rest);
    free(r.classes);
    free(r.given);
    return outLength;
}
