// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Uzbek for the Turkic engine (grammar_turkic.h, record mloc-0047), in
// its Latin alphabet, by the 1995 spelling rules (Cabinet decree 339):
// no vowel harmony; -ga written -ka after k and -qa after q (item 37);
// no pronominal n after a third person's possessive (uyiga); no
// instrumental case; the copula -dir and the question particle -mi
// joined; abbreviations and their suffixes joined (item 49: ToshDUning)
// and digits joined too, a hyphen after digits being the ordinal's
// (item 56: 7-sinf); a common noun's final k or q in a word of two
// syllables or more (and bek, yoʻq) written g or gʻ before a possessive
// (item 34: yuragim), in names too (Ulugʻbegi, as apertium-uzb writes). Numbers are read by the
// coded words below, Uzbek having no spell-out rules in CLDR.

#include "grammar_turkic.h"

#include <string.h>

// Letters by code point: k takes -ka and q -qa; every other consonant,
// voiceless or not, -ga, so the Latin table's voiceless letters are
// voiced here; the modifier letters ʻ and ʼ of oʻ, gʻ and the tutuq
// belgisi are signs.
static const TurkicLetter s_letters[] = {
    {'F', turkicVoiced},
    {'H', turkicVoiced},
    {'K', turkicVoiceless},
    {'P', turkicVoiced},
    {'Q', turkicUvular},
    {'S', turkicVoiced},
    {'T', turkicVoiced},
    {'X', turkicVoiced},
    {'f', turkicVoiced},
    {'h', turkicVoiced},
    {'k', turkicVoiceless},
    {'p', turkicVoiced},
    {'q', turkicUvular},
    {'s', turkicVoiced},
    {'t', turkicVoiced},
    {'x', turkicVoiced},
    {0x2BB, turkicSign | turkicVoiced},
    {0x2BC, turkicSign | turkicVoiced},
};

static const TurkicOnset s_onsets[] = {
    // The dative: k after k, q after q, g otherwise.
    {'G', {{'g', 'g'}, {'g', 'g'}, {'g', 'g'}, {'g', 'g'}, {'g', 'g'}, {'k', 'k'}, {'q', 'q'}}},
    // The third person's s, after a vowel only.
    {'S', {{'s', 's'}}},
};

// A letter's name: a vowel as itself, a consonant as the alphabet reads
// it (be, el, qe, xe); C and W are no Uzbek letters.
static const char* UzbekLetterName(uint32_t c)
{
    static const char* const s_names[26] = {"a",  "be", "",   "de", "e",  "ef", "ge", "ha", "i",
                                            "je", "ke", "el", "em", "en", "o",  "pe", "qe", "er",
                                            "es", "te", "u",  "ve", "",   "xe", "ye", "ze"};
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {
        const char* name = s_names[(c | 0x20) - 'a'];
        return name[0] != '\0' ? name : nullptr;
    }
    return nullptr;
}

// The number's last word: its lowest nonzero digit's unit or ten, yuz
// for a hundred, the scale's word for a thousand and up (5 000 is besh
// ming, 50 000 ellik ming).
static const char* UzbekNumberWord(uint8_t digit, int32_t power)
{
    static const char* const s_units[10] = {"nol",  "bir",  "ikki",  "uch",    "to\xCA\xBBrt",
                                            "besh", "olti", "yetti", "sakkiz", "to\xCA\xBBqqiz"};
    static const char* const s_tens[10] = {"",       "o\xCA\xBBn",    "yigirma", "o\xCA\xBBttiz",
                                           "qirq",   "ellik",         "oltmish", "yetmish",
                                           "sakson", "to\xCA\xBBqson"};
    static const char* const s_scales[] = {
        "ming",        "million",    "milliard",  "trillion",  "kvadrillion", "kvintillion",
        "sekstillion", "septillion", "oktillion", "nonillion", "detsillion"};
    if (digit > 9 || power < 0)
    {
        return nullptr;
    }
    if (power == 0)
    {
        return s_units[digit];
    }
    if (power == 1)
    {
        return s_tens[digit];
    }
    if (power == 2)
    {
        return "yuz";
    }
    size_t scale = (size_t)power / 3 - 1;
    return scale < sizeof(s_scales) / sizeof(s_scales[0]) ? s_scales[scale] : nullptr;
}

