// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out rules (record mloc-0034, src/spellout_tables.h): every
// group of common/rbnf compiled (spellout_compile.h) and filed under its
// file's locale key, and the algorithmic numbering systems of
// numberingSystems.xml with the rule set each names, and the titlecase
// of the rules' lowercase letters. They ship as a data file of their own.

#ifndef MLOCGEN_SPELLOUT_GEN_H
#define MLOCGEN_SPELLOUT_GEN_H

#include "datafile.h"
#include "spellout_compile.h"

#include <stddef.h>
#include <stdint.h>

// A compiled group as its unit's payload, which the caller frees; the
// program stops on a group the layout cannot hold.
uint8_t* SpelloutPayload(const RbnfGroup* group, size_t* sizeOut);

// Whether a locale file's rule text is well-formed and titlecased by
// ICU as the library titlecases it: false for Dutch "ij" beginning a
// word, a Lithuanian combining dot above or Armenian "և", which ICU's
// case locales treat apart (Turkish and Azerbaijani "i" the library
// writes itself).
bool SpelloutCasesAsIcu(const char* id, const char* text, size_t length);

// Adds the spell-out units to a file. Returns 0 and a message on
// failure.
int GenerateSpellout(const char* commonDirectory, DataFile* file, char* errorOut,
                     size_t errorCapacity);

#endif
