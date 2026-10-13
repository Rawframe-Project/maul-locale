// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit formatting (record mloc-0024): a number formatter's core within
// the unit's pattern for the number's plural category. The patterns are
// copied into the formatter, "{0}" marked as the number, so that one
// pool holds what keys along the chain gave; the pattern around the
// number is made from the copy each time the formatter is read. Percent
// and per mille, short or narrow and not compact, take the locale's
// percent pattern instead, as ICU's number_formatimpl.cpp does.

#include "number_core.h"
#include "plural.h"
#include "plural_rules.h"
#include "unit_mixed_format.h"
#include "unit_names.h"
#include "unit_patterns.h"

#include <string.h>

static_assert(sizeof(UnitFormat) <= sizeof(mlocUnitFormatter),
              "a unit formatter fits its opaque value");

static bool ValidOptions(const mlocUnitOptions* o)
{
    return o->width <= mloc_unitWidthLong && mlocValidFormatOptions(&o->number) &&
           o->number.style == mloc_numberDecimal;
}

// The style ICU writes a unit in: percent and per mille, short or narrow
// and not compact, in the percent pattern; others decimal, in their own.
static mlocNumberStyle UnitStyle(const char* unit, size_t length, const mlocUnitOptions* o)
{
    if (o->width == mloc_unitWidthLong || o->number.notation == mloc_notationCompactShort ||
        o->number.notation == mloc_notationCompactLong)
    {
        return mloc_numberDecimal;
    }
    if (length == 7 && memcmp(unit, "percent", 7) == 0)
    {
        return mloc_numberPercent;
    }
    return length == 8 && memcmp(unit, "permille", 8) == 0 ? mloc_numberPerMille
                                                           : mloc_numberDecimal;
}

// Finds the names of a unit CLDR names, by its normal identifier.
static mlocResult FindNames(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                            size_t unitLength, uint32_t kind, UnitNameFields* names)
{
    char id[unitMaxIdLength];
    size_t length = 0;
    uint8_t idKind = 0;
    mlocResult r = mlocNormalUnitName(data, unit, unitLength, id, sizeof(id), &length);
    if (r != mloc_success)
    {
        return r;
    }
    if (!mlocUnitNameKind(data, id, length, &idKind) || idKind != unitIdUnit ||
        !mlocGatherUnitNames(data, locale, kind, id, length, names))
    {
        return mloc_errorNotFound;
    }
    return mloc_success;
}

mlocResult mlocInitUnitFormatter(mlocUnitFormatter* formatterOut, const mlocDataSet* data,
                                 const mlocLocale* locale, const char* unit, size_t unitLength,
                                 const mlocUnitOptions* options)
{
    static const mlocUnitOptions s_defaults = {0};
    const mlocUnitOptions* o = options == nullptr ? &s_defaults : options;
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || unit == nullptr ||
        unitLength == 0 || !ValidOptions(o))
    {
        return mloc_errorInvalid;
    }
    char id[unitMaxIdLength];
    size_t idLength = 0;
    size_t used = 0;
    static UnitFormat s_zero;
    UnitFormat u = s_zero;
    u.parts = 1;
    mlocResult found = mlocIsMixedUnit(unit, unitLength)
                           ? mlocInitMixedUnit(&u, data, locale, unit, unitLength, o, &used)
                           : mlocNormalUnitName(data, unit, unitLength, id, sizeof(id), &idLength);
    if (found == mloc_success && u.parts == 1)
    {
        UnitForms forms;
        found = mlocBuildUnitForms(data, locale, id, idLength, mlocUnitWidthKind(o->width), &forms);
        found = found == mloc_success ? mlocAddUnitPatterns(&u, 0, &used, &forms) : found;
    }
    if (found < 0)
    {
        return found;
    }
    Formatter* f = &u.formatter;
    bool compact = o->number.notation == mloc_notationCompactShort ||
                   o->number.notation == mloc_notationCompactLong;
    mlocNumberStyle style = u.parts == 1 ? UnitStyle(id, idLength, o) : mloc_numberDecimal;
    mlocResult result = compact
                            ? mlocInitCompactCore(&f->core, data, locale,
                                                  o->number.notation == mloc_notationCompactLong,
                                                  &f->compactTable, &f->compactPatterns)
                            : mlocInitNumberCore(&f->core, data, locale, style);
    if (result < 0)
    {
        return result;
    }
    if (mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f->pluralRule) < 0 ||
        mlocInitPluralRules(&u.rules, data, locale, mloc_pluralCardinal) < 0)
    {
        return mloc_errorNotFound;
    }
    mlocSetFormatOptions(f, &o->number, unitStamp);
    f->style = (uint8_t)style;
    f->unscaled = 1;
    u.percent = style != mloc_numberDecimal;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &u, sizeof(u));
    return result;
}

