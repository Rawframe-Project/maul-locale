// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Turkmen for the Turkic engine (grammar_turkic.h), in its Latin alphabet,
// as apertium-tuk generates it: two-way harmony (a or e; y or i), a high
// vowel rounded only as the suffixes' first vowel after the stem's last,
// and only by some suffixes (the possessives after o, ö, u, ü: ýolum,
// gözüm, direktorum; the genitive and third person's possessive after ö
// and ü: gözüň, görüşüň, gözü; the accusative never: gözi), no
// assimilation of d, the buffers n and s after a vowel, the dative fused
// with a final vowel (ene, enä; okuwçy, okuwça), no instrumental case, the
// copula and the question particle joined; a hyphen after an abbreviation
// read by its letters and after digits, as the Turkmen press writes them
// (BMG-niň, AFK-nyň, 1700-den), a name joined; a final p, ç or k after a
// vowel in a word of two syllables or more voiced before a vowel (kitaby,
// agajy, ýüregi, Garaşsyzlygy), a final t only in -abat (Aşgabada; the
// loans in t keep it: döwleti, gazeti). Numbers are read by the coded
// words below, Turkmen having no spell-out rules in CLDR.

#include "grammar_turkic.h"

#include <string.h>

// Letters by code point: y is a back vowel and I the capital of i; ý is
// a consonant.
static const TurkicLetter s_letters[] = {
    {'I', turkicVowel},
    {'Y', turkicVowel | turkicBack},
    {'y', turkicVowel | turkicBack},
};

static const TurkicOnset s_onsets[] = {
    // The buffers, after a vowel only.
    {'N', {{'n', 'n'}}},
    {'S', {{'s', 's'}}},
};

static const TurkicVowel s_vowels[] = {
    // The possessives' vowel, rounded after any rounded vowel.
    {'U', false, {{'y', 'u'}, {'i', 0xFC}}, {0, 0}},
    // The same, after a consonant only.
    {'W', true, {{'y', 'u'}, {'i', 0xFC}}, {0, 0}},
    // The genitive's and third person's vowel, rounded after ö and ü.
    {'O', false, {{'y', 'y'}, {'i', 0xFC}}, {0, 0}},
    // The dative's a or e, fused with a final vowel as a or ä.
    {'E', false, {{'a', 'a'}, {'e', 'e'}}, {'a', 0xE4}},
};

// A letter's name: a vowel as itself, a consonant as the alphabet reads
// it (ge, ka, el, že); C, Q, V and X are no Turkmen letters.
static const char* TurkmenLetterName(uint32_t c)
{
    static const char* const s_names[26] = {
        "a",  "be", "",   "de", "e",  "fe", "ge", "he", "i", "\xC5\xBE\x65", "ka", "el", "em",
        "en", "o",  "pe", "",   "er", "es", "te", "u",  "",  "we",           "",   "y",  "ze"};
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
    {
        const char* name = s_names[(c | 0x20) - 'a'];
        return name[0] != '\0' ? name : nullptr;
    }
    switch (c)
    {
    case 0xC4:
    case 0xE4:
        return "\xC3\xA4";
    case 0xC7:
    case 0xE7:
        return "\xC3\xA7"
               "e";
    case 0xD6:
    case 0xF6:
        return "\xC3\xB6";
    case 0xDC:
    case 0xFC:
        return "\xC3\xBC";
    case 0xDD:
    case 0xFD:
        return "\xC3\xBD"
               "e";
    case 0x147:
    case 0x148:
        return "\xC5\x88"
               "e";
    case 0x15E:
    case 0x15F:
        return "\xC5\x9F"
               "e";
    case 0x17D:
    case 0x17E:
        return "\xC5\xBE"
               "e";
    default:
        return nullptr;
    }
}

