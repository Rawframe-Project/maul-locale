// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Kyrgyz for the Turkic engine (grammar_turkic.h, record mloc-0047), in
// its Cyrillic alphabet: four-way harmony for both vowels (A rounds after
// о and ө, not after у; I after each rounded vowel), a suffix's first
// consonant chosen by the stem's last sound (a vowel, a glide, another
// voiced consonant, a voiceless one), no instrumental case and no copula
// suffix, the question particle joined; no mark before suffixes, as the
// press writes them ("АКШнын", "350дөн"); a final п or к voiced before a
// vowel and a final ь dropped before any suffix (apertium-kir's model).

#include "grammar_turkic.h"

#include <string.h>

// Letters by code point (apertium-kir's readings): и is a front vowel;
// у and ю round I but not A.
static const TurkicLetter s_letters[] = {
    {0x401, turkicVowel | turkicBack | turkicRound},
    {0x410, turkicVowel | turkicBack},
    {0x411, turkicVoiced},
    {0x412, turkicVoiced},
    {0x413, turkicVoiced},
    {0x414, turkicVoiced},
    {0x415, turkicVowel},
    {0x416, turkicVoiced},
    {0x417, turkicVoiced},
    {0x418, turkicVowel},
    {0x419, turkicGlide},
    {0x41A, turkicVoiceless},
    {0x41B, turkicLiquid},
    {0x41C, turkicNasal},
    {0x41D, turkicNasal},
    {0x41E, turkicVowel | turkicBack | turkicRound},
    {0x41F, turkicVoiceless},
    {0x420, turkicGlide},
    {0x421, turkicVoiceless},
    {0x422, turkicVoiceless},
    {0x423, turkicVowel | turkicBack | turkicRound | turkicFlat},
    {0x424, turkicVoiceless},
    {0x425, turkicVoiceless},
    {0x426, turkicVoiceless},
    {0x427, turkicVoiceless},
    {0x428, turkicVoiceless},
    {0x429, turkicVoiceless},
    {0x42A, turkicSign | turkicVoiced},
    {0x42B, turkicVowel | turkicBack},
    {0x42C, turkicSign | turkicVoiced},
    {0x42D, turkicVowel},
    {0x42E, turkicVowel | turkicBack | turkicRound | turkicFlat},
    {0x42F, turkicVowel | turkicBack},
    {0x430, turkicVowel | turkicBack},
    {0x431, turkicVoiced},
    {0x432, turkicVoiced},
    {0x433, turkicVoiced},
    {0x434, turkicVoiced},
    {0x435, turkicVowel},
    {0x436, turkicVoiced},
    {0x437, turkicVoiced},
    {0x438, turkicVowel},
    {0x439, turkicGlide},
    {0x43A, turkicVoiceless},
    {0x43B, turkicLiquid},
    {0x43C, turkicNasal},
    {0x43D, turkicNasal},
    {0x43E, turkicVowel | turkicBack | turkicRound},
    {0x43F, turkicVoiceless},
    {0x440, turkicGlide},
    {0x441, turkicVoiceless},
    {0x442, turkicVoiceless},
    {0x443, turkicVowel | turkicBack | turkicRound | turkicFlat},
    {0x444, turkicVoiceless},
    {0x445, turkicVoiceless},
    {0x446, turkicVoiceless},
    {0x447, turkicVoiceless},
    {0x448, turkicVoiceless},
    {0x449, turkicVoiceless},
    {0x44A, turkicSign | turkicVoiced},
    {0x44B, turkicVowel | turkicBack},
    {0x44C, turkicSign | turkicVoiced},
    {0x44D, turkicVowel},
    {0x44E, turkicVowel | turkicBack | turkicRound | turkicFlat},
    {0x44F, turkicVowel | turkicBack},
    {0x451, turkicVowel | turkicBack | turkicRound},
    {0x4A2, turkicNasal},
    {0x4A3, turkicNasal},
    {0x4AE, turkicVowel | turkicRound},
    {0x4AF, turkicVowel | turkicRound},
    {0x4E8, turkicVowel | turkicRound},
    {0x4E9, turkicVowel | turkicRound},
};

// A letter's name, by its capital: the Russian-style names the alphabet
// reads (ка, эл, эр, ша), a vowel as itself.
typedef struct KyrgyzName
{
    uint16_t capital;
    const char* name;
} KyrgyzName;

static const KyrgyzName s_names[] = {
    {0x401, "\xD1\x91"},         {0x410, "\xD0\xB0"},         {0x411, "\xD0\xB1\xD1\x8D"},
    {0x412, "\xD0\xB2\xD1\x8D"}, {0x413, "\xD0\xB3\xD1\x8D"}, {0x414, "\xD0\xB4\xD1\x8D"},
    {0x415, "\xD0\xB5"},         {0x416, "\xD0\xB6\xD1\x8D"}, {0x417, "\xD0\xB7\xD1\x8D"},
    {0x418, "\xD0\xB8"},         {0x419, "\xD0\xB8\xD0\xB9"}, {0x41A, "\xD0\xBA\xD0\xB0"},
    {0x41B, "\xD1\x8D\xD0\xBB"}, {0x41C, "\xD1\x8D\xD0\xBC"}, {0x41D, "\xD1\x8D\xD0\xBD"},
    {0x41E, "\xD0\xBE"},         {0x41F, "\xD0\xBF\xD1\x8D"}, {0x420, "\xD1\x8D\xD1\x80"},
    {0x421, "\xD1\x8D\xD1\x81"}, {0x422, "\xD1\x82\xD1\x8D"}, {0x423, "\xD1\x83"},
    {0x424, "\xD1\x8D\xD1\x84"}, {0x425, "\xD1\x85\xD0\xB0"}, {0x426, "\xD1\x86\xD1\x8D"},
    {0x427, "\xD1\x87\xD1\x8D"}, {0x428, "\xD1\x88\xD0\xB0"}, {0x429, "\xD1\x89\xD0\xB0"},
    {0x42B, "\xD1\x8B"},         {0x42D, "\xD1\x8D"},         {0x42E, "\xD1\x8E"},
    {0x42F, "\xD1\x8F"},         {0x4A2, "\xD1\x8B\xD2\xA3"}, {0x4AE, "\xD2\xAF"},
    {0x4E8, "\xD3\xA9"},
};

