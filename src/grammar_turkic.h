// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Turkish suffixes (record mloc-0046): a spoken end read into its last
// vowel and final sound, and the suffixes a request asks for written by
// vowel harmony, assimilation and buffer consonants.

#ifndef MLOC_GRAMMAR_TURKIC_H
#define MLOC_GRAMMAR_TURKIC_H

#include "maul-locale/grammar.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    // The most bytes a request's suffixes take, the question particle
    // with its space included.
    turkicSuffixRoom = 48,
};

// How a spoken end sounds: its last vowel ('a', 'e', 'i' for ı, 'I' for
// i, 'o', 'O' for ö, 'u', 'U' for ü; 0 for none) and its final sound.
typedef struct TurkicSound
{
    char vowel;
    bool finalVowel;
    bool voiceless;
} TurkicSound;

// The suffixes written for a request: the bytes before the particle and
// the particle's (with its space).
typedef struct TurkicSuffixes
{
    char text[turkicSuffixRoom];
    uint8_t length;
    uint8_t particle;
    // Whether the first suffix begins with a vowel (a final stop then
    // softens in a common noun).
    bool vowelFirst;
} TurkicSuffixes;

// Whether a request is one Turkish has.
bool mlocTurkicRequestValid(const mlocInflection* request);

// Reads the sound of a spoken word's end (UTF-8, valid), or false when
// it has no vowel. A loanword the spelling guides list as breaking
// harmony reads its last vowel front ("saat" as "saet").
bool mlocTurkicSound(const uint8_t* word, size_t length, TurkicSound* out);

// The name of a letter as Turkish reads it ("ke" for K, "çift ve" for
// W), or null for a code point that is no letter Turkish names.
const char* mlocTurkicLetterName(uint32_t codePoint);

// Writes the suffixes a request asks for after a sound.
void mlocTurkicSuffixes(TurkicSound sound, const mlocInflection* request, TurkicSuffixes* out);

// The softened form of a common noun's final stop before a vowel (b for
// p, c for ç, ğ for k, g for k after n), as UTF-8 with the bytes of the
// stem's end it replaces; false when the word keeps its stop (one
// syllable, a final t, or another letter).
bool mlocTurkicSoften(const uint8_t* word, size_t length, const char** replacementOut,
                      size_t* replacedOut);

#endif
