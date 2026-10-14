// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The lead records of a spell-out group (src/spellout_tables.h): per rule
// set, what lets the parser refuse at once a place in the text no rule of
// the set can start reading at.

#ifndef MLOCGEN_SPELLOUT_LEAD_H
#define MLOCGEN_SPELLOUT_LEAD_H

#include "spellout_compile.h"

#include <stdint.h>

// Whether ICU keeps a special rule: every fraction rule, and the last
// negative, infinity and NaN rule.
bool SpelloutKept(const RbnfSet* set, int index);

// Writes a group's lead records, spelloutLeadSize bytes per set in the
// sets' order, of the rules the unit keeps.
void SpelloutLeads(const RbnfGroup* group, uint8_t* out);

#endif
