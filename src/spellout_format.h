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
    // infinity are written in: here, or in source when it is set.
    NumberCore core;
    // The formatter the fields below were copied from, without its core,
    // which formatting reads only for a pattern or a symbol; null when
    // core is here (always so in a formatter as stored).
    const void* source;
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
    // Whether the first word is titlecased, by the titlecase unit, and
    // "i" as Turkish and Azerbaijani titlecase it.
    bool capitalize;
    bool turkic;
    const uint8_t* titlecase;
    uint32_t titlecaseSize;
} SpelloutFormat;

// The first code point of well-formed UTF-8 text, 0 for none.
uint32_t mlocSpelloutFirstCodePoint(const uint8_t* text, size_t length);

// Whether a core holds what a formatter reads of it.
bool mlocValidSpelloutCore(const NumberCore* core);

// Writes a valid decimal by a formatter's rule set, with the text's first
// code point when a rule's text or a symbol begins it (0 when a pattern's
// digits do, or for no text). Returns mloc_errorCapacity when the rules
// nest past 64 levels or a division passes the decimal's digits, or
// mloc_errorInvalid for a source whose core does not hold; the writer's
// text is then partial.
mlocResult mlocWriteSpellout(const SpelloutFormat* format, const mlocDecimal* decimal,
                             NumberWriter* writer, uint32_t* firstOut);

#endif
