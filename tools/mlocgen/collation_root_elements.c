// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The root elements (collation_root_elements.h), ported from ICU4C's
// CollationBaseDataBuilder (addRootElement, buildRootElementsTable,
// writeRootElementsRange) and CollationRootElements.

#include "collation_root_elements.h"

#include "collation_tables.h"
#include "table_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr uint32_t sentinel = 0xFFFFFF00;
constexpr uint32_t common = collationCommonSecondaryTertiary;
constexpr uint32_t secondaryTertiaryMask = 0xFFFF3F3F;
constexpr uint32_t tertiaryMask = 0x3F3F;
constexpr uint32_t caseMask = 0xC000;
constexpr uint32_t beforeWeight = 0x0100;
constexpr uint32_t deltaFlag = rootElementsDeltaFlag;
// The primaries ICU gives the root itself: the first unassigned one, the
// first trailing one, U+FFFD's and U+FFFF's.
constexpr uint32_t firstUnassignedPrimary = 0xFE040200;
constexpr uint32_t firstTrailingPrimary = 0xFF020200;
constexpr uint32_t replacementPrimary = 0xFFFD0000;
constexpr uint32_t maxPrimary = 0xFFFF0000;

typedef struct Builder
{
    const RootTable* table;
    // Han's primaries, the last one past the last character; 0 without
    // Han ranges.
    uint32_t firstHan;
    uint32_t lastHan;
    uint64_t* elements;
    size_t count;
    size_t capacity;
    uint32_t* units;
    int32_t length;
    int32_t unitCapacity;
    char* error;
    size_t errorSize;
    // Set with the message: a run's failure reads as no run.
    bool failed;
} Builder;

static int Fail(Builder* b, const char* what)
{
    snprintf(b->error, b->errorSize, "root elements: %s", what);
    b->failed = true;
    return 0;
}

static bool Compressible(const RootTable* t, uint32_t p)
{
    return t->compressible[p >> 24] || p >> 24 == collationUnassignedByte;
}

static uint64_t Element(uint32_t p)
{
    return (uint64_t)p << 32 | common;
}

// addRootElement: an element without its case bits, unless it is Han's
// with common weights (its run holds it).
static int Add(Builder* b, uint64_t ce)
{
    if (ce == 0)
    {
        return 1;
    }
    ce &= ~(uint64_t)caseMask;
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t secondaryTertiary = (uint32_t)ce;
    if (b->firstHan != 0 && b->firstHan <= p && p <= b->lastHan)
    {
        if (secondaryTertiary < common)
        {
            return Fail(b, "a Han primary under weights below the common ones");
        }
        if (secondaryTertiary == common)
        {
            return 1;
        }
    }
    uint32_t s = secondaryTertiary >> 16;
    uint32_t t = secondaryTertiary & tertiaryMask;
    if ((s != 0 && s <= beforeWeight) || (t != 0 && t <= beforeWeight))
    {
        return Fail(b, "a secondary or tertiary of 01 xx or less");
    }
    if ((secondaryTertiary & 0xC0) != 0)
    {
        return Fail(b, "a tertiary's second byte above 3F");
    }
    if ((p & 0xFF) != 0)
    {
        return Fail(b, "a primary of four bytes");
    }
    if (b->count == b->capacity)
    {
        size_t capacity = b->capacity == 0 ? 4096 : 2 * b->capacity;
        uint64_t* grown = realloc(b->elements, capacity * sizeof *grown);
        if (grown == nullptr)
        {
            return Fail(b, "out of memory");
        }
        b->elements = grown;
        b->capacity = capacity;
    }
    b->elements[b->count++] = ce;
    return 1;
}

static int AddAll(Builder* b, const uint64_t* elements, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!Add(b, elements[i]))
        {
            return 0;
        }
    }
    return 1;
}

