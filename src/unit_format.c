// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit formatting: a number formatter's core within the unit's pattern for
// the number's plural category. The patterns are copied into the
// formatter, "{0}" marked as the number, so that one pool holds what keys
// along the chain gave; the pattern around the number is made from the
// copy each time the formatter is read. Percent and per mille, short or
// narrow and not compact, take the locale's percent pattern instead, as
// ICU's number_formatimpl.cpp does.

#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "number_core.h"
#include "plural.h"
#include "plural_rules.h"
#include "unit_coordinate.h"
#include "unit_mixed_format.h"
#include "unit_names.h"
#include "unit_patterns.h"
#include "unit_usage.h"

#include <stddef.h>
#include <string.h>

static_assert(sizeof(UnitFormat) <= sizeof(mlocUnitFormatter),
              "a unit formatter fits its opaque value");

static bool ValidOptions(const mlocUnitOptions* o)
{
    return (uint32_t)o->width <= mloc_unitWidthNumeric &&
           (uint32_t)o->coordinate <= mloc_coordinateLongitude &&
           (uint32_t)o->grammaticalCase < unitCaseCount && mlocValidFormatOptions(&o->number) &&
           o->number.style == mloc_numberDecimal && (o->usage != nullptr || o->usageLength == 0);
}

static_assert((int)mloc_caseNominative == (int)unitCaseNominative &&
                  (int)mloc_caseVocative + 1 == (int)unitCaseCount,
              "the public cases are the data's");

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
    bool named = false;
    mlocResult r = mlocNormalUnitName(data, unit, unitLength, id, sizeof(id), &length, &named);
    if (r != mloc_success)
    {
        return r;
    }
    if ((!named && (!mlocUnitNameKind(data, id, length, &idKind) || idKind != unitIdUnit)) ||
        !mlocGatherUnitNames(data, locale, kind, id, length, names))
    {
        return mloc_errorNotFound;
    }
    return mloc_success;
}

// Takes the locale's duration patterns when an output writes in them.
static mlocResult LoadDurations(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale)
{
    bool numeric = false;
    for (uint32_t i = 0; i < u->outputs; i++)
    {
        numeric |= u->output[i].numeric != 0;
    }
    UnitPatternTexts p;
    if (!numeric)
    {
        return mloc_success;
    }
    if (!mlocFindUnitPatterns(data, locale, &p))
    {
        return mloc_errorNotFound;
    }
    for (uint32_t d = 0; d < 3; d++)
    {
        u->duration[d] = p.text[unitPatternHm + d];
        u->durationLength[d] = p.length[unitPatternHm + d];
    }
    return mloc_success;
}

