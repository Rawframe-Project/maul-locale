// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Turkish lexicon of marks (grammar_lexicon.h, record mloc-0046): a
// word read into its key, the table's blocks searched by their first
// word, then the block's front-coded entries read in order.

#include "grammar_lexicon.h"

#include "generated/turkish_marks.h"

#include <string.h>

enum
{
    // The longest key looked up; the table's words are shorter.
    keyRoom = 64,
};

// Encodes a code point as UTF-8 at key[*length], when it fits.
static bool Put(uint32_t c, uint8_t* key, size_t* length)
{
    size_t n = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
    if (*length + n > keyRoom)
    {
        return false;
    }
    if (n == 1)
    {
        key[(*length)++] = (uint8_t)c;
    }
    else if (n == 2)
    {
        key[(*length)++] = (uint8_t)(0xC0 | c >> 6);
        key[(*length)++] = (uint8_t)(0x80 | (c & 0x3F));
    }
    else
    {
        key[(*length)++] = (uint8_t)(0xE0 | c >> 12);
        key[(*length)++] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
        key[(*length)++] = (uint8_t)(0x80 | (c & 0x3F));
    }
    return true;
}

// A letter in Turkish lowercase without the circumflex, or 0 for a code
// point no Turkish word holds.
static uint32_t Lower(uint32_t c)
{
    if (c >= 'a' && c <= 'z')
    {
        return c;
    }
    if (c >= 'A' && c <= 'Z')
    {
        // Turkish's capital I is ı.
        return c == 'I' ? 0x131 : c + 32;
    }
    switch (c)
    {
    case 0xE7:
    case 0x11F:
    case 0x131:
    case 0xF6:
    case 0x15F:
    case 0xFC:
        return c;
    case 0xC7:
    case 0xD6:
    case 0xDC:
        return c + 32;
    case 0x11E:
    case 0x15E:
        return c + 1;
    case 0x130:
        return 'i';
    case 0xE2:
    case 0xC2:
        return 'a';
    case 0xEE:
    case 0xCE:
        return 'i';
    case 0xFB:
    case 0xDB:
        return 'u';
    default:
        return 0;
    }
}

// The key of a word; false when it holds a letter no Turkish word has.
static bool KeyOf(const uint8_t* word, size_t length, uint8_t* key, size_t* keyLength)
{
    *keyLength = 0;
    for (size_t i = 0; i < length;)
    {
        uint32_t c = word[i];
        size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        for (size_t k = 1; k <= extra && i + k < length; k++)
        {
            c = c << 6 | (word[i + k] & 0x3F);
        }
        i += extra + 1;
        uint32_t lower = Lower(c);
        if (lower == 0 || !Put(lower, key, keyLength))
        {
            return false;
        }
    }
    return *keyLength > 0;
}

// Compares a key with a block's first word, which shares nothing.
static int CompareHead(const uint8_t* key, size_t length, size_t block)
{
    const uint8_t* entry = s_turkishMarks + s_turkishMarkBlocks[block];
    size_t own = entry[1];
    size_t n = length < own ? length : own;
    int c = memcmp(key, entry + 2, n);
    return c != 0 ? c : (int)(length > own) - (int)(length < own);
}

bool mlocTurkicMarks(const uint8_t* word, size_t length, TurkicMarks* out)
{
    *out = (TurkicMarks){0};
    uint8_t key[keyRoom];
    size_t keyLength = 0;
    if (!KeyOf(word, length, key, &keyLength))
    {
        return false;
    }
    // The last block whose first word is at most the key.
    size_t low = 0;
    size_t high = turkishMarkBlocks;
    while (high - low > 1)
    {
        size_t middle = low + (high - low) / 2;
        if (CompareHead(key, keyLength, middle) < 0)
        {
            high = middle;
        }
        else
        {
            low = middle;
        }
    }
    uint8_t current[keyRoom];
    size_t at = s_turkishMarkBlocks[low];
    for (size_t i = 0; i < turkishMarkBlock && at < turkishMarkBytes; i++)
    {
        size_t shared = s_turkishMarks[at];
        size_t own = s_turkishMarks[at + 1];
        if (shared + own > keyRoom)
        {
            return false;
        }
        memcpy(current + shared, s_turkishMarks + at + 2, own);
        size_t total = shared + own;
        uint8_t marks = s_turkishMarks[at + 2 + own];
        bool compound = (marks & turkicCompound) != 0;
        if (total == keyLength && memcmp(current, key, total) == 0)
        {
            out->marks = marks;
            out->possessive = compound ? s_turkishMarks[at + 3 + own] : 0;
            return true;
        }
        at += 3 + own + (compound ? 1 : 0);
    }
    return false;
}
