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

#include "currency_tables.h"
#include "kinds.h"
#include "number_core.h"
#include "number_format.h"
#include "plural.h"
#include "plural_rules.h"
#include "unit_names.h"

#include "maul-locale/unit.h"

#include <string.h>

enum
{
    // Marks a unit formatter the library made.
    unitStamp = 0x55,
    unitCategories = 6,
    // The bytes the patterns take, each kept once.
    unitPool = 1024,
};

// The opaque value's contents.
typedef struct UnitFormat
{
    Formatter formatter;
    uint16_t start[unitCategories];
    uint16_t length[unitCategories];
    uint8_t pool[unitPool];
    mlocPluralRules rules;
    // Written in the percent pattern, with no pattern around it.
    bool percent;
} UnitFormat;

static_assert(sizeof(UnitFormat) <= sizeof(mlocUnitFormatter),
              "a unit formatter fits its opaque value");

static const uint32_t s_widthKinds[] = {
    [mloc_unitWidthShort] = kindUnitNamesShort,
    [mloc_unitWidthNarrow] = kindUnitNamesNarrow,
    [mloc_unitWidthLong] = kindUnitNamesLong,
};

static bool ValidOptions(const mlocUnitOptions* o)
{
    return o->width <= mloc_unitWidthLong && mlocValidFormatOptions(&o->number) &&
           o->number.style == mloc_numberDecimal;
}

// Copies a pattern into the pool, "{0}" as currencyTextNumber, sharing an
// equal one already there; false when the pool is full.
static bool AddPattern(UnitFormat* u, size_t* used, uint32_t category, const uint8_t* text,
                       size_t length)
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
    for (uint32_t c = 0; c < category; c++)
    {
        if (u->length[c] == n && memcmp(u->pool + u->start[c], copy, n) == 0)
        {
            u->start[category] = u->start[c];
            u->length[category] = u->length[c];
            return true;
        }
    }
    if (n > unitPool - *used)
    {
        return false;
    }
    memcpy(u->pool + *used, copy, n);
    u->start[category] = (uint16_t)*used;
    u->length[category] = (uint16_t)n;
    *used += n;
    return true;
}

// Fills the patterns of each category from a unit's names.
static mlocResult AddPatterns(UnitFormat* u, const UnitNameFields* names)
{
    size_t used = 0;
    for (uint32_t c = 0; c < unitCategories; c++)
    {
        uint32_t field = mlocUnitPatternField(names, c);
        if (field == unitNameFields)
        {
            return mloc_errorNotFound;
        }
        if (!AddPattern(u, &used, c, names->text[field], names->length[field]))
        {
            return mloc_errorCapacity;
        }
    }
    return mloc_success;
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

// Checks the unit's arguments and finds its names.
static mlocResult FindNames(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                            size_t unitLength, uint32_t kind, UnitNameFields* names)
{
    uint8_t idKind = 0;
    if (!mlocUnitNameKind(data, unit, unitLength, &idKind) || idKind != unitIdUnit ||
        !mlocGatherUnitNames(data, locale, kind, unit, unitLength, names))
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
    UnitNameFields names;
    mlocResult found = FindNames(data, locale, unit, unitLength, s_widthKinds[o->width], &names);
    if (found < 0)
    {
        return found;
    }
    UnitFormat u;
    memset(&u, 0, sizeof(u));
    found = AddPatterns(&u, &names);
    if (found < 0)
    {
        return found;
    }
    Formatter* f = &u.formatter;
    bool compact = o->number.notation == mloc_notationCompactShort ||
                   o->number.notation == mloc_notationCompactLong;
    mlocNumberStyle style = UnitStyle(unit, unitLength, o);
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

// The unit formatter of an opaque value and the pattern around its
// number (null for none), or false when the library did not make it.
static bool ReadUnit(const mlocUnitFormatter* opaque, UnitFormat* u, Outer* outer,
                     const Outer** outerOut)
{
    if (!mlocReadFormatter(opaque, sizeof(*opaque), unitStamp, &u->formatter))
    {
        return false;
    }
    memcpy(u, opaque, sizeof(*u));
    *outerOut = u->percent ? nullptr : outer;
    if (u->formatter.pluralRule >= mlocPluralRuleSetCount || u->formatter.unscaled != 1)
    {
        return false;
    }
    *outer = (Outer){
        .patternText = u->pool,
        .textKind = mloc_numberPartUnit,
        .patternKind = mloc_numberPartUnit,
        .rules = u->rules,
    };
    for (uint32_t c = 0; c < unitCategories; c++)
    {
        if (u->start[c] > unitPool || u->length[c] > unitPool - u->start[c])
        {
            return false;
        }
        outer->patternStart[c] = u->start[c];
        outer->patternLength[c] = u->length[c];
    }
    return true;
}

mlocResult mlocFormatUnit(const mlocUnitFormatter* formatter, const mlocDecimal* decimal,
                          mlocOutput* output)
{
    UnitFormat u;
    Outer outer;
    const Outer* around = nullptr;
    if (formatter == nullptr || !ReadUnit(formatter, &u, &outer, &around))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatWith(&u.formatter, around, decimal, output);
}

mlocResult mlocFormatUnitRange(const mlocUnitFormatter* formatter, const mlocDecimal* start,
                               const mlocDecimal* end, mlocOutput* output)
{
    UnitFormat u;
    Outer outer;
    const Outer* around = nullptr;
    if (formatter == nullptr || !ReadUnit(formatter, &u, &outer, &around))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatRangeWith(&u.formatter, around, start, end, output);
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
    UnitNameFields names;
    mlocResult found = FindNames(data, locale, unit, unitLength, s_widthKinds[width], &names);
    if (found < 0)
    {
        return found;
    }
    bool named = (names.found & 1u << unitNameDisplay) != 0;
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