// Every element ICU's genuca.cpp and its base builder add: the
// mappings' but those of texts starting with U+FFFD to U+FFFF (the
// runtime's fixed weights; the merge separator has none), the script
// starts' but the unassigned one's, the special lines', and the root's
// own primaries.
static int Gather(Builder* b)
{
    const RootTable* t = b->table;
    for (size_t i = 0; i < t->count; i++)
    {
        const RootMapping* m = &t->mappings[i];
        if (m->text[0] >= 0xFFFD && m->text[0] <= 0xFFFF)
        {
            continue;
        }
        if (!AddAll(b, m->elements, m->elementCount))
        {
            return 0;
        }
    }
    for (size_t i = 0; i < t->scriptStartCount; i++)
    {
        if (t->scriptStarts[i].codePoint != 0xFDD0 && !Add(b, t->scriptStarts[i].element))
        {
            return 0;
        }
    }
    for (size_t i = 0; i < t->specialCount; i++)
    {
        if (!Add(b, t->specials[i].element))
        {
            return 0;
        }
    }
    const uint64_t own[] = {Element(firstUnassignedPrimary), Element(firstTrailingPrimary),
                            Element(replacementPrimary), Element(maxPrimary)};
    return AddAll(b, own, sizeof own / sizeof own[0]);
}

static int Compare(const void* a, const void* b)
{
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return x < y ? -1 : x > y;
}

static int Put(Builder* b, uint32_t unit)
{
    if (b->length == b->unitCapacity)
    {
        int32_t capacity = b->unitCapacity == 0 ? 4096 : 2 * b->unitCapacity;
        uint32_t* grown = realloc(b->units, (size_t)capacity * sizeof *grown);
        if (grown == nullptr)
        {
            return Fail(b, "out of memory");
        }
        b->units = grown;
        b->unitCapacity = capacity;
    }
    b->units[b->length++] = unit;
    return 1;
}

static uint32_t AddToTwoBytePrimary(uint32_t primary, bool compressible, uint32_t offset)
{
    uint32_t low = compressible ? 4 : 2;
    uint32_t count = compressible ? 251 : 254;
    offset += ((primary >> 16) & 0xFF) - low;
    uint32_t second = offset % count + low;
    offset /= count;
    return ((primary & 0xFF000000) + (offset << 24)) | second << 16;
}

static uint32_t AddToPrimary(uint32_t primary, bool compressible, uint32_t offset, bool twoBytes)
{
    return twoBytes ? AddToTwoBytePrimary(primary, compressible, offset)
                    : mlocAddToPrimary(primary, compressible, offset);
}

// The number of primaries from p1 to p2, of two bytes or of three.
static int64_t Distance(uint32_t p1, uint32_t p2, bool compressible, bool twoBytes)
{
    int64_t low = compressible ? 4 : 2;
    int64_t count = compressible ? 251 : 254;
    int64_t second1 = ((p1 >> 16) & 0xFF) - low;
    int64_t second2 = ((p2 >> 16) & 0xFF) - low;
    if (twoBytes)
    {
        return (int64_t)(p2 >> 24) * count + second2 - ((int64_t)(p1 >> 24) * count + second1);
    }
    int64_t third1 = ((p1 >> 8) & 0xFF) - 2;
    int64_t third2 = ((p2 >> 8) & 0xFF) - 2;
    return ((int64_t)(p2 >> 24) * count + second2) * 254 + third2 -
           (((int64_t)(p1 >> 24) * count + second1) * 254 + third1);
}

