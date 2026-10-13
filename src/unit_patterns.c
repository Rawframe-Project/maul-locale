// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A unit formatter's patterns (unit_patterns.h).

#include "unit_patterns.h"

#include "currency_tables.h"
#include "kinds.h"
#include "unit_names.h"

#include <string.h>

static const uint32_t s_widthKinds[] = {
    [mloc_unitWidthShort] = kindUnitNamesShort,
    [mloc_unitWidthNarrow] = kindUnitNamesNarrow,
    [mloc_unitWidthLong] = kindUnitNamesLong,
    [mloc_unitWidthNumeric] = kindUnitNamesNarrow,
};

uint32_t mlocUnitWidthKind(mlocUnitWidth width)
{
    return s_widthKinds[width];
}

// Copies a pattern into the pool, "{0}" as currencyTextNumber, sharing an
// equal one already there; false when the pool is full.
static bool AddPattern(UnitFormat* u, uint32_t slot, size_t* used, uint32_t category,
                       const uint8_t* text, size_t length)
{
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
    uint16_t* start = u->start[slot];
    uint16_t* lengths = u->length[slot];
    for (uint32_t c = 0; c < category; c++)
    {
        if (lengths[c] == n && memcmp(u->pool + start[c], copy, n) == 0)
        {
            start[category] = start[c];
            lengths[category] = lengths[c];
            return true;
        }
    }
    if (n > unitPool - *used)
    {
        return false;
    }
    memcpy(u->pool + *used, copy, n);
    start[category] = (uint16_t)*used;
    lengths[category] = (uint16_t)n;
    *used += n;
    return true;
}

mlocResult mlocAddUnitPatterns(UnitFormat* u, uint32_t slot, size_t* used, const UnitForms* forms)
{
    for (uint32_t c = 0; c < unitCategories; c++)
    {
        const UnitPattern* p = forms->form[c].present ? &forms->form[c] : &forms->form[5];
        if (!p->present)
        {
            return mloc_errorNotFound;
        }
        if (!AddPattern(u, slot, used, c, (const uint8_t*)p->text, p->length))
        {
            return mloc_errorCapacity;
        }
    }
    return mloc_success;
}

mlocResult mlocNormalUnitName(const mlocDataSet* data, const char* unit, size_t length, char* out,
                              size_t capacity, size_t* lengthOut)
{
    mlocResult r = mlocNormalizeUnitName(data, unit, length, out, capacity, lengthOut);
    uint8_t kind = 0;
    if (r == mloc_errorNotFound && length <= capacity &&
        mlocUnitNameKind(data, unit, length, &kind) && kind == unitIdUnit)
    {
        memcpy(out, unit, length);
        *lengthOut = length;
        return mloc_success;
    }
    return r;
}

mlocResult mlocLoadUnitForms(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                             size_t length, uint32_t kind, UnitForms* out)
{
    char id[unitMaxIdLength];
    size_t idLength = 0;
    mlocResult r = mlocNormalUnitName(data, unit, length, id, sizeof(id), &idLength);
    return r == mloc_success ? mlocBuildUnitForms(data, locale, id, idLength, kind, out) : r;
}

void mlocUnitOuter(const UnitFormat* u, uint32_t slot, Outer* out)
{
    *out = (Outer){
        .patternText = u->pool,
        .textKind = mloc_numberPartUnit,
        .patternKind = mloc_numberPartUnit,
        .rules = u->rules,
    };
    memcpy(out->patternStart, u->start[slot], sizeof(out->patternStart));
    memcpy(out->patternLength, u->length[slot], sizeof(out->patternLength));
}
