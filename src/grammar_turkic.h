// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Turkic suffixes (records mloc-0046, mloc-0047): one engine reads a
// spoken end into its last vowel's harmony and its final sound's class,
// and writes the suffixes a request asks for from archiphonemes; each
// language is a table of letters, vowels, onsets and suffix templates
// (grammar_tr.c, grammar_kk.c, grammar_ky.c).

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

// The class of a sound, which chooses a suffix's first consonant: a
// vowel; a glide (Kazakh р, й, у); a liquid (л); a nasal (м, н, ң); any
// other voiced consonant; a voiceless one. A language without such
// choices uses vowel, voiced and voiceless alone.
typedef enum TurkicClass
{
    turkicVowel = 0,
    turkicGlide = 1,
    turkicLiquid = 2,
    turkicNasal = 3,
    turkicVoiced = 4,
    turkicVoiceless = 5,
    turkicClasses = 6,
} TurkicClass;

// A letter's information: its class in the low bits, and these.
enum
{
    turkicClassMask = 0x07,
    // A back vowel (a, ı, o, u).
    turkicBack = 0x08,
    // A rounded vowel (o, ö, u, ü).
    turkicRound = 0x10,
    // A glide, which gives the harmony only when the word has no other
    // vowel (Kazakh и, у, ю).
    turkicWeak = 0x20,
    // A sign that is no sound (Kazakh ь, ъ).
    turkicSign = 0x40,
    // On a sign or a word's final consonant: the suffixes are front
    // (Kazakh ь; к in паркке).
    turkicFront = 0x80,
    // A rounded vowel after which the two-way-or-four-way A stays
    // unrounded while I rounds (Kyrgyz у: кулдар, кулдун).
    turkicFlat = 0x100,
};

typedef struct TurkicLetter
{
    uint16_t codePoint;
    uint16_t info;
} TurkicLetter;

// An archiphoneme of consonants: the letter it is after each class of
// sound, back and front (0 for none: a buffer letter after a consonant).
typedef struct TurkicOnset
{
    char symbol;
    uint16_t forms[turkicClasses][2];
} TurkicOnset;

// How a spoken end sounds: whether it has a vowel, its last vowel's
// harmony and its final sound's class.
typedef struct TurkicSound
{
    bool vowel;
    bool back;
    bool round;
    // A rounded vowel that leaves A unrounded (turkicFlat).
    bool flat;
    uint8_t final;
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

// The case templates' variants: after a stem, after a third person's
// possessive (Turkish's pronominal n), after a first or second person's.
enum
{
    turkicAfterStem = 0,
    turkicAfterThird = 1,
    turkicAfterPossessive = 2,
    turkicVariants = 3,
};

// The cases a Turkic language may have, as template rows.
enum
{
    turkicNominative = 0,
    turkicGenitive = 1,
    turkicDative = 2,
    turkicAccusative = 3,
    turkicLocative = 4,
    turkicAblative = 5,
    turkicInstrumental = 6,
    turkicCases = 7,
};

// Where a language writes its boundary mark before suffixes.
enum
{
    // Names (Turkish: Ahmet'e).
    turkicMarkNames = 1,
    // A word whose last letter is outside the language's script.
    turkicMarkForeign = 2,
};

// A Turkic language. Templates are UTF-8: A is the two-way (or
// four-way) vowel, I the four-way (or two-way) one, V the I after a
// consonant only, an onset's symbol its consonant, anything else
// itself.
typedef struct TurkicLanguage
{
    // Letters sorted by code point; a letter not here is read by the
    // Latin table, and a letter neither has is a voiced consonant.
    const TurkicLetter* letters;
    uint16_t letterCount;
    // The script's code points, for the foreign-word mark.
    uint16_t scriptFirst;
    uint16_t scriptLast;
    // A and I by [front][round].
    uint16_t a[2][2];
    uint16_t i[2][2];
    const TurkicOnset* onsets;
    uint8_t onsetCount;
    // The boundary mark (' or -) and where it goes besides numbers and
    // abbreviations read by their letters.
    char mark;
    uint8_t markOn;
    // Whether digits and a period are an ordinal (Turkish 5.).
    bool periodOrdinals;
    // Whether a final y after a consonant is said as i (Harry).
    bool finalYAsI;
    // Whether common nouns are looked up in the lexicon of marks.
    bool lexicon;
    // Whether names change their stem as common nouns do (Kazakh
    // Айтбегі).
    bool stemNames;
    // Whether a plural's third person plural possessive is written once
    // (Turkish evleri).
    bool pluralInThirdPlural;
    const char* plural;
    const char* possessives[2][3];
    // Null for a case the language does not have.
    const char* cases[turkicCases][turkicVariants];
    // Null when the language has no copula suffix.
    const char* copula;
    const char* question;
    // The name of a letter as the language reads it, or null for one it
    // does not name.
    const char* (*letterName)(uint32_t codePoint);
    // A stem's changed end (UTF-8, valid) before the suffixes written for
    // it, with the lexicon's marks; false when it stays as it is. It may
    // change the suffixes' first letter where the two fuse (Kyrgyz той
    // and у: тоюм).
    bool (*stem)(const uint8_t* word, size_t length, uint8_t marks, TurkicSuffixes* suffixes,
                 TurkicStem* out);
} TurkicLanguage;

extern const TurkicLanguage mlocTurkish;
extern const TurkicLanguage mlocKazakh;
extern const TurkicLanguage mlocKyrgyz;

// A letter's information in a language.
uint16_t mlocTurkicLetterInfo(const TurkicLanguage* language, uint32_t codePoint);

// Decodes the code point that ends at text[end] (UTF-8, valid); its
// first byte's index in startOut.
uint32_t mlocTurkicPrevious(const uint8_t* text, size_t end, size_t* startOut);

// The capital of a Cyrillic letter (Russian's and the Turkic extensions'),
// any other code point as it is.
uint32_t mlocTurkicCyrillicCapital(uint32_t codePoint);

// Whether a request is one the language has.
bool mlocTurkicRequestValid(const TurkicLanguage* language, const mlocInflection* request);

// Reads the sound of a spoken word's end (UTF-8, valid), or false when
// it has no vowel; with inverse, its harmony is front (a word of
// inverse harmony: saat as saet).
bool mlocTurkicSound(const TurkicLanguage* language, const uint8_t* word, size_t length,
                     bool inverse, TurkicSound* out);

// Writes the suffixes a request asks for after a sound; endsThird for a
// word that already ends in a third person's possessive (a compound:
// zeytinyağı), whose cases then take the variant after it.
void mlocTurkicSuffixes(const TurkicLanguage* language, TurkicSound sound,
                        const mlocInflection* request, bool endsThird, TurkicSuffixes* out);

#endif
