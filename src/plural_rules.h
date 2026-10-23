// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The compiled plural rule sets, which src/generated/plural_rules.c
// defines, and the layout of the plural units the generator writes and the
// library reads:
//
// Plurals (kindPlurals), one unit per locale id CLDR's files name:
//   u8 cardinal, ordinal, ranges: a rule set's index (a cardinal one,
//       then an ordinal one) and a range table's, 0xFF for none (as is a
//       table index past the ranges unit's tables)
//   u8 zero[5]
//
// Plural ranges (kindPluralRanges), a unit of no locale:
//   u8 tableCount, u8 zero[7]
//   tables[tableCount], 36 bytes each: the category of a range from
//       start to end at start * 6 + end, 0xFF for none

#ifndef MLOC_PLURAL_RULES_H
#define MLOC_PLURAL_RULES_H

#include "maul-locale/decimal.h"

enum
{
    pluralUnitSize = 8,
    pluralRangesHeaderSize = 8,
    pluralRangeSize = 36,
    pluralNone = 0xFF,
};

// A rule set: its function, whether it is ordinal, and the categories it
// gives (1 << category).
typedef struct PluralRule
{
    uint8_t (*select)(const mlocPluralOperands* o);
    uint8_t ordinal;
    uint8_t categories;
} PluralRule;

extern const uint32_t mlocPluralRuleSetCount;
extern const PluralRule mlocPluralRuleSets[];

#endif
