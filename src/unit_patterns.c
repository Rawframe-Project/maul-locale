// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A unit formatter's patterns (unit_patterns.h).

#include "unit_patterns.h"

#include "currency_tables.h"
#include "invariant.h"
#include "kinds.h"
#include "unit_names.h"
#include "unit_normal.h"

#include <string.h>

static const uint32_t s_widthKinds[] = {
    [mloc_unitWidthShort] = kindUnitNamesShort,
    [mloc_unitWidthNarrow] = kindUnitNamesNarrow,
    [mloc_unitWidthLong] = kindUnitNamesLong,
    [mloc_unitWidthNumeric] = kindUnitNamesNarrow,
};

static_assert(sizeof(s_widthKinds) / sizeof(s_widthKinds[0]) == mloc_unitWidthNumeric + 1,
              "a names kind for each mlocUnitWidth");

uint32_t mlocUnitWidthKind(mlocUnitWidth width)
{
    // The formatter's options and the display name's width were checked.
    MLOC_ASSERT((uint32_t)width <= mloc_unitWidthNumeric);
    return s_widthKinds[width];
}

// Copies a pattern into a pool, "{0}" as currencyTextNumber, sharing an
// equal one of a category before it; false when the pool is full.
static bool AddPattern(const PatternPool* pool, PatternSlot* slot, uint32_t category,
                       const uint8_t* text, size_t length)
{
    // A slot keeps a pattern's start and length in 16 bits.
    MLOC_ASSERT(pool->capacity <= UINT16_MAX);
    uint8_t copy[unitPool];
    size_t n = 0;
    for (size_t at = 0; at < length; at++)
    {
        if (n == sizeof(copy))
        {
            return false;
        }
        bool number = at + 3 <= length && memcmp(text + at, "{0}", 3) == 0;
        copy[n++] = number ? currencyTextNumber : text[at];
        at += number ? 2 : 0;
    }
    for (uint32_t c = 0; c < category; c++)
    {
        if (slot->length[c] == n && memcmp(pool->bytes + slot->start[c], copy, n) == 0)
        {
            slot->start[category] = slot->start[c];
            slot->length[category] = slot->length[c];
            return true;
        }
    }
    if (n > pool->capacity - *pool->used)
    {
        return false;
    }
    memcpy(pool->bytes + *pool->used, copy, n);
    slot->start[category] = (uint16_t)*pool->used;
    slot->length[category] = (uint16_t)n;
    *pool->used += n;
    return true;
}

mlocResult mlocPoolUnitPatterns(const PatternPool* pool, PatternSlot* slot, const UnitForms* forms)
{
    for (uint32_t c = 0; c < unitCategories; c++)
    {
        const UnitPattern* p = forms->form[c].present ? &forms->form[c] : &forms->form[5];
        if (!p->present)
        {
            return mloc_errorNotFound;
        }
        if (!AddPattern(pool, slot, c, (const uint8_t*)p->text, p->length))
        {
            return mloc_errorLimit;
        }
    }
    return mloc_success;
}

mlocResult mlocAddUnitPatterns(UnitFormat* u, uint32_t slot, size_t* used, const UnitForms* forms)
{
    // Each output takes its slots below unitSlots, refused past it.
    MLOC_ASSERT(slot < unitSlots);
    PatternPool pool = {u->pool, unitPool, used};
    PatternSlot s;
    memcpy(s.start, u->start[slot], sizeof(s.start));
    memcpy(s.length, u->length[slot], sizeof(s.length));
    mlocResult r = mlocPoolUnitPatterns(&pool, &s, forms);
    memcpy(u->start[slot], s.start, sizeof(s.start));
    memcpy(u->length[slot], s.length, sizeof(s.length));
    return r;
}

mlocResult mlocNormalUnitName(const mlocDataSet* data, const char* unit, size_t length, char* out,
                              size_t capacity, size_t* lengthOut, bool* namedOut)
{
    // A unit the names data names is its own normal identifier (or one
    // normalizing does not find): test_unit_names checks each.
    uint8_t kind = 0;
    if (length <= capacity && mlocUnitNameKind(data, unit, length, &kind) && kind == unitIdUnit)
    {
        memcpy(out, unit, length);
        *lengthOut = length;
        *namedOut = true;
        return mloc_success;
    }
    *namedOut = false;
    return mlocNormalizeUnitName(data, unit, length, out, capacity, lengthOut);
}

mlocResult mlocLoadUnitForms(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                             size_t length, uint32_t kind, uint32_t gcase, UnitForms* out)
{
    char id[unitMaxIdLength];
    size_t idLength = 0;
    bool named = false;
    mlocResult r = mlocNormalUnitName(data, unit, length, id, sizeof(id), &idLength, &named);
    return r == mloc_success
               ? mlocBuildUnitForms(data, locale, id, idLength, named, kind, gcase, out)
               : r;
}

void mlocUnitOuter(const UnitFormat* u, uint32_t slot, Outer* out)
{
    mlocSlotOuter(u->patterns, u->start[slot], u->length[slot], &u->rules, out);
}

void mlocSlotOuter(const uint8_t* patterns, const uint16_t start[unitCategories],
                   const uint16_t length[unitCategories], const mlocPluralRules* rules, Outer* out)
{
    // Field by field: clearing the whole value first takes a string
    // store, which costs more to start than these writes.
    out->patternText = patterns;
    memcpy(out->patternStart, start, sizeof(out->patternStart));
    memcpy(out->patternLength, length, sizeof(out->patternLength));
    for (uint32_t c = 0; c < 6; c++)
    {
        out->text[c] = nullptr;
        out->textLength[c] = 0;
    }
    out->textKind = mloc_numberPartUnit;
    out->patternKind = mloc_numberPartUnit;
    memset(out->code, 0, sizeof(out->code));
    out->rules = *rules;
}