mlocResult mlocInitUnitFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                 const char* unit, size_t unitLength,
                                 const mlocUnitOptions* options, mlocUnitFormatter* formatterOut)
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
    // All but the pool begins as zero; the pool's bytes the patterns
    // take are copied out, its rest written as zero there.
    UnitFormat u;
    size_t pool = offsetof(UnitFormat, pool);
    size_t rest = offsetof(UnitFormat, patterns);
    memset(&u, 0, pool);
    memset((uint8_t*)&u + rest, 0, sizeof(u) - rest);
    u.grammaticalCase = (uint8_t)o->grammaticalCase;
    if (u.grammaticalCase != mloc_caseNominative && !mlocDataSetHasKind(data, kindUnitCases))
    {
        return mloc_errorNotFound;
    }
    u.outputs = 1;
    u.output[0].parts = 1;
    bool named = false;
    mlocResult found =
        o->usageLength > 0 ? mlocInitUsage(&u, data, locale, unit, unitLength, o, &used)
        : mlocIsMixedUnit(unit, unitLength)
            ? mlocInitMixedUnit(&u, data, locale, unit, unitLength, o, &used)
            : mlocNormalUnitName(data, unit, unitLength, id, sizeof(id), &idLength, &named);
    bool single = !u.usage && u.output[0].parts == 1;
    if (found == mloc_success && single)
    {
        UnitForms forms;
        found = mlocBuildUnitForms(data, locale, id, idLength, named, mlocUnitWidthKind(o->width),
                                   u.grammaticalCase, &forms);
        found = found == mloc_success ? mlocAddUnitPatterns(&u, 0, &used, &forms) : found;
    }
    found = found == mloc_success ? LoadDurations(&u, data, locale) : found;
    found =
        found == mloc_success ? mlocInitCoordinate(&u, data, locale, unit, unitLength, o) : found;
    if (found < 0)
    {
        return found;
    }
    Formatter* f = &u.formatter;
    bool compact = o->number.notation == mloc_notationCompactShort ||
                   o->number.notation == mloc_notationCompactLong;
    mlocNumberStyle style = single ? UnitStyle(id, idLength, o) : mloc_numberDecimal;
    mlocResult result = compact
                            ? mlocInitCompactCore(&f->core, data, locale,
                                                  o->number.notation == mloc_notationCompactLong,
                                                  &f->compactTable, &f->compactPatterns)
                            : mlocInitNumberCore(&f->core, data, locale, style);
    if (result < 0)
    {
        return result;
    }
    if (mlocInitPluralRules(data, locale, mloc_pluralCardinal, &u.rules) < 0)
    {
        return mloc_errorNotFound;
    }
    f->pluralRule = mlocPluralRuleOf(&u.rules);
    mlocSetFormatOptions(f, &o->number, unitStamp);
    f->style = (uint8_t)style;
    f->unscaled = 1;
    u.percent = style != mloc_numberDecimal;
    uint8_t* out = (uint8_t*)formatterOut;
    memcpy(out, &u, pool + used);
    memset(out + pool + used, 0, rest - pool - used);
    memcpy(out + rest, (const uint8_t*)&u + rest, sizeof(u) - rest);
    memset(out + sizeof(u), 0, sizeof(*formatterOut) - sizeof(u));
    return result;
}

