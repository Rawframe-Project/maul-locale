// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unsafe-backward ranges (collation_unsafe_table.h).

#include "collation_unsafe_table.h"

#include "table_gen.h"

#include <stdlib.h>

enum
{
    codePoints = 0x110000,
};

// Marks the list a contraction CE32 names.
static void MarkContraction(uint32_t ce32, bool* contraction)
{
    if (mlocCe32IsSpecial(ce32) && mlocCe32Tag(ce32) == collationTagContraction)
    {
        contraction[mlocCe32Index(ce32)] = true;
    }
}

// Marks a trie value's contraction, or the contractions its prefix
// list's default and entries name.
static void MarkValue(const CollationRoot* r, uint32_t ce32, bool* contraction)
{
    MarkContraction(ce32, contraction);
    if (!mlocCe32IsSpecial(ce32) || mlocCe32Tag(ce32) != collationTagPrefix)
    {
        return;
    }
    uint32_t at = mlocCollationListStart(r, mlocCe32Index(ce32));
    MarkContraction(mlocCollationContext(r, at), contraction);
    uint32_t count = mlocCollationContext(r, at + 1);
    at += 2;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t length = mlocCollationContext(r, at);
        MarkContraction(mlocCollationContext(r, at + 1 + length), contraction);
        at += length + 2;
    }
}

// The Hangul syllable a jamo entry spells (L V, or L V T), or 0.
static uint32_t Syllable(const CollationRoot* r, uint32_t at, uint32_t length)
{
    uint32_t l = mlocCollationContext(r, at);
    uint32_t v = length >= 2 ? mlocCollationContext(r, at + 1) : 0;
    uint32_t t = length == 3 ? mlocCollationContext(r, at + 2) : 0x11A7;
    if (length < 2 || length > 3 || l < 0x1100 || l > 0x1112 || v < 0x1161 || v > 0x1175 ||
        t < 0x11A7 || t > 0x11C2 || (length == 3 && t == 0x11A7))
    {
        return 0;
    }
    return 0xAC00 + ((l - 0x1100) * 21 + (v - 0x1161)) * 28 + (t - 0x11A7);
}

// Sets each code point of a contraction list's entries. In U+FDD1's
// list a Hangul script start is the syllable, as ICU4C's string holds it
// (the data writes its jamo, as an NFD text reads it).
static void SetList(const CollationRoot* r, uint32_t list, uint8_t* set, bool scripts)
{
    uint32_t at = mlocCollationListStart(r, list);
    uint32_t count = mlocCollationContext(r, at + 1);
    at += 2;
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t length = mlocCollationContext(r, at);
        uint32_t syllable = scripts ? Syllable(r, at + 1, length) : 0;
        for (uint32_t k = 0; k < length; k++)
        {
            uint32_t c = syllable != 0 ? syllable : mlocCollationContext(r, at + 1 + k);
            set[c / 8] |= (uint8_t)(1u << (c % 8));
        }
        at += length + 2;
    }
}

static bool Has(const uint8_t* set, uint32_t c)
{
    return (set[c / 8] >> (c % 8) & 1) != 0;
}

uint8_t* CollationUnsafeAppend(uint8_t* unit, size_t* size, const CollationRoot* view)
{
    bool* contraction = GenAllocate(sizeof(bool) * (view->listCount + 1));
    uint8_t* set = GenAllocate(codePoints / 8);
    for (uint32_t i = 0; i <= view->listCount; i++)
    {
        contraction[i] = false;
    }
    for (uint32_t i = 0; i < codePoints / 8; i++)
    {
        set[i] = 0;
    }
    MarkValue(view, view->trie.highValue, contraction);
    for (uint32_t i = 0; i < view->trie.dataLength; i++)
    {
        MarkValue(view, mlocLoad32(view->trie.data + (size_t)i * 4), contraction);
    }
    uint32_t scripts = mlocTrieValue(&view->trie, 0xFDD1);
    bool hasScripts = mlocCe32IsSpecial(scripts) && mlocCe32Tag(scripts) == collationTagContraction;
    for (uint32_t i = 0; i < view->listCount; i++)
    {
        if (contraction[i])
        {
            SetList(view, i, set, hasScripts && i == mlocCe32Index(scripts));
        }
    }
    size_t count = 0;
    for (uint32_t c = 0; c < codePoints; c++)
    {
        count += Has(set, c) && (c == 0 || !Has(set, c - 1));
    }
    uint8_t* grown = realloc(unit, *size + 4 + count * 8);
    if (grown == nullptr)
    {
        GenDie("unsafe ranges", "out of memory");
    }
    uint8_t* out = grown + *size;
    GenPut(out, count, 4);
    out += 4;
    for (uint32_t c = 0; c < codePoints; c++)
    {
        if (Has(set, c) && (c == 0 || !Has(set, c - 1)))
        {
            uint32_t last = c;
            while (last + 1 < codePoints && Has(set, last + 1))
            {
                last++;
            }
            GenPut(out, c, 4);
            GenPut(out + 4, last, 4);
            out += 8;
        }
    }
    *size += 4 + count * 8;
    free(set);
    free(contraction);
    return grown;
}
