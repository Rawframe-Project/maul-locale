// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The core every number formatter embeds (record mloc-0021): a locale's
// digits, symbols, pattern and grouping resolved from the data, and the
// writing of a rounded decimal with them.

#ifndef MLOC_NUMBER_CORE_H
#define MLOC_NUMBER_CORE_H

#include "number_tables.h"

#include "maul-locale/number.h"

#include <stddef.h>

// Which sign a number shows.
typedef enum NumberSign
{
    numberSignNone,
    numberSignMinus,
    numberSignPlus,
} NumberSign;

// The affixes of a pattern, in its record's order.
enum
{
    affixPositivePrefix,
    affixPositiveSuffix,
    affixNegativePrefix,
    affixNegativeSuffix,
    affixCount,
};

typedef struct NumberCore
{
    // The texts of the symbol and pattern units.
    const uint8_t* symbolText;
    const uint8_t* affixText;
    uint16_t symbolStart[numberSymbolCount];
    uint16_t symbolLength[numberSymbolCount];
    uint16_t affixStart[affixCount];
    uint16_t affixLength[affixCount];
    // The ten digits, numberDigitSize bytes apart, each digitLength long.
    uint8_t digits[10 * numberDigitSize];
    uint8_t digitLength;
    uint8_t primary;
    uint8_t secondary;
    uint8_t minGrouping;
} NumberCore;

// Where a formatter writes: the text pass copies bytes while they fit
// and counts them all; the parts pass, given the output with its part
// function, reports each run of one part kind over the text already
// written.
typedef struct NumberWriter
{
    char* buffer;
    size_t capacity;
    size_t length;
    const mlocOutput* parts;
    uint32_t kind;
    size_t start;
} NumberWriter;

// Resolves a locale's core: its numbers unit along the main chain, the
// numbering system its -u-nu keyword or its data names, and that
// system's entry (latn's when it has none of its own). Returns
// mloc_success, mloc_fellBack when the system asked for has no digits
// and the default stands instead, or mloc_errorNotFound for data without
// numbers or whose units disagree.
mlocResult mlocInitNumberCore(NumberCore* coreOut, const mlocDataSet* data,
                              const mlocLocale* locale);

// The sign a rounded decimal shows under a sign display.
NumberSign mlocNumberSign(const mlocDecimal* decimal, mlocSignDisplay display);

// Writes a rounded decimal in standard notation: the affixes for its
// sign, the integer digits (at least minInteger) grouped as grouping
// says, and the fraction digits it shows.
void mlocWriteStandard(const NumberCore* core, const mlocDecimal* decimal, NumberSign sign,
                       mlocGrouping grouping, uint32_t minInteger, NumberWriter* writer);

// Ends a writer's last part.
void mlocEndParts(NumberWriter* writer);

#endif
