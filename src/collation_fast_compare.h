// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fast Latin compare (record mloc-0072): ICU4C's
// CollationFastLatin::compareUTF8 over a unit's table, which answers as
// the general compare does for text it holds and bails out otherwise.
// Each level reads both texts again, as the general compare does; the
// first reads every character, so a bail-out comes before any answer.
// A character's primary comes from its table word (ICU4C keeps an array
// of them in its settings).

#ifndef MLOC_SRC_COLLATION_FAST_COMPARE_H
#define MLOC_SRC_COLLATION_FAST_COMPARE_H

#include "collation_compare.h"
#include "collation_tables.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // The compare's answer when the general one must decide.
    fastLatinBail = -2,
};

// The settings as the fast compare reads them (ICU4C's getOptions).
typedef struct FastLatinOptions
{
    // The table's character words, after its head.
    const uint8_t* words;
    // The highest variable mini CE: below the long primaries when not
    // shifted.
    uint32_t variableTop;
    uint32_t strength;
    bool backwards;
    bool caseLevel;
    // Upper case first on the case level; case bits in the tertiaries,
    // upper case first there.
    bool upperFirst;
    bool tertiaryCase;
    bool tertiaryUpperFirst;
    // Digits bail out: numeric ordering, or digits reordered apart from
    // the groups before Latin.
    bool digitsBail;
} FastLatinOptions;

// The options for a table (its words and count) under settings; false
// when the fast compare cannot serve them: no table, the identical
// strength, a variable top that ends no group, or a reordering that
// moves the groups before Latin.
bool mlocFastLatinOptions(const CollationRoot* root, const CollationSettings* settings,
                          const uint8_t* table, uint32_t count, FastLatinOptions* out);

// -1, 0 or 1 as a sorts before, with or after b up to the quaternary
// level, or fastLatinBail.
int mlocFastLatinCompare(const FastLatinOptions* options, const uint8_t* a, size_t aLength,
                         const uint8_t* b, size_t bLength);

#endif
