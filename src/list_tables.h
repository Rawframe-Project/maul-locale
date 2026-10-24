// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The list part's units, shared by the library and the generator. All
// integers are little-endian.
//
// List pattern sets, a unit of no locale in the table shape of
// number_tables.h: each record the string indexes (u16) of a set's
// patterns for two items, the start, the middle and the end. Each pattern
// holds "{0}" and "{1}" once each.
//
// A locale's lists, a unit per locale filed where the resolved data
// differs from its chain's: the set index (u16) of each style, in the
// order of ListStyle. The values are CLDR's resolution, root's aliases
// among the styles followed from the locale.
//
// A data layout: tools/check_data_format.py watches this header.

#ifndef MLOC_LIST_TABLES_H
#define MLOC_LIST_TABLES_H

enum
{
    listPatternTwo = 0,
    listPatternStart = 1,
    listPatternMiddle = 2,
    listPatternEnd = 3,
    listPatternCount = 4,
    listSetSize = 2 * listPatternCount,
};

// The styles: a type (and, or, unit) by width (wide, short, narrow).
typedef enum ListStyle
{
    listStandard = 0,
    listStandardShort = 1,
    listStandardNarrow = 2,
    listOr = 3,
    listOrShort = 4,
    listOrNarrow = 5,
    listUnit = 6,
    listUnitShort = 7,
    listUnitNarrow = 8,
    listStyleCount = 9,
} ListStyle;

enum
{
    listLocaleSize = 2 * listStyleCount,
};

#endif
