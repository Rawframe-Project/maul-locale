// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out part's units (src/spellout_tables.h): their validation
// when a data file opens, and the parts of a valid group.

#ifndef MLOC_SPELLOUT_DATA_H
#define MLOC_SPELLOUT_DATA_H

#include "data_view.h"
#include "spellout_tables.h"

#include <stdint.h>

// Whether a group's unit holds: its sizes, every offset, index and
// target in range, its text well-formed UTF-8, each set's bases in
// order.
bool mlocValidSpelloutRules(const DataView* view, DataSpan payload);

// Whether the numbering systems unit holds, its ids ascending.
bool mlocValidSpelloutSystems(const DataView* view, DataSpan payload);

// The parts of a valid group's unit.
typedef struct SpelloutRules
{
    const uint8_t* sets;
    const uint8_t* rules;
    const uint8_t* pieces;
    const uint8_t* plurals;
    const uint8_t* patterns;
    const uint8_t* text;
    uint32_t setCount;
    uint32_t ruleCount;
    uint32_t pieceCount;
    uint32_t pluralCount;
    uint32_t patternCount;
    uint32_t defaultSet;
    uint32_t textSize;
} SpelloutRules;

SpelloutRules mlocReadSpelloutRules(DataSpan payload);

// A rule's divisor (ten, or a normal rule's radix, to its exponent), or 0
// when it passes 64 bits, which a valid unit never has.
uint64_t mlocSpelloutDivisor(const uint8_t* rule);

#endif
