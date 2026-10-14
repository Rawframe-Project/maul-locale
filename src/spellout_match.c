// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A rule's text as the parser matches it (spellout_match.h).

#include "spellout_match.h"

#include <string.h>

static const uint8_t* Piece(const SpelloutRules* r, uint32_t index)
{
    return r->pieces + (size_t)index * spelloutPieceSize;
}

static SpelloutText PieceText(const SpelloutRules* r, const uint8_t* piece)
{
    return (SpelloutText){r->text + mlocLoad16(piece + 4), mlocLoad16(piece + 2)};
}

SpelloutParts mlocReadSpelloutParts(const SpelloutRules* r, const SpelloutRule* rule)
{
    SpelloutParts parts = {.firstPiece = rule->firstPiece, .pieceCount = rule->pieces};
    int subs = 0;
    for (uint32_t i = 0; i < rule->pieces; i++)
    {
        const uint8_t* piece = Piece(r, rule->firstPiece + i);
        if (piece[0] == spelloutSubstitution && subs < 2)
        {
            parts.subAt[subs] = i;
            parts.sub[subs++] = piece;
        }
        else if (piece[0] == spelloutPlural)
        {
            parts.plural = piece;
        }
    }
    parts.prefixEnd = subs > 0 ? parts.subAt[0] : rule->pieces;
    parts.middleEnd = subs > 1 ? parts.subAt[1] : rule->pieces;
    return parts;
}

bool mlocSpelloutDelimiter(const SpelloutRules* r, const SpelloutParts* parts, uint32_t from,
                           uint32_t to, SpelloutText* textOut)
{
    *textOut = (SpelloutText){nullptr, 0};
    bool plural = false;
    for (uint32_t i = from; i < to; i++)
    {
        const uint8_t* piece = Piece(r, parts->firstPiece + i);
        if (piece[0] == spelloutText)
        {
            *textOut = PieceText(r, piece);
        }
        plural = plural || piece[0] == spelloutPlural;
    }
    return plural;
}

// UnicodeString::indexOf: the first match at or after from; an empty key
// matches nowhere.
static int64_t IndexOf(SpelloutText text, SpelloutText key, size_t from)
{
    if (key.length == 0 || key.length > text.length)
    {
        return -1;
    }
    for (size_t at = from; at + key.length <= text.length; at++)
    {
        if (text.bytes[at] == key.bytes[0] && memcmp(text.bytes + at, key.bytes, key.length) == 0)
        {
            return (int64_t)at;
        }
    }
    return -1;
}

// Whether text at a position holds the text pieces of a range in order,
// and the length they take.
static bool HoldsPieces(const SpelloutRules* r, const SpelloutParts* parts, uint32_t from,
                        uint32_t to, SpelloutText text, size_t at, size_t* lengthOut)
{
    size_t length = 0;
    for (uint32_t i = from; i < to; i++)
    {
        const uint8_t* piece = Piece(r, parts->firstPiece + i);
        if (piece[0] != spelloutText)
        {
            continue;
        }
        SpelloutText t = PieceText(r, piece);
        if (at + length + t.length > text.length ||
            memcmp(text.bytes + at + length, t.bytes, t.length) != 0)
        {
            return false;
        }
        length += t.length;
    }
    *lengthOut = length;
    return true;
}

// The length of the text pieces of a range.
static size_t PiecesLength(const SpelloutRules* r, const SpelloutParts* parts, uint32_t from,
                           uint32_t to)
{
    size_t length = 0;
    for (uint32_t i = from; i < to; i++)
    {
        const uint8_t* piece = Piece(r, parts->firstPiece + i);
        length += piece[0] == spelloutText ? mlocLoad16(piece + 2) : 0;
    }
    return length;
}

// NFRule::findText for a rule with a plural block, through
// PluralFormat::parseType: whatever the key, the form found first in the
// block's order, a later or equal one kept only when longer, must have
// the rule's text before the block (its substitutions taken out) just
// before it and the text after just after.
static int64_t FindPlural(const SpelloutRules* r, const SpelloutParts* parts, SpelloutText text,
                          size_t from, size_t* lengthOut)
{
    const uint8_t* plural = r->plurals + (size_t)mlocLoad16(parts->plural + 2) * spelloutPluralSize;
    uint32_t order = mlocLoad32(plural + 24);
    int64_t matched = -1;
    size_t matchedLength = 0;
    for (uint32_t k = 0; k < 6 && (order >> (4 * k) & 0xF) != 0xF; k++)
    {
        uint32_t c = order >> (4 * k) & 0xF;
        SpelloutText form = {r->text + mlocLoad16(plural + 4 * c), mlocLoad16(plural + 4 * c + 2)};
        int64_t at = IndexOf(text, form, from);
        if (at >= 0 && at >= matched && form.length > matchedLength)
        {
            matched = at;
            matchedLength = form.length;
        }
    }
    uint32_t block = 0;
    while (Piece(r, parts->firstPiece + block) != parts->plural)
    {
        block++;
    }
    size_t before = PiecesLength(r, parts, 0, block);
    size_t after = 0;
    size_t unused = 0;
    if (matched < 0 || (size_t)matched < before ||
        !HoldsPieces(r, parts, 0, block, text, (size_t)matched - before, &unused) ||
        !HoldsPieces(r, parts, block + 1, parts->pieceCount, text, (size_t)matched + matchedLength,
                     &after))
    {
        return -1;
    }
    *lengthOut = before + matchedLength + after;
    return matched - (int64_t)before;
}

int64_t mlocFindSpelloutText(const SpelloutRules* r, const SpelloutParts* parts, SpelloutText text,
                             SpelloutText key, size_t from, size_t* lengthOut)
{
    if (parts->plural != nullptr)
    {
        return FindPlural(r, parts, text, from, lengthOut);
    }
    *lengthOut = key.length;
    return IndexOf(text, key, from);
}
