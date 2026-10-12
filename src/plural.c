// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Plural rules (record mloc-0020): a locale's unit names its compiled
// rule sets and range table; the rules value keeps the rule's index and
// a copy of the table, so selecting reads no data. The units' layout is
// in plural_rules.h.

#include "plural.h"

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "plural_rules.h"

#include "maul-locale/plural.h"

#include <string.h>

enum
{
    categoryCount = 6,
    // Marks a value the library made.
    rulesStamp = 0xA5,
};

// The opaque value's bytes.
typedef struct Rules
{
    uint8_t rule;
    uint8_t type;
    uint8_t hasRanges;
    uint8_t stamp;
    uint8_t ranges[pluralRangeSize];
} Rules;

static_assert(sizeof(Rules) == sizeof(mlocPluralRules), "the rules fill their opaque value");

static bool ValidRule(uint8_t rule, uint8_t ordinal)
{
    return rule == pluralNone ||
           (rule < mlocPluralRuleSetCount && mlocPluralRuleSets[rule].ordinal == ordinal);
}

static bool ValidTable(const uint8_t* table)
{
    for (int k = 0; k < pluralRangeSize; k++)
    {
        if (table[k] >= categoryCount && table[k] != pluralNone)
        {
            return false;
        }
    }
    return true;
}

bool mlocValidPluralRanges(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < pluralRangesHeaderSize ||
        payload.size != pluralRangesHeaderSize + (size_t)payload.bytes[0] * pluralRangeSize ||
        mlocLoad32(payload.bytes) >> 8 != 0 || mlocLoad32(payload.bytes + 4) != 0)
    {
        return false;
    }
    for (uint32_t t = 0; t < payload.bytes[0]; t++)
    {
        if (!ValidTable(payload.bytes + pluralRangesHeaderSize + (size_t)t * pluralRangeSize))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidPlurals(const DataView* view, DataSpan payload)
{
    if (payload.size != pluralUnitSize || payload.bytes[3] != 0 ||
        mlocLoad32(payload.bytes + 4) != 0 || !ValidRule(payload.bytes[0], 0) ||
        !ValidRule(payload.bytes[1], 1))
    {
        return false;
    }
    if (payload.bytes[2] == pluralNone)
    {
        return true;
    }
    // A table the unit names is in the same file.
    DataSpan ranges;
    return mlocFindDataUnit(view, kindPluralRanges, dataNoLocale, &ranges) &&
           ranges.size >= pluralRangesHeaderSize && payload.bytes[2] < ranges.bytes[0];
}

mlocResult mlocInitPluralRules(mlocPluralRules* rulesOut, const mlocDataSet* data,
                               const mlocLocale* locale, mlocPluralType type)
{
    if (rulesOut == nullptr || data == nullptr || locale == nullptr ||
        (type != mloc_pluralCardinal && type != mloc_pluralOrdinal))
    {
        return mloc_errorInvalid;
    }
    DataSpan unit;
    bool fellBack = false;
    if (!mlocFindLocaleUnit(data, mloc_componentPlurals, kindPlurals, locale, &unit, &fellBack))
    {
        return mloc_errorNotFound;
    }
    Rules rules = {.rule = unit.bytes[type], .type = (uint8_t)type, .stamp = rulesStamp};
    if (rules.rule == pluralNone)
    {
        return mloc_errorNotFound;
    }
    memset(rules.ranges, pluralNone, sizeof(rules.ranges));
    DataSpan ranges;
    if (unit.bytes[2] != pluralNone &&
        mlocFindDataSetUnit(data, kindPluralRanges, nullptr, 0, &ranges) &&
        unit.bytes[2] < ranges.bytes[0])
    {
        memcpy(rules.ranges,
               ranges.bytes + pluralRangesHeaderSize + (size_t)unit.bytes[2] * pluralRangeSize,
               pluralRangeSize);
        rules.hasRanges = 1;
    }
    memcpy(rulesOut, &rules, sizeof(rules));
    return fellBack ? mloc_fellBack : mloc_success;
}

// The rules of an opaque value, or false when the library did not make
// it.
static bool Read(const mlocPluralRules* opaque, Rules* rules)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(rules, opaque, sizeof(*rules));
    return rules->stamp == rulesStamp && rules->type <= mloc_pluralOrdinal &&
           rules->hasRanges <= 1 && rules->rule < mlocPluralRuleSetCount &&
           mlocPluralRuleSets[rules->rule].ordinal == rules->type && ValidTable(rules->ranges);
}

mlocResult mlocSelectPluralOperands(const mlocPluralRules* rules,
                                    const mlocPluralOperands* operands,
                                    mlocPluralCategory* categoryOut)
{
    Rules r;
    if (!Read(rules, &r) || operands == nullptr || categoryOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    *categoryOut = (mlocPluralCategory)mlocPluralRuleSets[r.rule].select(operands);
    return mloc_success;
}

mlocResult mlocSelectPlural(const mlocPluralRules* rules, const mlocDecimal* decimal,
                            mlocPluralCategory* categoryOut)
{
    Rules r;
    if (!Read(rules, &r) || decimal == nullptr || categoryOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    if (!mlocValidDecimal(decimal))
    {
        return mloc_errorInvalid;
    }
    if (decimal->kind != mloc_decimalFinite)
    {
        *categoryOut = mloc_pluralOther;
        return mloc_success;
    }
    mlocPluralOperands operands;
    (void)mlocGetPluralOperands(decimal, &operands);
    *categoryOut = (mlocPluralCategory)mlocPluralRuleSets[r.rule].select(&operands);
    return mloc_success;
}

mlocResult mlocSelectPluralRange(const mlocPluralRules* rules, mlocPluralCategory start,
                                 mlocPluralCategory end, mlocPluralCategory* categoryOut)
{
    Rules r;
    if (!Read(rules, &r) || r.type != mloc_pluralCardinal || categoryOut == nullptr ||
        (uint32_t)start >= categoryCount || (uint32_t)end >= categoryCount)
    {
        return mloc_errorInvalid;
    }
    uint8_t category = r.ranges[start * categoryCount + end];
    *categoryOut = category == pluralNone ? end : (mlocPluralCategory)category;
    return mloc_success;
}

mlocResult mlocGetPluralCategories(const mlocPluralRules* rules, uint32_t* categoriesOut)
{
    Rules r;
    if (!Read(rules, &r) || categoriesOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    *categoriesOut = mlocPluralRuleSets[r.rule].categories;
    return mloc_success;
}