// The number's last word: its lowest nonzero digit's unit or ten, ýüz
// for a hundred, the scale's word for a thousand and up.
static const char* TurkmenNumberWord(uint8_t digit, int32_t power)
{
    static const char* const s_units[10] = {"nol",         "bir",
                                            "iki",         "\xC3\xBC\xC3\xA7",
                                            "d\xC3\xB6rt", "b\xC3\xA4\xC5\x9F",
                                            "alty",        "\xC3\xBD\x65\x64i",
                                            "sekiz",       "dokuz"};
    static const char* const s_tens[10] = {
        "",       "on",    "\xC3\xBDigrimi", "otuz",
        "kyrk",   "elli",  "altmy\xC5\x9F",  "\xC3\xBD\x65tmi\xC5\x9F",
        "segsen", "togsan"};
    static const char* const s_scales[] = {"m\xC3\xBC\xC5\x88", "million",     "milliard",
                                           "trillion",          "kwadrillion", "kwintillion"};
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
        return "\xC3\xBD\xC3\xBCz";
    }
    size_t scale = (size_t)power / 3 - 1;
    return scale < sizeof(s_scales) / sizeof(s_scales[0]) ? s_scales[scale] : nullptr;
}

// The voiced form of a final stop before a vowel: b, j, g, and d for the
// t of -abat (Aşgabat, Türkmenabat).
static char Voiced(const uint8_t* word, size_t length, uint32_t last)
{
    bool abat = length >= 4 && memcmp(word + length - 4, "abat", 4) == 0;
    return last == 'p' ? 'b' : last == 0xE7 ? 'j' : last == 'k' ? 'g' : abat ? 'd' : 0;
}

// Before a suffix that begins with a vowel: a final vowel gives way to
// the dative's (ene and ä: enä), and a final p, ç or k after a vowel in
// a word of two syllables or more, or the t of -abat, is voiced
// (kitaby, Aşgabada), in names too. No dictionary of the words that
// keep a stop (tarapy) is free to use.
static bool TurkmenStem(const uint8_t* word, size_t length, uint8_t marks, TurkicSuffixes* suffixes,
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
    if ((mlocTurkicLetterInfo(&mlocTurkmen, last) & turkicClassMask) == turkicVowel)
    {
        out->kept = finalStart;
        return true;
    }
    char voiced = Voiced(word, length, last);
    size_t beforeStart = 0;
    uint16_t before =
        finalStart > 0
            ? mlocTurkicLetterInfo(&mlocTurkmen, mlocTurkicPrevious(word, finalStart, &beforeStart))
            : turkicVoiced;
    size_t vowels = 0;
    for (size_t end = finalStart; end > 0;)
    {
        size_t start = 0;
        uint16_t info = mlocTurkicLetterInfo(&mlocTurkmen, mlocTurkicPrevious(word, end, &start));
        vowels += (info & turkicClassMask) == turkicVowel ? 1 : 0;
        end = start;
    }
    if (voiced == 0 || (before & turkicClassMask) != turkicVowel || vowels < 2)
    {
        return false;
    }
    out->kept = finalStart;
    out->tail[0] = voiced;
    out->tailLength = 1;
    return true;
}

const TurkicLanguage mlocTurkmen = {
    .letters = s_letters,
    .letterCount = sizeof(s_letters) / sizeof(s_letters[0]),
    .scriptFirst = 0,
    .scriptLast = 0x24F,
    .a = {{'a', 'a'}, {'e', 'e'}},
    .i = {{'y', 'y'}, {'i', 'i'}},
    .onsets = s_onsets,
    .onsetCount = sizeof(s_onsets) / sizeof(s_onsets[0]),
    .vowels = s_vowels,
    .vowelCount = sizeof(s_vowels) / sizeof(s_vowels[0]),
    .roundsFirstOnly = true,
    .mark = '-',
    .markOn = 0,
    .periodOrdinals = false,
    .finalYAsI = false,
    .lexicon = false,
    .stemNames = true,
    .pluralInThirdPlural = false,
    .plural = "lAr",
    .possessives = {{"Wm", "W\xC5\x88", "SO"}, {"WmIz", "W\xC5\x88Iz", "SO"}},
    .cases =
        {
            [turkicNominative] = {"", "", ""},
            [turkicGenitive] = {"NO\xC5\x88", "nO\xC5\x88", "NO\xC5\x88"},
            [turkicDative] = {"E", "nA", "E"},
            [turkicAccusative] = {"NI", "nI", "NI"},
            [turkicLocative] = {"dA", "ndA", "dA"},
            [turkicAblative] = {"dAn", "ndAn", "dAn"},
            [turkicInstrumental] = {nullptr, nullptr, nullptr},
        },
    .copula = "dIr",
    .question = "mI",
    .letterName = TurkmenLetterName,
    .numberWord = TurkmenNumberWord,
    .stem = TurkmenStem,
};
