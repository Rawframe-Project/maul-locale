// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Finnish suffixes, as the Kielitoimisto guidance writes them: a word's
// inflection type and consonant gradation (Kotus's types 1 to 49, from the
// word list's table or the ending), its stems, the case endings by
// harmony, the possessive suffixes and the question clitic; a colon before
// the ending of digits and capitals, an apostrophe after a name's unsaid
// end.

#ifndef MLOC_GRAMMAR_FI_H
#define MLOC_GRAMMAR_FI_H

#include "maul-locale/grammar.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum
{
    finnishTailRoom = 24,
    finnishSuffixRoom = 40,
};

// A text's end as the inflector has read it.
typedef struct FinnishText
{
    // The written word: the text's last letters.
    const uint8_t* word;
    size_t wordLength;
    // How it is said: the word, the hint's spelling, a number's spoken
    // last word or a letter's name.
    const uint8_t* spoken;
    size_t spokenLength;
    // Digits, capitals, a period or a closing mark: a colon and the
    // ending as said (EU:ssa, 5:een).
    bool marked;
    // A number, whose spoken form is read by the numerals' table.
    bool number;
    bool ordinal;
    bool name;
    bool hinted;
} FinnishText;

// What a request writes: the word's bytes kept, its changed end, the
// mark (':', '\'' or 0), the endings.
typedef struct FinnishForm
{
    size_t kept;
    char tail[finnishTailRoom];
    uint8_t tailLength;
    char mark;
    char suffixes[finnishSuffixRoom];
    uint8_t length;
} FinnishForm;

// Whether Finnish has what a request asks: one of its 14 cases, a
// possessor of person 1 to 3, the question clitic; no copula.
bool mlocFinnishRequestValid(const mlocInflection* request);

// The name of the letter that ends text[0, end), as Kielitoimisto reads
// it (aa, bee, äf); null for none.
const char* mlocFinnishLetterName(const uint8_t* text, size_t end);

// Writes a request's endings on a text's end; false when there is
// nothing to read or a marked text is asked a plural or a possessor.
bool mlocFinnishInflect(const FinnishText* text, const mlocInflection* request, FinnishForm* out);

#endif
