// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// How a Hungarian word sounds (record mloc-0048): its last letters read
// as lowercase code points, its harmony, and its final sound, a vowel or
// a consonant with the modern spelling an old or foreign one says
// (Kossuth's th as t, Madách's ch as cs, Marx's x as ksz; AkH 12, 86,
// 216); the letters' names.

#ifndef MLOC_GRAMMAR_HU_SOUND_H
#define MLOC_GRAMMAR_HU_SOUND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    // The spoken end's code points read, at most.
    hungarianLetters = 48,
};

typedef enum HungarianHarmony
{
    hungarianHarmonyBack = 0,
    hungarianHarmonyFront = 1,
    hungarianHarmonyRound = 2,
} HungarianHarmony;

// A vowel's information.
enum
{
    hungarianVowelBack = 0x01,
    hungarianVowelRound = 0x02,
    // i, í, é: after a back vowel the word stays back (papírban).
    hungarianVowelNeutral = 0x04,
    // a, e, o, ö: lengthened before most suffixes.
    hungarianVowelShort = 0x08,
    hungarianVowelAny = 0x10,
};

// A word's last letters, lowercase, as code points, with each one's
// first byte in the word.
typedef struct HungarianLetters
{
    uint32_t c[hungarianLetters];
    size_t at[hungarianLetters];
    size_t count;
} HungarianLetters;

// How a word's end sounds.
typedef struct HungarianSound
{
    HungarianHarmony harmony;
    bool vowelEnd;
    // The final vowel, when it is one a suffix lengthens.
    uint32_t vowel;
    // The final consonant's sound in modern letters (sz for x and s in
    // Wells, cs for ch), what -val assimilates to.
    char sound[4];
    uint8_t soundLength;
    // The written end is the sound's modern spelling (a digraph then
    // doubles in its first letter), or an old or foreign one that stays.
    bool modern;
    // The written end is a long consonant (two same letters: toll, Papp,
    // gally) and its spelling's length in bytes.
    bool isLong;
    uint8_t writtenLength;
    // The accusative's t joins without a linking vowel (j, l, ly, n, ny,
    // r, s, sz, z, zs), after a long one only in a name (orrot, Bonnt).
    bool directT;
    // Two consonants end it (kert, bank): the third person's j.
    bool cluster;
    bool hasVowel;
    // Its vowels, counted up to 255.
    uint8_t vowels;
} HungarianSound;

// The long vowel a short one is lengthened to (a á, e é, o ó, ö ő), a
// capital to a capital; 0 for none.
uint32_t mlocHungarianLongVowel(uint32_t c);

// Decodes the code point that ends at text[end] (valid UTF-8); its first
// byte's index in startOut.
uint32_t mlocHungarianPrevious(const uint8_t* text, size_t end, size_t* startOut);

// Reads the letters that end a text (after its last hyphen or space).
void mlocHungarianReadLetters(const uint8_t* text, size_t length, HungarianLetters* out);

// Whether the letters before end are the UTF-8 string s.
bool mlocHungarianEndsWith(const HungarianLetters* w, size_t end, const char* s);

// The vowel information of the letter at i (a final y after a consonant
// but g, l, n, t read as i), or 0 for a consonant.
uint8_t mlocHungarianLetterVowel(const HungarianLetters* w, size_t i);

// The harmony by the last vowel that is not i, í or é. One of them after
// a back vowel leaves the word back (kávé, papír, Mali), two or more
// make it front (analízis, Euklidész), as the oracle counts them; the
// last vowel's rounding is read only for a front word.
HungarianHarmony mlocHungarianReadHarmony(const HungarianLetters* w);

// Reads how letters sound: their harmony, their vowels, and their end.
void mlocHungarianReadSound(const HungarianLetters* w, HungarianSound* s);

// The name of the letter that ends text[0, end), a digraph (sz, cs, gy,
// ny, ty, ly, zs, dz, dzs) read as one; null for none.
const char* mlocHungarianLetterName(const uint8_t* text, size_t end);

#endif
