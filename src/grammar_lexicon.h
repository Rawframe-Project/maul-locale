// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Turkish words whose suffixes the rules alone would get wrong
// (record mloc-0046), from Zemberek-NLP's dictionaries.

#ifndef MLOC_GRAMMAR_LEXICON_H
#define MLOC_GRAMMAR_LEXICON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A word's marks, as bits.
enum
{
    // A word that keeps the final stop the rule softens (ahlak, tank).
    turkicKeeps = 1,
    // A word that softens a final stop the rule keeps (kalp: kalbi).
    turkicSoftens = 2,
    // Front suffixes after a back last vowel (rol: rolü).
    turkicInverse = 4,
    // The final consonant doubled before a vowel (hak: hakkı).
    turkicDoubles = 8,
    // The last vowel dropped before a vowel (burun: burnu).
    turkicDropsVowel = 16,
    // A compound ending in a third person's possessive (zeytinyağı).
    turkicCompound = 32,
};

typedef struct TurkicMarks
{
    uint8_t marks;
    // A compound's possessive, in bytes at the word's end.
    uint8_t possessive;
} TurkicMarks;

// Looks a word up (UTF-8, valid; read in Turkish lowercase without the
// circumflex); false, with no marks, for a word the table does not hold.
bool mlocTurkicMarks(const uint8_t* word, size_t length, TurkicMarks* out);

#endif
