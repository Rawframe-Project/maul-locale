// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units' grammatical genders (mlocGetUnitGender), as ICU4C 79's
// LongNameHandler gives a formatted unit's: a unit CLDR names has its
// long name's gender, else in a gendered language what the gender
// derivations make of its parts (calculateGenderForUnit); a name built
// from its parts takes theirs through processPatternTimes and
// forArbitraryUnit.

#include "data_view.h"
#include "kinds.h"
#include "unit_data.h"
#include "unit_id.h"
#include "unit_names.h"
#include "unit_normal.h"
#include "unit_tables.h"

#include "maul-locale/unit.h"

#include <string.h>

static_assert((int)mloc_genderAnimate == (int)unitGenderAnimate &&
                  (int)mloc_genderPersonal + 1 == (int)unitGenderCount,
              "the public genders are the data's");

typedef struct GenderContext
{
    const mlocDataSet* data;
    const mlocLocale* locale;
    UnitTable table;
} GenderContext;

// A gender's index in its long name's text, or 0.
static uint8_t GenderOf(const uint8_t* text, size_t length)
{
    static const char* const s_names[unitGenderCount] = {
        "", "animate", "common", "feminine", "inanimate", "masculine", "neuter", "personal"};
    for (uint32_t g = unitGenderAnimate; g < unitGenderCount; g++)
    {
        if (strlen(s_names[g]) == length && memcmp(s_names[g], text, length) == 0)
        {
            return (uint8_t)g;
        }
    }
    return 0;
}

// A unit CLDR names: its long name's gender along the chain, or 0.
static uint8_t NamedGender(const GenderContext* c, const char* id, size_t length)
{
    UnitNameFields f;
    if (!mlocGatherUnitNames(c->data, c->locale, kindUnitNameExtras, id, length, &f) ||
        (f.found & 1u << unitExtraGender) == 0)
    {
        return 0;
    }
    return GenderOf(f.text[unitExtraGender], f.length[unitExtraGender]);
}

// A simple unit's gender.
static uint8_t SimpleGender(const GenderContext* c, uint16_t simple)
{
    const char* text = nullptr;
    size_t length = 0;
    mlocSimpleUnitText(&c->table, simple, &text, &length);
    return NamedGender(c, text, length);
}

// What a structure's gender derivation gives (unitGenderFirst and the
// others), unitDerivedNone where the data has none.
static uint8_t GenderRule(const GenderContext* c, uint32_t structure)
{
    uint8_t value = unitDerivedNone;
    uint8_t unused = 0;
    return mlocUnitDerivation(c->data, c->locale, unitFeatureGender, structure, &value, &unused)
               ? value
               : unitDerivedNone;
}

// Whether a rule fixes a gender, or gives none (ICU's rule of a length
// other than 1).
static bool Fixed(uint8_t rule)
{
    return rule != unitGenderFirst && rule != unitGenderSecond;
}

// A fixed rule's gender: its own, or 0 for none.
static uint8_t FixedGender(uint8_t rule)
{
    return rule == unitDerivedNone ? 0 : rule;
}

// ICU's getDerivedGender: the first or second component's gender, or the
// rule's own.
static uint8_t Derived(uint8_t rule, uint8_t first, uint8_t second)
{
    return rule == unitGenderFirst ? first : rule == unitGenderSecond ? second : FixedGender(rule);
}

// Narrows a unit with a denominator to the side the per rule picks;
// false with the gender decided where the rule fixes one or the side is
// empty.
static bool PerSide(const GenderContext* c, const UnitId* id, int32_t* start, int32_t* end,
                    uint8_t* decided)
{
    uint8_t per = GenderRule(c, unitDerivePer);
    if (Fixed(per))
    {
        *decided = FixedGender(per);
        return false;
    }
    while (per == unitGenderSecond && id->parts[*start].power >= 0)
    {
        ++*start;
    }
    while (per == unitGenderFirst && *end >= 0 && id->parts[*end].power < 0)
    {
        --*end;
    }
    *decided = 0;
    return *end >= 0;
}

// The gender of a unit CLDR names without one, from the derivations top
// down (calculateGenderForUnit), its quirk kept: a prefix's rule is read
// only beside a power.
static uint8_t Calculated(const GenderContext* c, const UnitId* id)
{
    int32_t start = 0;
    int32_t end = (int32_t)id->count - 1;
    uint8_t decided = 0;
    if (id->count > 1 && id->parts[end].power < 0 && !PerSide(c, id, &start, &end, &decided))
    {
        return decided;
    }
    if (end > start)
    {
        uint8_t times = GenderRule(c, unitDeriveTimes);
        if (Fixed(times))
        {
            return FixedGender(times);
        }
        start = times == unitGenderFirst ? start : end;
    }
    const UnitPart* part = &id->parts[start];
    if (part->power != 1 && part->power != -1)
    {
        uint8_t power = GenderRule(c, unitDerivePower);
        uint8_t prefix = GenderRule(c, unitDerivePrefix);
        if (Fixed(power) || Fixed(prefix))
        {
            return FixedGender(Fixed(power) ? power : prefix);
        }
    }
    return SimpleGender(c, part->simple);
}

