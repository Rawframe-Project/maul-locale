// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Finnish words as code points (record mloc-0049): reading, harmony,
// the patterns the types' rows write, consonant gradation.

#ifndef MLOC_GRAMMAR_FI_WORD_H
#define MLOC_GRAMMAR_FI_WORD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    finnishLetterRoom = 64,
};

// A word as code points.
typedef struct FinnishWord
{
    uint32_t c[finnishLetterRoom];
    size_t count;
} FinnishWord;

// The code point that ends text[0, end); startOut receives where it
// begins.
uint32_t mlocFinnishPrevious(const uint8_t* text, size_t end, size_t* startOut);

// A letter in lower case: ASCII and Latin-1's capitals; others as they
// are.
uint32_t mlocFinnishSmall(uint32_t c);

// Reads the letters after a text's last space or hyphen.
void mlocFinnishReadWord(const uint8_t* text, size_t length, FinnishWord* out);

bool mlocFinnishIsVowel(uint32_t c);

// The harmony by the last vowel that is not e or i: back for a, o, u
// (Tampereella, Hämeenlinnassa), front for ä, ö, y and for none
// (Malmössä, Kemissä).
bool mlocFinnishIsBack(const FinnishWord* w);

void mlocFinnishPush(FinnishWord* w, uint32_t c);

// Appends a pattern: letters as they are; A, O, U by harmony (a or ä, o
// or ö, u or y); V the last vowel again; L the original word's last
// letter.
void mlocFinnishAppend(FinnishWord* w, const char* pattern, bool back, uint32_t last);

void mlocFinnishGrade(FinnishWord* w, char letter, bool toWeak);

#endif