// Whether an output's slots and patterns lie within the formatter.
static bool ValidOutput(const UnitFormat* u, const UnitOutput* o)
{
    if (o->parts < 1 || o->parts > unitMaxParts || o->slot > unitSlots - o->parts ||
        o->smallest >= o->parts || (o->parts > 1 && (!o->id.mixed || o->id.count != o->parts)) ||
        o->numeric > unitPatternHms + 1 ||
        (o->numeric != 0 && (o->parts < 2 || u->duration[o->numeric - 1] == nullptr)))
    {
        return false;
    }
    for (uint32_t f = 0; f < 3; f++)
    {
        if (o->numeric != 0 && o->field[f] >= (int8_t)o->parts)
        {
            return false;
        }
    }
    for (uint32_t p = o->slot; p < o->slot + o->parts; p++)
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

static_assert(offsetof(UnitFormat, formatter) == 0, "the formatter begins a unit formatter");

// The unit formatter of an opaque value, or false when the library did
// not make it. Only what formatting reads is copied: the pattern pool,
// most of the value, is read in place, the output units are those the
// formatter has, and the input unit, unit table and unit list only a
// usage or a mixed unit reads.
static bool ReadUnit(const mlocUnitFormatter* opaque, UnitFormat* u)
{
    if (opaque == nullptr || !mlocReadFormatter(opaque, sizeof(*opaque), unitStamp, &u->formatter))
    {
        return false;
    }
    const uint8_t* bytes = (const uint8_t*)opaque;
    uint8_t* to = (uint8_t*)u;
    size_t head = sizeof(Formatter);
    size_t pool = offsetof(UnitFormat, pool);
    size_t rest = offsetof(UnitFormat, patterns);
    size_t input = offsetof(UnitFormat, input);
    size_t outputs = offsetof(UnitFormat, output);
    size_t tail = offsetof(UnitFormat, duration);
    memcpy(to + rest, bytes + rest, input - rest);
    memcpy(to + tail, bytes + tail, sizeof(*u) - tail);
    if (u->outputs < 1 || u->outputs > unitOutputs)
    {
        return false;
    }
    // A single unit, the usual formatter, copies its one output and its
    // one slot in moves of a fixed size; a variable size compiles to a
    // string move, which costs more to start than these bytes.
    if (u->outputs == 1)
    {
        memcpy(to + outputs, bytes + outputs, sizeof(UnitOutput));
    }
    else
    {
        memcpy(to + outputs, bytes + outputs, u->outputs * sizeof(UnitOutput));
    }
    if (u->outputs == 1 && u->output[0].slot == 0 && u->output[0].parts == 1)
    {
        size_t start = offsetof(UnitFormat, start);
        size_t length = offsetof(UnitFormat, length);
        memcpy(to + start, bytes + start, sizeof(u->start[0]));
        memcpy(to + length, bytes + length, sizeof(u->length[0]));
    }
    else
    {
        memcpy(to + head, bytes + head, pool - head);
    }
    u->patterns = bytes + pool;
    bool mixed = false;
    bool valid = u->formatter.pluralRule < mlocPluralRuleSetCount && u->formatter.unscaled == 1 &&
                 (u->usage || u->outputs == 1);
    for (uint32_t i = 0; valid && i < u->outputs; i++)
    {
        valid = ValidOutput(u, &u->output[i]);
        mixed |= u->output[i].parts > 1;
    }
    if (u->usage || mixed)
    {
        memcpy(to + input, bytes + input, outputs - input);
    }
    return valid && mlocValidCoordinate(u) && (!mixed || (!u->percent && mlocValidMixedUnit(u))) &&
           (!u->usage || (!u->percent && mlocValidUsageOutputs(u)));
}

static bool ValidArguments(const mlocDecimal* decimal, const mlocOutput* output)
{
    return decimal != nullptr && output != nullptr &&
           (output->buffer != nullptr || output->capacity == 0) && mlocValidDecimal(decimal);
}

mlocResult mlocUnitFormatter_Format(const mlocUnitFormatter* formatter, const mlocDecimal* decimal,
                                    mlocOutput* textOut)
{
    UnitFormat u;
    if (!ReadUnit(formatter, &u))
    {
        return mloc_errorInvalid;
    }
    // A single unit goes to mlocFormatWith, which checks the arguments.
    if (u.coordinate == mloc_coordinateNone && !u.usage && u.output[0].parts == 1)
    {
        Outer outer;
        mlocUnitOuter(&u, 0, &outer);
        return mlocFormatWith(&u.formatter, u.percent ? nullptr : &outer, decimal, textOut);
    }
    if (!ValidArguments(decimal, textOut))
    {
        return mloc_errorInvalid;
    }
    if (u.coordinate != mloc_coordinateNone)
    {
        return mlocFormatCoordinate(&u, decimal, textOut);
    }
    return u.usage ? mlocFormatUsage(&u, decimal, textOut)
                   : mlocFormatMixedUnit(&u, decimal, textOut);
}

mlocResult mlocUnitFormatter_FormatRange(const mlocUnitFormatter* formatter,
                                         const mlocDecimal* start, const mlocDecimal* end,
                                         mlocOutput* textOut)
{
    UnitFormat u;
    if (!ReadUnit(formatter, &u) || (u.output[0].parts > 1 && !u.usage) ||
        u.coordinate != mloc_coordinateNone)
    {
        return mloc_errorInvalid;
    }
    if (u.usage)
    {
        if (!ValidArguments(start, textOut) || end == nullptr || !mlocValidDecimal(end) ||
            start->kind == mloc_decimalNaN || end->kind == mloc_decimalNaN)
        {
            return mloc_errorInvalid;
        }
        return mlocFormatUsageRange(&u, start, end, textOut);
    }
    Outer outer;
    mlocUnitOuter(&u, 0, &outer);
    return mlocFormatRangeWith(&u.formatter, u.percent ? nullptr : &outer, start, end, textOut);
}

mlocResult mlocGetUnitDisplayName(const mlocDataSet* data, const mlocLocale* locale,
                                  const char* unit, size_t unitLength, mlocUnitWidth width,
                                  mlocOutput* textOut)
{
    if (data == nullptr || locale == nullptr || unit == nullptr || unitLength == 0 ||
        textOut == nullptr || (textOut->buffer == nullptr && textOut->capacity != 0) ||
        (uint32_t)width > mloc_unitWidthNumeric)
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
    textOut->length = length;
    if (textOut->buffer == nullptr || length > textOut->capacity)
    {
        return mloc_errorCapacity;
    }
    memcpy(textOut->buffer, text, length);
    return named ? mloc_success : mloc_fellBack;
}