// A unit CLDR names: its gender, else in a gendered language (meter has
// one) the calculated one (maybeCalculateGender).
static uint8_t NamedOrCalculated(const GenderContext* c, const char* name, size_t length,
                                 const UnitId* id)
{
    uint8_t gender = NamedGender(c, name, length);
    return gender != 0 || id == nullptr || NamedGender(c, "meter", 5) == 0 ? gender
                                                                           : Calculated(c, id);
}

// A product's gender (processPatternTimes): a product CLDR names, else
// each single unit's through its prefix's and power's derivations, then
// the product's times rule; 0 for none.
static uint8_t ProductGender(const GenderContext* c, const UnitPart* parts, uint32_t count,
                             uint64_t constant)
{
    char name[unitMaxIdLength];
    size_t length = 0;
    uint8_t kind = 0;
    if ((count == 0 && constant <= 1) ||
        !mlocWriteNormalUnit(&c->table, parts, count, constant, name, sizeof(name), &length))
    {
        return 0;
    }
    if (mlocUnitNameKind(c->data, name, length, &kind) && kind == unitIdUnit)
    {
        UnitId id = {.count = (uint8_t)count};
        memcpy(id.parts, parts, sizeof(UnitPart) * count);
        return NamedOrCalculated(c, name, length, &id);
    }
    uint8_t times = GenderRule(c, unitDeriveTimes);
    uint8_t gender = 0;
    bool set = false;
    for (uint32_t i = 0; i < count; i++)
    {
        uint8_t single = SimpleGender(c, parts[i].simple);
        if (single == 0)
        {
            continue;
        }
        single =
            parts[i].prefix >= 0 ? Derived(GenderRule(c, unitDerivePrefix), single, 0) : single;
        single = parts[i].power != 1 ? Derived(GenderRule(c, unitDerivePower), single, 0) : single;
        bool taken = times == unitGenderFirst    ? i == 0
                     : times == unitGenderSecond ? i + 1 == count
                                                 : !set;
        if (taken)
        {
            gender = Fixed(times) ? FixedGender(times) : single;
            set = true;
        }
    }
    return gender;
}

// A unit CLDR does not name (forArbitraryUnit): its sides' genders
// through the per rule.
static uint8_t ArbitraryGender(const GenderContext* c, const UnitId* id)
{
    UnitPart numerator[unitMaxFactors];
    UnitPart denominator[unitMaxFactors];
    uint32_t n = 0;
    uint32_t d = 0;
    for (uint32_t i = 0; i < id->count; i++)
    {
        UnitPart part = id->parts[i];
        if (part.power > 0)
        {
            numerator[n++] = part;
            continue;
        }
        part.power = (int8_t)-part.power;
        denominator[d++] = part;
    }
    return Derived(GenderRule(c, unitDerivePer), ProductGender(c, numerator, n, 1),
                   ProductGender(c, denominator, d, id->denominatorConstant));
}

mlocResult mlocGetUnitGender(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                             size_t unitLength, mlocGender* genderOut)
{
    if (data == nullptr || locale == nullptr || unit == nullptr || unitLength == 0 ||
        genderOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    GenderContext c = {.data = data, .locale = locale};
    DataSpan span;
    UnitId id;
    if (!mlocDataSetHasKind(data, kindUnitIds) ||
        !mlocFindDataSetUnit(data, kindUnits, nullptr, 0, &span))
    {
        return mloc_errorNotFound;
    }
    c.table = mlocReadUnitTable(span);
    mlocResult r = mlocParseUnitId(&c.table, unit, unitLength, &id);
    uint8_t kind = 0;
    if (r == mloc_errorNotFound && mlocUnitNameKind(data, unit, unitLength, &kind) &&
        kind == unitIdUnit)
    {
        // A unit the names know and the table does not ("generic").
        *genderOut = (mlocGender)NamedOrCalculated(&c, unit, unitLength, nullptr);
        return mloc_success;
    }
    if (r == mloc_success && id.mixed != 0)
    {
        // ICU gives a mixed unit none (ICU-21494).
        *genderOut = mloc_genderNone;
        return mloc_success;
    }
    r = r == mloc_success ? mlocParseNormalUnit(data, unit, unitLength, &c.table, &id) : r;
    char name[unitMaxIdLength];
    size_t length = 0;
    if (r == mloc_success &&
        !mlocWriteNormalUnit(&c.table, id.parts, id.count, id.denominatorConstant, name,
                             sizeof(name), &length))
    {
        r = mloc_errorLimit;
    }
    if (r != mloc_success)
    {
        return r;
    }
    bool named = mlocUnitNameKind(data, name, length, &kind) && kind == unitIdUnit;
    *genderOut =
        (mlocGender)(named ? NamedOrCalculated(&c, name, length, &id) : ArbitraryGender(&c, &id));
    return mloc_success;
}