// Whether a word has two syllables or more.
static bool Polysyllabic(const uint8_t* word, size_t end)
{
    size_t vowels = 0;
    while (end > 0)
    {
        size_t start = 0;
        uint16_t info = mlocTurkicLetterInfo(&mlocUzbek, mlocTurkicPrevious(word, end, &start));
        vowels += (info & turkicClassMask) == turkicVowel ? 1 : 0;
        end = start;
    }
    return vowels >= 2;
}

// A final k or q after a vowel in a word of two syllables or more, and
// in bek and yoʻq, written g or gʻ before a possessive (the vowel-first
// suffixes); after a consonant it stays (xalqi, obeliski).
// No dictionary of the loans that keep it (ishtiroki) is free to use.
static bool UzbekStem(const uint8_t* word, size_t length, uint8_t marks, TurkicSuffixes* suffixes,
                      TurkicStem* out)
{
    (void)marks;
    if (!suffixes->vowelFirst)
    {
        return false;
    }
    *out = (TurkicStem){.kept = length};
    size_t finalStart = 0;
    uint32_t last = mlocTurkicPrevious(word, length, &finalStart);
    if (last != 'k' && last != 'q')
    {
        return false;
    }
    bool named = (length == 3 && memcmp(word, "bek", 3) == 0) ||
                 (length == 5 && memcmp(word, "yo\xCA\xBBq", 5) == 0);
    // The letter before, past the ʻ of oʻ.
    size_t beforeStart = finalStart;
    uint16_t before = turkicVoiced | turkicSign;
    while ((before & turkicSign) != 0 && beforeStart > 0)
    {
        before =
            mlocTurkicLetterInfo(&mlocUzbek, mlocTurkicPrevious(word, beforeStart, &beforeStart));
    }
    if (!named && ((before & turkicClassMask) != turkicVowel || !Polysyllabic(word, finalStart)))
    {
        return false;
    }
    out->kept = finalStart;
    out->tail[0] = 'g';
    out->tailLength = 1;
    if (last == 'q')
    {
        out->tail[1] = (char)0xCA;
        out->tail[2] = (char)0xBB;
        out->tailLength = 3;
    }
    return true;
}

const TurkicLanguage mlocUzbek = {
    .letters = s_letters,
    .letterCount = sizeof(s_letters) / sizeof(s_letters[0]),
    .scriptFirst = 0,
    .scriptLast = 0x2BC,
    .a = {{'a', 'a'}, {'a', 'a'}},
    .i = {{'i', 'i'}, {'i', 'i'}},
    .onsets = s_onsets,
    .onsetCount = sizeof(s_onsets) / sizeof(s_onsets[0]),
    .vowels = nullptr,
    .vowelCount = 0,
    .roundsFirstOnly = false,
    .mark = 0,
    .markOn = 0,
    .periodOrdinals = false,
    .finalYAsI = false,
    .lexicon = false,
    .stemNames = true,
    .pluralInThirdPlural = true,
    .plural = "lar",
    .possessives = {{"Vm", "Vng", "SI"}, {"Vmiz", "Vngiz", "lari"}},
    .cases =
        {
            [turkicNominative] = {"", "", ""},
            [turkicGenitive] = {"ning", "ning", "ning"},
            [turkicDative] = {"Ga", "Ga", "Ga"},
            [turkicAccusative] = {"ni", "ni", "ni"},
            [turkicLocative] = {"da", "da", "da"},
            [turkicAblative] = {"dan", "dan", "dan"},
            [turkicInstrumental] = {nullptr, nullptr, nullptr},
        },
    .copula = "dir",
    .question = "mi",
    .letterName = UzbekLetterName,
    .numberWord = UzbekNumberWord,
    .stem = UzbekStem,
};
