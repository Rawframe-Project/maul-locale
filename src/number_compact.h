// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Compact notation (record mloc-0021): the power of ten a value is
// divided by and the pattern its plural category takes, from a locale's
// compact table (number_tables.h).

#ifndef MLOC_NUMBER_COMPACT_H
#define MLOC_NUMBER_COMPACT_H

#include "number_data.h"

#include "maul-locale/number.h"

// A locale's compact table, with the compact patterns it indexes.
typedef struct CompactData
{
    const uint8_t* table;
    const uint8_t* patterns;
} CompactData;

// What compacting chose: the power the value was divided by, and the
// compact pattern, or compactNoPattern for the decimal one (a value below
// 1000 or a power the locale does not compact).
typedef struct CompactChoice
{
    int32_t exponent;
    uint32_t pattern;
} CompactChoice;

enum
{
    compactNoPattern = 0xFFFFFFFF,
};

// Whether every pattern index a table holds is in the patterns unit, and
// every other pattern writes digits.
bool mlocValidCompactUse(CompactData data);

// Divides a finite decimal by the power its magnitude's row gives, rounds
// it by the precision (choosing the row again when rounding carries it
// to the next power), and picks the pattern of its plural category: the
// category of the digits shown, with c the exponent, by the rule set
// given (mlocFindPluralRule); the exact value 1's pattern for 1.
mlocResult mlocApplyCompact(CompactData data, const mlocPrecision* precision, uint8_t pluralRule,
                            mlocDecimal* decimal, CompactChoice* choiceOut);

#endif
