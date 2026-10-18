// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Turkish suffixes (record mloc-0046): a spoken end read into its last
// vowel and final sound, and the suffixes a request asks for written by
// vowel harmony, assimilation and buffer consonants.

#ifndef MLOC_GRAMMAR_TURKIC_H
#define MLOC_GRAMMAR_TURKIC_H

#include "grammar_lexicon.h"

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
// it has no vowel; with inverse, its last vowel reads front (a word of
// inverse harmony: "saat" as "saet").
bool mlocTurkicSound(const uint8_t* word, size_t length, bool inverse, TurkicSound* out);

// The name of a letter as Turkish reads it ("ke" for K, "çift ve" for
// W), or null for a code point that is no letter Turkish names.
const char* mlocTurkicLetterName(uint32_t codePoint);

// Writes the suffixes a request asks for after a sound; endsThird for a
// word that already ends in a third person's possessive (a compound:
// zeytinyağı), whose cases then take the pronominal n.
void mlocTurkicSuffixes(TurkicSound sound, const mlocInflection* request, bool endsThird,
                        TurkicSuffixes* out);

// A common noun's stem before a suffix that begins with a vowel: the
// bytes of the word kept, then a tail (its final stop softened, its
// final consonant doubled, the consonants after its dropped last
// vowel).
typedef struct TurkicStem
{
    size_t kept;
    char tail[12];
    uint8_t tailLength;
} TurkicStem;

// The stem of a common noun (UTF-8, valid) with a lexicon's marks
// (grammar_lexicon.h): by the rule the marks are written against, a
// final ç, k, p or t softens in a word of two syllables or more without
// inverse harmony and a final nk or og in any word, the marks saying
// otherwise; false when the word stays as it is.
bool mlocTurkicStem(const uint8_t* word, size_t length, uint8_t marks, TurkicStem* out);

#endif
