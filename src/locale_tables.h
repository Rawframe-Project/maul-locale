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

#ifndef MLOC_LOCALE_TABLES_H
#define MLOC_LOCALE_TABLES_H

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

static inline uint64_t mlocPackTag(uint32_t language, uint32_t script, uint32_t region)
{
    return (uint64_t)language << 31 | (uint64_t)script << 11 | region;
}

#endif