// The unit formatter of an opaque value, or false when the library did
// not make it.
static bool ReadUnit(const mlocUnitFormatter* opaque, UnitFormat* u)
{
    if (opaque == nullptr || !mlocReadFormatter(opaque, sizeof(*opaque), unitStamp, &u->formatter))
    {
        return false;
    }
    memcpy(u, opaque, sizeof(*u));
    if (u->formatter.pluralRule >= mlocPluralRuleSetCount || u->formatter.unscaled != 1 ||
        u->parts < 1 || u->parts > unitMaxParts ||
        (u->parts > 1 && (u->percent || !mlocValidMixedUnit(u))))
    {
        return false;
    }
    for (uint32_t p = 0; p < u->parts; p++)
    {
        for (uint32_t c = 0; c < unitCategories; c++)
        {
            if (u->start[p][c] > unitPool || u->length[p][c] > unitPool - u->start[p][c])
            {
                return false;
            }
        }
    }
    return true;
}

mlocResult mlocFormatUnit(const mlocUnitFormatter* formatter, const mlocDecimal* decimal,
                          mlocOutput* output)
{
    UnitFormat u;
    if (!ReadUnit(formatter, &u))
    {
        return mloc_errorInvalid;
    }
    if (u.parts > 1)
    {
        return mlocFormatMixedUnit(&u, decimal, output);
    }
    Outer outer;
    mlocUnitOuter(&u, 0, &outer);
    return mlocFormatWith(&u.formatter, u.percent ? nullptr : &outer, decimal, output);
}

mlocResult mlocFormatUnitRange(const mlocUnitFormatter* formatter, const mlocDecimal* start,
                               const mlocDecimal* end, mlocOutput* output)
{
    UnitFormat u;
    if (!ReadUnit(formatter, &u) || u.parts > 1)
    {
        return mloc_errorInvalid;
    }
    Outer outer;
    mlocUnitOuter(&u, 0, &outer);
    return mlocFormatRangeWith(&u.formatter, u.percent ? nullptr : &outer, start, end, output);
}

mlocResult mlocGetUnitDisplayName(const mlocDataSet* data, const mlocLocale* locale,
                                  const char* unit, size_t unitLength, mlocUnitWidth width,
                                  mlocOutput* output)
{
    if (data == nullptr || locale == nullptr || unit == nullptr || unitLength == 0 ||
        output == nullptr || (output->buffer == nullptr && output->capacity != 0) ||
        width > mloc_unitWidthLong)
    {
        return mloc_errorInvalid;
    }
    UnitNameFields names = {0};
    mlocResult found = FindNames(data, locale, unit, unitLength, mlocUnitWidthKind(width), &names);
    if (found < 0)
    {
        return found;
    }
    bool named = (names.found & 1u << unitNameDisplay) != 0 && names.length[unitNameDisplay] != 0;
    const void* text = named ? (const void*)names.text[unitNameDisplay] : unit;
    size_t length = named ? names.length[unitNameDisplay] : unitLength;
    output->length = length;
    if (output->buffer == nullptr || length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    memcpy(output->buffer, text, length);
    return named ? mloc_success : mloc_fellBack;
}