// writeRootElementsRange: after prevPrimary and p, the primaries from
// element i on that go on by the same step with common weights (the
// last may have others) make a run with them; its last primary is
// written with the step. The index of its last element, or 0 when there
// is no run of three.
static size_t Run(Builder* b, uint32_t prevPrimary, uint32_t p, size_t i)
{
    const RootTable* t = b->table;
    if (i >= b->count || (p & prevPrimary & 0xFF0000) == 0)
    {
        return 0;
    }
    bool compressible = Compressible(t, p);
    if ((compressible || Compressible(t, prevPrimary)) &&
        (p & 0xFF000000) != (prevPrimary & 0xFF000000))
    {
        return 0;
    }
    bool twoBytes = (p & 0xFF00) == 0;
    if (twoBytes != ((prevPrimary & 0xFF00) == 0))
    {
        return 0;
    }
    int64_t step = Distance(prevPrimary, p, compressible, twoBytes);
    if (step > rootElementsStepMask)
    {
        return 0;
    }
    size_t end = 0;
    for (;;)
    {
        prevPrimary = p;
        uint32_t next = AddToPrimary(p, compressible, (uint32_t)step, twoBytes);
        uint64_t ce = b->elements[i];
        p = (uint32_t)(ce >> 32);
        uint32_t secondaryTertiary = (uint32_t)ce & secondaryTertiaryMask;
        if (p != next || ((p & 0xFF000000) != (prevPrimary & 0xFF000000) &&
                          (compressible || Compressible(t, p))))
        {
            p = prevPrimary;
            break;
        }
        end = i++;
        if (secondaryTertiary != common || i >= b->count)
        {
            break;
        }
    }
    if (end != 0 && !Put(b, p | (uint32_t)step))
    {
        return 0;
    }
    return end;
}

typedef struct State
{
    uint32_t nextHan;
    uint32_t prevPrimary;
    bool needCommon;
    bool hasDelta;
} State;

// A primary at or after the next Han one: Han's run up to it, then it.
static int HanPrimary(Builder* b, State* s, uint32_t p)
{
    if (p == s->nextHan)
    {
        s->nextHan = p < b->lastHan ? mlocAddToPrimary(p, false, rootHanStep) : UINT32_MAX;
        return Put(b, p);
    }
    if (!Put(b, s->nextHan))
    {
        return 0;
    }
    if (s->nextHan == b->lastHan)
    {
        s->nextHan = UINT32_MAX;
        return Put(b, p);
    }
    if (p < b->lastHan)
    {
        s->nextHan = mlocAddToPrimary(p, false, rootHanStep);
        return Put(b, p | rootHanStep);
    }
    s->nextHan = UINT32_MAX;
    if (p == b->lastHan)
    {
        return Put(b, p | rootHanStep);
    }
    return Put(b, b->lastHan | rootHanStep) && Put(b, p);
}

// The element at index i, its primary and its secondary and tertiary.
typedef struct Current
{
    size_t index;
    uint32_t p;
    uint32_t secondaryTertiary;
} Current;

static Current At(const Builder* b, size_t index)
{
    uint64_t ce = b->elements[index];
    return (Current){index, (uint32_t)(ce >> 32), (uint32_t)ce & secondaryTertiaryMask};
}

// A new primary: Han's run up to it, or a run from the previous primary
// (the current element moved to its end), or the primary alone.
static int NewPrimary(Builder* b, State* s, Current* c)
{
    if (c->p >= s->nextHan)
    {
        if (!HanPrimary(b, s, c->p))
        {
            return 0;
        }
    }
    else
    {
        size_t end = s->prevPrimary != 0 && !s->hasDelta && c->secondaryTertiary == common
                         ? Run(b, s->prevPrimary, c->p, c->index + 1)
                         : 0;
        if (end != 0)
        {
            *c = At(b, end);
        }
        else if (!Put(b, c->p))
        {
            return 0;
        }
    }
    s->prevPrimary = c->p;
    s->needCommon = false;
    s->hasDelta = false;
    return 1;
}

// The secondary and tertiary unit, unless the common weights are
// implied; after weights below the common ones under a primary, the
// common unit is written even so.
static int Delta(Builder* b, State* s, const Current* c)
{
    if (c->secondaryTertiary == common && !s->needCommon)
    {
        return 1;
    }
    if (c->secondaryTertiary < common)
    {
        s->needCommon = c->p != 0;
    }
    else if (c->secondaryTertiary == common)
    {
        s->needCommon = false;
    }
    s->hasDelta = true;
    return Put(b, c->secondaryTertiary | deltaFlag);
}

