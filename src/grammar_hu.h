// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Hungarian suffixes (record mloc-0048), by the Academy's spelling rules
// (AkH, 12th edition): the suffixes a request asks for, built on how a
// word is said (harmony, lengthening, linking vowels, the v of -val and
// -vá assimilated), the stem a joined word takes before them, and the
// hyphen where the rules ask for one.

#ifndef MLOC_GRAMMAR_HU_H
#define MLOC_GRAMMAR_HU_H

#include "maul-locale/grammar.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    // The most bytes a request's suffixes take.
    hungarianSuffixRoom = 40,
    hungarianTailRoom = 16,
};

// A text's end as the inflector has read it.
typedef struct HungarianText
{
    // The written word: the text's last letters, what a joined suffix
    // changes.
    const uint8_t* word;
    size_t wordLength;
    // How it is said: the word itself, the hint's spelling, a number's
    // last word or a letter's name, in Hungarian letters.
    const uint8_t* spoken;
    size_t spokenLength;
    // A hyphen goes before the suffixes whatever the word: after digits,
    // capitals, a period or a closing mark (AkH 282, 288, 292).
    bool marked;
    // A name, whose double final letter keeps a hyphen before a suffix
    // that begins with it (Papp-pal; AkH 160, 217); a common noun's does
    // not (tollal, AkH 93).
    bool name;
    // The spoken form is the hint's, which may say the word's last
    // letters are not said (Rousseau-val; AkH 217).
    bool hinted;
} HungarianText;

// What a request writes: the word's bytes kept, then its changed end,
// then the suffixes, with a hyphen before them or not.
typedef struct HungarianForm
{
    size_t kept;
    char tail[hungarianTailRoom];
    uint8_t tailLength;
    bool hyphen;
    char suffixes[hungarianSuffixRoom];
    uint8_t length;
} HungarianForm;

// Whether Hungarian has what a request asks: one of its 17 cases, a
// possessor of person 1 to 3; no copula and no question suffix.
bool mlocHungarianRequestValid(const mlocInflection* request);

// Writes a request's suffixes on a text's end; false when the spoken
// form has nothing to read (no letter).
bool mlocHungarianInflect(const HungarianText* text, const mlocInflection* request,
                          HungarianForm* out);

// The stems the rules cannot tell: a lowering stem (házat), one whose
// harmony its vowels do not show (hídon), one whose accusative takes a
// linking vowel (mézet), and one whose stem changes before a linking
// vowel (víz, vizet; bokor, bokrot; ló, lovat), which also changes it
// in the superessive (bokron, lovon; but vízen) or not in the third
// person's possessive, which is -ja on the word as it is (útja).
enum
{
    hungarianLowering = 0x01,
    hungarianBack = 0x02,
    hungarianLinking = 0x04,
    hungarianSuperessive = 0x08,
    hungarianOwnJ = 0x10,
};

typedef struct HungarianWord
{
    const char* key;
    // The stem before a linking vowel, or null for the key itself.
    const char* stem;
    uint8_t flags;
} HungarianWord;

// The table's entry for a lowercase word, or null.
const HungarianWord* mlocHungarianFindWord(const uint8_t* word, size_t length);

#endif
