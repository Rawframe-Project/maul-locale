// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Azerbaijani for the Turkic engine (grammar_turkic.h, record mloc-0047),
// in its Latin alphabet, as the orthography norms write it: two-way A (a
// or ə) and four-way I, no assimilation of d after a voiceless consonant,
// the buffer consonants y, n and s after a vowel, no instrumental case,
// the question particle joined; a hyphen after an abbreviation read by
// its letters, after digits ("BMT-yə", "2-yə") and after a name a hint
// says is foreign ("Microsoft-un", as Microsoft's style guide writes a
// product's name), a native name joined; a common noun's final k or q
// after a vowel, in a word of two syllables or more, written y or ğ
// before a vowel (çörəyi, ayağı).

#include "grammar_turkic.h"

#include <string.h>

// The letters the Latin table lacks: ə and its capital.
static const TurkicLetter s_letters[] = {
    {0x18F, turkicVowel},
    {0x259, turkicVowel},
};

static const TurkicOnset s_onsets[] = {
    // The buffers, after a vowel only.
    {'Y', {{'y', 'y'}}},
    {'N', {{'n', 'n'}}},
    {'S', {{'s', 's'}}},
};

// A letter's name: a vowel as itself, a consonant with e (be, ce, qe,
// xe).
static const char* AzerbaijaniLetterName(uint32_t c)
{
    static const char* const s_names[26] = {
        "a",  "be", "ce", "de", "e",  "fe", "ge", "he", "\xC4\xB1", "je", "ke", "le", "me",
        "ne", "o",  "pe", "qe", "re", "se", "te", "u",  "ve",       "",   "xe", "ye", "ze"};
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {
        const char* name = c == 'i' ? "i" : s_names[(c | 0x20) - 'a'];
        return name[0] != '\0' ? name : nullptr;
    }
    switch (c)
    {
    case 0xC7:
    case 0xE7:
        return "\xC3\xA7"
               "e";
    case 0x18F:
    case 0x259:
        return "\xC9\x99";
    case 0x11E:
    case 0x11F:
        return "\xC4\x9F"
               "e";
    case 0x130:
        return "i";
    case 0x131:
        return "\xC4\xB1";
    case 0xD6:
    case 0xF6:
        return "\xC3\xB6";
    case 0x15E:
    case 0x15F:
        return "\xC5\x9F"
               "e";
    case 0xDC:
    case 0xFC:
        return "\xC3\xBC";
    default:
        return nullptr;
    }
}

// A final k or q after a vowel, in a word of two syllables or more, is
// written y or ğ before a vowel. No dictionary of the words that keep
// it is free to use.
static bool AzerbaijaniStem(const uint8_t* word, size_t length, uint8_t marks,
                            TurkicSuffixes* suffixes, TurkicStem* out)
{
    (void)marks;
    if (!suffixes->vowelFirst)
    {
        return false;
    }
    *out = (TurkicStem){.kept = length};
    size_t finalStart = 0;
    uint32_t last = mlocTurkicPrevious(word, length, &finalStart);
    if ((last != 'k' && last != 'q') || finalStart == 0)
    {
        return false;
    }
    size_t vowels = 0;
    bool afterVowel = false;
    for (size_t end = finalStart; end > 0;)
    {
        size_t start = 0;
        uint16_t info =
            mlocTurkicLetterInfo(&mlocAzerbaijani, mlocTurkicPrevious(word, end, &start));
        bool vowel = (info & turkicClassMask) == turkicVowel;
        afterVowel = afterVowel || (vowel && end == finalStart);
        vowels += vowel ? 1 : 0;
        end = start;
    }
    if (!afterVowel || vowels < 2)
    {
        return false;
    }
    out->kept = finalStart;
    if (last == 'k')
    {
        out->tail[0] = 'y';
        out->tailLength = 1;
    }
    else
    {
        out->tail[0] = (char)0xC4;
        out->tail[1] = (char)0x9F;
        out->tailLength = 2;
    }
    return true;
}

const TurkicLanguage mlocAzerbaijani = {
    .letters = s_letters,
    .letterCount = sizeof(s_letters) / sizeof(s_letters[0]),
    .scriptFirst = 0,
    .scriptLast = 0x24F,
    .a = {{'a', 'a'}, {0x259, 0x259}},
    .i = {{0x131, 'u'}, {'i', 0xFC}},
    .onsets = s_onsets,
    .onsetCount = sizeof(s_onsets) / sizeof(s_onsets[0]),
    .mark = '-',
    .markOn = turkicMarkHinted,
    .periodOrdinals = false,
    .finalYAsI = false,
    .lexicon = false,
    .stemNames = false,
    .pluralInThirdPlural = true,
    .plural = "lAr",
    .possessives = {{"Vm", "Vn", "SI"}, {"VmIz", "VnIz", "lArI"}},
    .cases =
        {
            [turkicNominative] = {"", "", ""},
            [turkicGenitive] = {"NIn", "nIn", "NIn"},
            [turkicDative] = {"YA", "nA", "YA"},
            [turkicAccusative] = {"NI", "nI", "NI"},
            [turkicLocative] = {"dA", "ndA", "dA"},
            [turkicAblative] = {"dAn", "ndAn", "dAn"},
            [turkicInstrumental] = {nullptr, nullptr, nullptr},
        },
    .copula = "dIr",
    .question = "mI",
    .letterName = AzerbaijaniLetterName,
    .stem = AzerbaijaniStem,
};
