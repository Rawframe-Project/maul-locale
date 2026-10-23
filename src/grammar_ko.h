// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Korean particles: the final a text's end is read with (a Hangul
// syllable's, a jamo's, a Hanja's by Unihan's kHangul), the Latin letters'
// Korean names, and each particle's form after a consonant, after a vowel,
// or both where the end cannot be read.

#ifndef MLOC_GRAMMAR_KO_H
#define MLOC_GRAMMAR_KO_H

#include "maul-locale/grammar.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The final a particle reads.
typedef enum KoreanFinal
{
    // No reading, or readings whose finals differ.
    koreanUnknown = 0,
    koreanVowel = 1,
    // ㄹ, which the (으)로 family reads as a vowel.
    koreanRieul = 2,
    koreanConsonant = 3,
} KoreanFinal;

// Whether Korean has what a request asks: a particle and nothing of the
// Turkic, Hungarian or Finnish fields.
bool mlocKoreanRequestValid(const mlocInflection* request);

// The final of the code point that ends text[0, length).
KoreanFinal mlocKoreanFinal(const uint8_t* text, size_t length);

// The Korean name of the Latin letter that ends text[0, end), as the
// loanword rules write it (에이, 엘); null for none.
const char* mlocKoreanLetterName(const uint8_t* text, size_t end);

// The stem a pronoun takes before the subject particle 가 (나: 내가,
// 너: 네가, 저: 제가, 누구: 누가); null for another word.
const char* mlocKoreanSubjectStem(const uint8_t* word, size_t length);

// Writes a particle's form after a final into out (both forms, the
// consonant form's own syllables in parentheses, for an unknown one:
// "은(는)", "(으)로"); returns its length, 0 for none.
size_t mlocKoreanParticleText(mlocKoreanParticle particle, KoreanFinal final, char* out,
                              size_t room);

#endif