static const TurkicOnset s_onsets[] = {
    // The plural: l after a vowel or glide, d after another voiced consonant, t after a voiceless
    // one.
    {'L',
     {{0x43B, 0x43B},
      {0x43B, 0x43B},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The genitive and accusative: n after a vowel.
    {'N',
     {{0x43D, 0x43D},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The dative.
    {'G',
     {{0x433, 0x433},
      {0x433, 0x433},
      {0x433, 0x433},
      {0x433, 0x433},
      {0x433, 0x433},
      {0x43A, 0x43A}}},
    // The dative after a possessive: none after its nasal.
    {'H', {{0x433, 0x433}, {0x433, 0x433}, {0x433, 0x433}, {0, 0}, {0x433, 0x433}, {0x43A, 0x43A}}},
    // The locative and ablative.
    {'D',
     {{0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The question particle.
    {'B',
     {{0x431, 0x431},
      {0x431, 0x431},
      {0x431, 0x431},
      {0x431, 0x431},
      {0x431, 0x431},
      {0x43F, 0x43F}}},
    // The third person's s, after a vowel only.
    {'S', {{0x441, 0x441}}},
};

static const char* KyrgyzLetterName(uint32_t c)
{
    uint32_t capital = mlocTurkicCyrillicCapital(c);
    for (size_t i = 0; i < sizeof(s_names) / sizeof(s_names[0]); i++)
    {
        if (s_names[i].capital == capital)
        {
            return s_names[i].name;
        }
    }
    return nullptr;
}

// A final ь or ъ is dropped before any suffix (княздын, лагерге); a final
// й fuses with a suffix's у (тоюм); a final п or к after a vowel is
// voiced before a vowel (китеби, Айбеги), in names too. No dictionary of
// the words that keep a stop is free to use.
static bool KyrgyzStem(const uint8_t* word, size_t length, uint8_t marks, TurkicSuffixes* suffixes,
                       TurkicStem* out)
{
    (void)marks;
    *out = (TurkicStem){.kept = length};
    size_t finalStart = 0;
    uint32_t last = mlocTurkicPrevious(word, length, &finalStart);
    if ((mlocTurkicLetterInfo(&mlocKyrgyz, last) & turkicSign) != 0)
    {
        out->kept = finalStart;
        return true;
    }
    // A final й and the suffix's у are written ю (тоюм, тоюна).
    uint8_t* first = (uint8_t*)suffixes->text;
    if (last == 0x439 && suffixes->particle >= 2 && first[0] == 0xD1 && first[1] == 0x83)
    {
        first[1] = 0x8E;
        out->kept = finalStart;
        return true;
    }
    uint32_t voiced = last == 0x43F ? 0x431 : last == 0x43A ? 0x433 : 0;
    if (!suffixes->vowelFirst || voiced == 0 || finalStart == 0)
    {
        return false;
    }
    size_t beforeStart = 0;
    uint16_t before =
        mlocTurkicLetterInfo(&mlocKyrgyz, mlocTurkicPrevious(word, finalStart, &beforeStart));
    if ((before & turkicClassMask) != turkicVowel)
    {
        return false;
    }
    out->kept = finalStart;
    out->tail[0] = (char)(0xC0 | voiced >> 6);
    out->tail[1] = (char)(0x80 | (voiced & 0x3F));
    out->tailLength = 2;
    return true;
}

const TurkicLanguage mlocKyrgyz = {
    .letters = s_letters,
    .letterCount = sizeof(s_letters) / sizeof(s_letters[0]),
    .scriptFirst = 0x400,
    .scriptLast = 0x52F,
    .a = {{0x430, 0x43E}, {0x435, 0x4E9}},
    .i = {{0x44B, 0x443}, {0x438, 0x4AF}},
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
    .pluralInThirdPlural = false,
    .plural = "LA\xD1\x80",
    .possessives = {{"V\xD0\xBC", "V\xD2\xA3", "SI"},
                    {"V\xD0\xB1I\xD0\xB7",
                     "V\xD2\xA3"
                     "A\xD1\x80",
                     "SI"}},
    .cases =
        {
            [turkicNominative] = {"", "", ""},
            [turkicGenitive] = {"NI\xD0\xBD", "\xD0\xBDI\xD0\xBD", "NI\xD0\xBD"},
            [turkicDative] = {"GA",
                              "\xD0\xBD"
                              "A",
                              "HA"},
            [turkicAccusative] = {"NI", "\xD0\xBD", "NI"},
            [turkicLocative] = {"DA",
                                "\xD0\xBD\xD0\xB4"
                                "A",
                                "DA"},
            [turkicAblative] = {"DA\xD0\xBD",
                                "\xD0\xBD"
                                "A\xD0\xBD",
                                "DA\xD0\xBD"},
            [turkicInstrumental] = {nullptr, nullptr, nullptr},
        },
    .copula = nullptr,
    .question = "BI",
    .letterName = KyrgyzLetterName,
    .numberWord = nullptr,
    .stem = KyrgyzStem,
};
