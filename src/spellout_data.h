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

// Whether the titlecase unit holds: its code points ascending, each
// titlecase in range and well-formed UTF-8.
bool mlocValidSpelloutTitlecase(const DataView* view, DataSpan payload);

// A lowercase letter's titlecase from a valid titlecase unit; false for a
// code point it does not list.
bool mlocSpelloutTitlecase(DataSpan unit, uint32_t codePoint, const uint8_t** textOut,
                           uint32_t* lengthOut);

// An algorithmic numbering system: the locale whose rules it names
// (empty for the locale asked for), the group and the rule set.
typedef struct SpelloutSystem
{
    const char* locale;
    const char* name;
    uint8_t localeLength;
    uint8_t nameLength;
    uint8_t group;
} SpelloutSystem;

// A numbering system of a valid systems unit by its id; false for none.
bool mlocFindSpelloutSystem(DataSpan unit, const char* id, size_t length, SpelloutSystem* out);

// The parts of a valid group's unit.
typedef struct SpelloutRules
{
    const uint8_t* sets;
    const uint8_t* leads;
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
    uint32_t flags;
} SpelloutRules;

SpelloutRules mlocReadSpelloutRules(DataSpan payload);

// A rule's divisor (ten, or a normal rule's radix, to its exponent), or 0
// when it passes 64 bits, which a valid unit never has.
uint64_t mlocSpelloutDivisor(const uint8_t* rule);

#endif
