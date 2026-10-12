// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The locale part's tables, as the generator writes them and the
// library reads them. Subtags are packed into numbers: a letter is 1 to
// 26 in 5 bits, so a language of up to 3 letters takes 15 bits, a
// script 20, and a region 11 (two letters below 1024, or 1024 plus a
// three-digit number); zero is "none". A packed language, script and
// region together take 46 bits, language highest.
//
// Likely subtags (kindLocaleLikely), little-endian:
//   u32 languageCount, u32 generalCount, u64 zero
//   u64 generalKeys[generalCount], sorted, unique: language, script and
//       region packed, for every entry but a language alone
//   u64 generalValues[generalCount]: the full language, script, region
//   u32 languageValues[languageCount]: script << 11 | region
//   u16 languageKeys[languageCount], sorted, unique: a language alone
//       (never "und", which is a general entry)
//
// Aliases (kindLocaleAliases), the rules of UTS #35 Annex C in its
// order, each indexed under the first field its type has:
//   u32 ruleCount, keyCount, variantCount, regionCount, legacyCount,
//       textSize, u64 zero
//   rules[ruleCount], 32 bytes each: u32 typeScript, replacementScript;
//       u16 typeLanguage, typeRegion, typeVariants[2],
//       replacementLanguage, replacementRegion, replacementVariants[2],
//       regionStart; u8 regionCount, variantCounts (type | replacement
//       << 4). A language of 0 is "und"; a variant is an index into
//       variants; regionCount replacement regions start at regionStart.
//   u32 keys[keyCount], sorted, then u16 keyRules[keyCount] in the same
//       order: field << 30 | value (0 language, 1 script, 2 region, 3
//       variant), and the rule, sorted by rule within a key
//   zero bytes to a multiple of 8
//   char variants[variantCount][8], sorted, NUL-padded
//   u16 regions[regionCount]
//   legacy[legacyCount], 8 bytes each: u16 typeOffset, typeLength,
//       replacementOffset, replacementLength in text; types sorted, in
//       lowercase with "-"
//   char text[textSize]

#ifndef MLOC_LOCALE_TABLES_H
#define MLOC_LOCALE_TABLES_H

#include <stdbool.h>
#include <stdint.h>

static inline uint32_t mlocPackLetter(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (uint32_t)(c - 'a' + 1);
    }
    if (c >= 'A' && c <= 'Z')
    {
        return (uint32_t)(c - 'A' + 1);
    }
    return 0;
}

// A language of 2 or 3 letters, or 0 for any other.
static inline uint32_t mlocPackLanguage(const char* language, uint32_t length)
{
    if (length < 2 || length > 3)
    {
        return 0;
    }
    uint32_t packed = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        uint32_t letter = i < length ? mlocPackLetter(language[i]) : 0;
        if (i < length && letter == 0)
        {
            return 0;
        }
        packed = packed << 5 | letter;
    }
    return packed;
}

// A script of 4 letters, or 0.
static inline uint32_t mlocPackScript(const char* script, uint32_t length)
{
    if (length != 4)
    {
        return 0;
    }
    uint32_t packed = 0;
    for (uint32_t i = 0; i < 4; i++)
    {
        uint32_t letter = mlocPackLetter(script[i]);
        if (letter == 0)
        {
            return 0;
        }
        packed = packed << 5 | letter;
    }
    return packed;
}

// A region of 2 letters or 3 digits, or 0.
static inline uint32_t mlocPackRegion(const char* region, uint32_t length)
{
    if (length == 2)
    {
        uint32_t a = mlocPackLetter(region[0]);
        uint32_t b = mlocPackLetter(region[1]);
        return a == 0 || b == 0 ? 0 : a << 5 | b;
    }
    if (length == 3)
    {
        uint32_t number = 0;
        for (uint32_t i = 0; i < 3; i++)
        {
            if (region[i] < '0' || region[i] > '9')
            {
                return 0;
            }
            number = number * 10 + (uint32_t)(region[i] - '0');
        }
        return 1024 + number;
    }
    return 0;
}

enum
{
    aliasHeaderSize = 32,
    aliasRuleSize = 32,
    aliasLegacySize = 8,
    aliasMaxVariants = 2,
};

static inline uint64_t mlocPackTag(uint32_t language, uint32_t script, uint32_t region)
{
    return (uint64_t)language << 31 | (uint64_t)script << 11 | region;
}

// Whether count 5-bit groups, highest first, hold letters: the first
// `required` must, and the rest may be zero once one is.
static inline bool mlocPackedLetters(uint64_t packed, int count, int required)
{
    for (int i = 0; i < count; i++)
    {
        uint32_t letter = (uint32_t)(packed >> (5 * (count - 1 - i))) & 31;
        if (letter > 26 || (i < required && letter == 0))
        {
            return false;
        }
        if (letter == 0 && i + 1 < count && ((packed >> (5 * (count - 2 - i))) & 31) != 0)
        {
            return false;
        }
    }
    return true;
}

static inline bool mlocValidPackedLanguage(uint32_t language)
{
    return language < 1u << 15 && mlocPackedLetters(language, 3, 2);
}

static inline bool mlocValidPackedScript(uint32_t script)
{
    return script < 1u << 20 && mlocPackedLetters(script, 4, 4);
}

static inline bool mlocValidPackedRegion(uint32_t region)
{
    return (region >= 1024 && region < 2024) || (region < 1024 && mlocPackedLetters(region, 2, 2));
}

// Writes a packed language, NUL-padded to 8 bytes.
static inline void mlocUnpackLanguage(uint32_t packed, char* language)
{
    for (int i = 0; i < 8; i++)
    {
        uint32_t letter = i < 3 ? packed >> (5 * (2 - i)) & 31 : 0;
        language[i] = letter == 0 ? '\0' : (char)('a' + letter - 1);
    }
}

// Writes a packed script in title case, 4 bytes.
static inline void mlocUnpackScript(uint32_t packed, char* script)
{
    for (int i = 0; i < 4; i++)
    {
        char base = i == 0 ? 'A' : 'a';
        script[i] = (char)(base + (packed >> (5 * (3 - i)) & 31) - 1);
    }
}

// Writes a packed region, NUL-padded to 4 bytes.
static inline void mlocUnpackRegion(uint32_t packed, char* region)
{
    region[3] = '\0';
    if (packed >= 1024)
    {
        uint32_t number = packed - 1024;
        region[0] = (char)('0' + number / 100);
        region[1] = (char)('0' + number / 10 % 10);
        region[2] = (char)('0' + number % 10);
        return;
    }
    region[0] = (char)('A' + (packed >> 5) - 1);
    region[1] = (char)('A' + (packed & 31) - 1);
    region[2] = '\0';
}

#endif