// buildRootElementsTable's loop over the sorted elements.
static int Units(Builder* b)
{
    State s = {.nextHan = b->firstHan != 0 ? b->firstHan : UINT32_MAX};
    for (size_t i = 0; i < b->count; i++)
    {
        Current c = At(b, i);
        // A common unit before a new primary or weights above the common
        // ones, after weights below them.
        if ((c.p != s.prevPrimary || c.secondaryTertiary > common) && s.needCommon &&
            !Put(b, common | deltaFlag))
        {
            return 0;
        }
        if ((c.p != s.prevPrimary && !NewPrimary(b, &s, &c)) || !Delta(b, &s, &c))
        {
            return 0;
        }
        i = c.index;
    }
    return 1;
}

static int Build(Builder* b)
{
    const RootTable* t = b->table;
    if (t->hanRangeCount != 0)
    {
        uint32_t han = 0;
        for (size_t i = 0; i < t->hanRangeCount; i++)
        {
            han += t->hanRanges[2 * i + 1] - t->hanRanges[2 * i] + 1;
        }
        b->firstHan = t->firstHanPrimary;
        b->lastHan = mlocAddToPrimary(t->firstHanPrimary, false, han * rootHanStep);
    }
    if (!Gather(b))
    {
        return 0;
    }
    GenSort(b->elements, b->count, sizeof *b->elements, Compare);
    size_t distinct = 0;
    for (size_t i = 0; i < b->count; i++)
    {
        if (distinct == 0 || b->elements[i] != b->elements[distinct - 1])
        {
            b->elements[distinct++] = b->elements[i];
        }
    }
    b->count = distinct;
    if (!Add(b, Element(sentinel)))
    {
        return 0;
    }
    for (int32_t i = 0; i < rootElementsIndexCount; i++)
    {
        if (!Put(b, 0))
        {
            return 0;
        }
    }
    if (!Units(b) || b->failed)
    {
        return 0;
    }
    // The sentinel ends both scans.
    int32_t index = rootElementsIndexCount;
    b->units[rootElementsFirstTertiaryIndex] = (uint32_t)index;
    while ((b->units[index] & 0xFFFF0000) == 0)
    {
        index++;
    }
    b->units[rootElementsFirstSecondaryIndex] = (uint32_t)index;
    while ((b->units[index] & deltaFlag) != 0)
    {
        index++;
    }
    b->units[rootElementsFirstPrimaryIndex] = (uint32_t)index;
    b->units[rootElementsCommonIndex] = common;
    b->units[rootElementsBoundsIndex] = (uint32_t)t->lastCommonSecondaryByte << 24 |
                                        (uint32_t)t->firstIgnorableSecondaryByte << 16 |
                                        t->firstIgnorableTertiaryByte;
    return 1;
}

int RootElementsBuild(const RootTable* table, RootElements* out, char* error, size_t errorSize)
{
    Builder b = {.table = table, .error = error, .errorSize = errorSize};
    int ok = Build(&b);
    free(b.elements);
    if (!ok)
    {
        free(b.units);
        return 0;
    }
    *out = (RootElements){b.units, b.length, b.firstHan, b.lastHan};
    return 1;
}

void RootElementsFree(RootElements* elements)
{
    free(elements->units);
    *elements = (RootElements){0};
}

static bool IsDelta(uint32_t unit)
{
    return (unit & deltaFlag) != 0;
}

// findP: the largest index of a primary unit at most p.
static int32_t FindP(const RootElements* e, uint32_t p)
{
    const uint32_t* u = e->units;
    int32_t start = (int32_t)u[rootElementsFirstPrimaryIndex];
    int32_t limit = e->length - 1;
    while (start + 1 < limit)
    {
        int32_t i = (start + limit) / 2;
        uint32_t q = u[i];
        if (IsDelta(q))
        {
            int32_t j = i + 1;
            while (j != limit && IsDelta(u[j]))
            {
                j++;
            }
            if (j != limit)
            {
                i = j;
                q = u[j];
            }
            else
            {
                j = i - 1;
                while (j != start && IsDelta(u[j]))
                {
                    j--;
                }
                if (j == start)
                {
                    break;
                }
                i = j;
                q = u[j];
            }
        }
        if (p < (q & 0xFFFFFF00))
        {
            limit = i;
        }
        else
        {
            start = i;
        }
    }
    return start;
}

