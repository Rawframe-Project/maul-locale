// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Spell-out parsing: text read back by a formatter's group of rules as
// ICU4C 79's strict RuleBasedNumberFormat::parse reads it, the value
// exact.

#ifndef MLOC_SPELLOUT_PARSE_H
#define MLOC_SPELLOUT_PARSE_H

#include "spellout_format.h"

#include <stddef.h>
#include <stdint.h>

// Reads text by every public, parseable rule set of a formatter's group,
// keeping the longest match (the first of equal ones). Returns
// mloc_success with the value and the bytes read; mloc_errorSyntax, the
// end 0, when no rule set reads anything or the group cannot be parsed;
// mloc_errorCapacity when a value passes a decimal's digits or the
// search its steps.
mlocResult mlocReadSpellout(const SpelloutFormat* format, const uint8_t* text, size_t length,
                            mlocDecimal* valueOut, size_t* endOut);

#endif
