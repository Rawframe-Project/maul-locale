// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out engine (record mloc-0034): a rule set of a compiled
// group run on an exact decimal, as ICU4C 79's NFRuleSet::format and
// NFRule::doFormat run it, its int64_t path for integers that fit and
// its double path, made exact, for the rest.

#ifndef MLOC_SPELLOUT_FORMAT_H
#define MLOC_SPELLOUT_FORMAT_H

#include "number_core.h"
#include "spellout_data.h"

#include "maul-locale/decimal.h"

#include <stdint.h>

// A spell-out formatter: the group and rule set, and what the rules read
// of the locale.
typedef struct SpelloutFormat
{
    // The locale's symbols and digits, which decimal patterns, NaN and
    // infinity are written in.
    NumberCore core;
    // The group's unit, in the data.
    const uint8_t* group;
    uint32_t groupSize;
    // The first code point of the locale's decimal separator, which
    // chooses among fraction rules ("x.x" or "x,x").
    uint32_t decimalPoint;
    uint16_t set;
    // The plural rule sets of plural blocks: cardinal and ordinal.
    uint8_t cardinal;
    uint8_t ordinal;
    uint8_t stamp;
} SpelloutFormat;

// Writes a valid decimal by a formatter's rule set. Returns
// mloc_errorCapacity when the rules nest past 64 levels or a division
// passes the decimal's digits; the writer's text is then partial.
mlocResult mlocWriteSpellout(const SpelloutFormat* format, const mlocDecimal* decimal,
                             NumberWriter* writer);

#endif