int32_t RootElementsFindPrimary(const RootElements* e, uint32_t p)
{
    return FindP(e, p);
}

// getFirstSecTerForPrimary: the first secondary and tertiary of the
// primary before index.
static uint32_t FirstSecondaryTertiary(const RootElements* e, int32_t index)
{
    uint32_t unit = e->units[index];
    if (!IsDelta(unit))
    {
        return common;
    }
    unit &= ~deltaFlag;
    return unit > common ? common : unit;
}

static uint32_t SubtractStep(uint32_t p, bool compressible, uint32_t step)
{
    uint32_t low = compressible ? 4 : 2;
    uint32_t count = compressible ? 251 : 254;
    if ((p & 0xFFFF) == 0)
    {
        int32_t second = (int32_t)((p >> 16) & 0xFF) - (int32_t)step;
        if (second < (int32_t)low)
        {
            second += (int32_t)count;
            p -= 0x1000000;
        }
        return (p & 0xFF000000) | (uint32_t)second << 16;
    }
    int32_t third = (int32_t)((p >> 8) & 0xFF) - (int32_t)step;
    if (third >= 2)
    {
        return (p & 0xFFFF0000) | (uint32_t)third << 8;
    }
    third += 254;
    int32_t second = (int32_t)((p >> 16) & 0xFF) - 1;
    if (second < (int32_t)low)
    {
        second = (int32_t)(low + count - 1);
        p -= 0x1000000;
    }
    return (p & 0xFF000000) | (uint32_t)second << 16 | (uint32_t)third << 8;
}

uint32_t RootElementsPrimaryBefore(const RootElements* e, uint32_t p, bool compressible)
{
    int32_t index = FindP(e, p);
    uint32_t q = e->units[index];
    uint32_t step = 0;
    if (p == (q & 0xFFFFFF00))
    {
        step = q & rootElementsStepMask;
        if (step == 0)
        {
            do
            {
                p = e->units[--index];
            } while (IsDelta(p));
            return p & 0xFFFFFF00;
        }
    }
    else
    {
        step = e->units[index + 1] & rootElementsStepMask;
    }
    return SubtractStep(p, compressible, step);
}

uint32_t RootElementsPrimaryAfter(const RootElements* e, uint32_t p, int32_t index,
                                  bool compressible)
{
    uint32_t q = e->units[++index];
    uint32_t step = q & rootElementsStepMask;
    if (!IsDelta(q) && step != 0)
    {
        return AddToPrimary(p, compressible, step, (p & 0xFFFF) == 0);
    }
    while (IsDelta(q))
    {
        q = e->units[++index];
    }
    return q;
}

uint32_t RootElementsSecondaryBefore(const RootElements* e, uint32_t p, uint32_t s)
{
    int32_t index = 0;
    uint32_t previous = 0;
    uint32_t secondary = 0;
    if (p == 0)
    {
        index = (int32_t)e->units[rootElementsFirstSecondaryIndex];
        secondary = e->units[index] >> 16;
    }
    else
    {
        index = FindP(e, p) + 1;
        previous = beforeWeight;
        secondary = FirstSecondaryTertiary(e, index) >> 16;
    }
    while (s > secondary && IsDelta(e->units[index]))
    {
        previous = secondary;
        secondary = e->units[index++] >> 16;
    }
    return previous;
}

