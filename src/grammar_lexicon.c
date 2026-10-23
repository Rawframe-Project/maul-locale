// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Turkish lexicon of marks (grammar_lexicon.h): a word read into its
// key, the table's blocks searched by their first word, then the block's
// front-coded entries read in order.

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

// A letter in Turkish lowercase, or 0 for a code point no Turkish word
// holds.
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
    case 0xE2:
    case 0xEE:
    case 0xFB:
        return c;
    case 0xC7:
    case 0xD6:
    case 0xDC:
    case 0xC2:
    case 0xCE:
    case 0xDB:
        return c + 32;
    case 0x11E:
    case 0x15E:
        return c + 1;
    case 0x130:
        return 'i';
    default:
        return 0;
    }
}

// A lowercase letter without its circumflex.
static uint32_t Plain(uint32_t c)
{
    return c == 0xE2 ? 'a' : c == 0xEE ? 'i' : c == 0xFB ? 'u' : c;
}

// Decodes the code point at word[*at] and moves past it.
static uint32_t Next(const uint8_t* word, size_t length, size_t* at)
{
    size_t i = *at;
    uint32_t c = word[i];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
    for (size_t k = 1; k <= extra && i + k < length; k++)
    {
        c = c << 6 | (word[i + k] & 0x3F);
    }
    *at = i + extra + 1;
    return c;
}

// The key of a word, without its circumflexes when plain; false when it
// holds a letter no Turkish word has. Whether the word had a circumflex
// goes in circumflexOut.
static bool KeyOf(const uint8_t* word, size_t length, bool plain, uint8_t* key, size_t* keyLength,
                  bool* circumflexOut)
{
    *keyLength = 0;
    *circumflexOut = false;
    for (size_t i = 0; i < length;)
    {
        uint32_t lower = Lower(Next(word, length, &i));
        *circumflexOut = *circumflexOut || Plain(lower) != lower;
        if (lower == 0 || !Put(plain ? Plain(lower) : lower, key, keyLength))
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

// The length of the prefix two byte strings share.
static size_t Common(const uint8_t* a, size_t aLength, const uint8_t* b, size_t bLength)
{
    size_t k = 0;
    while (k < aLength && k < bLength && a[k] == b[k])
    {
        k++;
    }
    return k;
}

// Finds a key in a block of the table, whose entries are sorted.
static bool ScanBlock(const uint8_t* key, size_t keyLength, size_t block, TurkicMarks* out)
{
    // Front coding: an entry sharing more with the one before than that
    // one shared with the key is below the key too, one sharing less is
    // past it; only an entry sharing as much is compared, from there on.
    size_t matched = 0;
    size_t at = s_turkishMarkBlocks[block];
    for (size_t i = 0; i < turkishMarkBlock && at < turkishMarkBytes; i++)
    {
        size_t shared = s_turkishMarks[at];
        size_t own = s_turkishMarks[at + 1];
        if (shared + own > keyRoom || shared < matched)
        {
            return false;
        }
        const uint8_t* bytes = s_turkishMarks + at + 2;
        uint8_t marks = bytes[own];
        bool compound = (marks & turkicCompound) != 0;
        if (shared == matched)
        {
            size_t rest = keyLength - matched;
            size_t k = Common(bytes, own, key + matched, rest);
            if (k == own && k == rest)
            {
                out->marks = marks;
                out->possessive = compound ? bytes[own + 1] : 0;
                return true;
            }
            if (k == rest || (k < own && bytes[k] > key[matched + k]))
            {
                // Past the key in the sorted block.
                return false;
            }
            matched += k;
        }
        at += 3 + own + (compound ? 1 : 0);
    }
    return false;
}

// Finds a key's marks in the table.
static bool Find(const uint8_t* key, size_t keyLength, TurkicMarks* out)
{
    // The last block whose first word is at most the key, among the
    // blocks whose first words begin with the key's first byte or below
    // it, past the last of those below it but one.
    size_t below = s_turkishMarkFirst[key[0]];
    size_t low = below > 0 ? below - 1 : 0;
    size_t high = s_turkishMarkFirst[key[0] + 1];
    if (high == 0)
    {
        return false;
    }
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
    return ScanBlock(key, keyLength, low, out);
}

bool mlocTurkicMarks(const uint8_t* word, size_t length, TurkicMarks* out)
{
    *out = (TurkicMarks){0};
    uint8_t key[keyRoom];
    size_t keyLength = 0;
    bool circumflex = false;
    // A circumflexed spelling as it is written first (hâl, beside hal),
    // then without its circumflexes (helâk, filed as helak).
    if (!KeyOf(word, length, false, key, &keyLength, &circumflex))
    {
        return false;
    }
    if (Find(key, keyLength, out))
    {
        return true;
    }
    return circumflex && KeyOf(word, length, true, key, &keyLength, &circumflex) &&
           Find(key, keyLength, out);
}
