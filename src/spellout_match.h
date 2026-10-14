// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A rule's text as the parser matches it (spellout_parse.c): its pieces
// sorted out around its substitutions, and NFRule::findText, strict.

#ifndef MLOC_SPELLOUT_MATCH_H
#define MLOC_SPELLOUT_MATCH_H

#include "spellout_rule.h"

#include <stddef.h>
#include <stdint.h>

// A byte range of the text.
typedef struct SpelloutText
{
    const uint8_t* bytes;
    size_t length;
} SpelloutText;

// A rule's pieces sorted out: its substitutions, its plural block, and the
// text before, between and after the substitutions.
typedef struct SpelloutParts
{
    const uint8_t* sub[2];
    // The plural block's piece, or null.
    const uint8_t* plural;
    uint32_t firstPiece;
    uint32_t pieceCount;
    // The pieces of the prefix: those before the first substitution.
    uint32_t prefixEnd;
    // The pieces after each substitution, to the next or the end.
    uint32_t middleEnd;
    uint32_t subAt[2];
} SpelloutParts;

// A rule's pieces sorted out.
SpelloutParts mlocReadSpelloutParts(const SpelloutRules* r, const SpelloutRule* rule);

// The text of a delimiter's pieces (from, to), and whether it holds the
// plural block. Text pieces between two cuts are one at most, so a
// delimiter without the block is one piece or none.
bool mlocSpelloutDelimiter(const SpelloutRules* r, const SpelloutParts* parts, uint32_t from,
                           uint32_t to, SpelloutText* textOut);

// NFRule::findText, strict: where a key is found at or after a position
// and the length found, or -1. For a rule with a plural block, whatever
// the key, PluralFormat::parseType's form found first in the block's
// order (a later or equal one kept only when longer), which must have the
// rule's text before the block (its substitutions taken out) just before
// it and the text after just after.
int64_t mlocFindSpelloutText(const SpelloutRules* r, const SpelloutParts* parts, SpelloutText text,
                             SpelloutText key, size_t from, size_t* lengthOut);

#endif