uint32_t RootElementsTertiaryBefore(const RootElements* e, uint32_t p, uint32_t s, uint32_t t)
{
    int32_t index = 0;
    uint32_t previous = beforeWeight;
    uint32_t secondaryTertiary = 0;
    if (p == 0)
    {
        if (s == 0)
        {
            index = (int32_t)e->units[rootElementsFirstTertiaryIndex];
            previous = 0;
        }
        else
        {
            index = (int32_t)e->units[rootElementsFirstSecondaryIndex];
        }
        secondaryTertiary = e->units[index] & ~deltaFlag;
    }
    else
    {
        index = FindP(e, p) + 1;
        secondaryTertiary = FirstSecondaryTertiary(e, index);
    }
    uint32_t wanted = s << 16 | t;
    while (wanted > secondaryTertiary && IsDelta(e->units[index]))
    {
        if (secondaryTertiary >> 16 == s)
        {
            previous = secondaryTertiary;
        }
        secondaryTertiary = e->units[index++] & ~deltaFlag;
    }
    return previous & 0xFFFF;
}

uint32_t RootElementsSecondaryAfter(const RootElements* e, int32_t index, uint32_t s)
{
    uint32_t secondaryTertiary = 0;
    uint32_t limit = 0;
    if (index == 0)
    {
        index = (int32_t)e->units[rootElementsFirstSecondaryIndex];
        secondaryTertiary = e->units[index];
        limit = 0x10000;
    }
    else
    {
        secondaryTertiary = FirstSecondaryTertiary(e, index + 1);
        limit = RootElementsSecondaryBoundary(e);
    }
    for (;;)
    {
        uint32_t secondary = secondaryTertiary >> 16;
        if (secondary > s)
        {
            return secondary;
        }
        secondaryTertiary = e->units[++index];
        if (!IsDelta(secondaryTertiary))
        {
            return limit;
        }
    }
}

uint32_t RootElementsTertiaryAfter(const RootElements* e, int32_t index, uint32_t s, uint32_t t)
{
    uint32_t secondaryTertiary = 0;
    uint32_t limit = RootElementsTertiaryBoundary(e);
    if (index == 0)
    {
        if (s == 0)
        {
            index = (int32_t)e->units[rootElementsFirstTertiaryIndex];
            limit = 0x4000;
        }
        else
        {
            index = (int32_t)e->units[rootElementsFirstSecondaryIndex];
        }
        secondaryTertiary = e->units[index] & ~deltaFlag;
    }
    else
    {
        secondaryTertiary = FirstSecondaryTertiary(e, index + 1);
    }
    uint32_t wanted = s << 16 | t;
    for (;;)
    {
        if (secondaryTertiary > wanted)
        {
            return secondaryTertiary & 0xFFFF;
        }
        secondaryTertiary = e->units[++index];
        if (!IsDelta(secondaryTertiary) || secondaryTertiary >> 16 > s)
        {
            return limit;
        }
        secondaryTertiary &= ~deltaFlag;
    }
}

uint64_t RootElementsLastBefore(const RootElements* e, uint32_t p)
{
    if (p == 0)
    {
        return 0;
    }
    int32_t index = FindP(e, p);
    uint32_t q = e->units[index];
    uint32_t secondaryTertiary = common;
    if (p == (q & 0xFFFFFF00))
    {
        uint32_t before = e->units[index - 1];
        if (!IsDelta(before))
        {
            p = before & 0xFFFFFF00;
        }
        else
        {
            secondaryTertiary = before;
            index -= 2;
            while (IsDelta(e->units[index]))
            {
                index--;
            }
            p = e->units[index] & 0xFFFFFF00;
        }
    }
    else
    {
        p = q & 0xFFFFFF00;
        while (IsDelta(q = e->units[++index]))
        {
            secondaryTertiary = q;
        }
    }
    return (uint64_t)p << 32 | (secondaryTertiary & ~deltaFlag);
}

uint64_t RootElementsFirstAtLeast(const RootElements* e, uint32_t p)
{
    if (p == 0)
    {
        return 0;
    }
    int32_t index = FindP(e, p);
    if (p != (e->units[index] & 0xFFFFFF00))
    {
        do
        {
            p = e->units[++index];
        } while (IsDelta(p));
    }
    return (uint64_t)p << 32 | common;
}
