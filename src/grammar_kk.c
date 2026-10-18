// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Kazakh for the Turkic engine (grammar_turkic.h, record mloc-0047), in
// its Cyrillic alphabet: two-way harmony for both vowels (a or e, y or
// i), a suffix's first consonant chosen by the stem's last sound (a
// vowel, a glide, l, a nasal, z or zh, a voiceless consonant), the n
// forms after a possessive, no copula suffix; a hyphen after an
// abbreviation read by its letters, digits and a word in another
// script, a name joined; a common noun's final p, k, q voiced before a
// vowel.

#include "grammar_turkic.h"

#include <string.h>

// Letters by code point (apertium-kaz's readings): и, у and ю are glides
// whose harmony a word takes only when it has no other vowel; б, в, г, ғ, д and
// ж end a word as voiceless; a final к or г makes the suffixes front
// (паркке, ботаникке); ь makes them front.
static const TurkicLetter s_letters[] = {
    {0x401, turkicVowel | turkicBack | turkicRound},
    {0x406, turkicVowel},
    {0x410, turkicVowel | turkicBack},
    {0x411, turkicVoiceless},
    {0x412, turkicVoiceless},
    {0x413, turkicVoiceless | turkicFront},
    {0x414, turkicVoiceless},
    {0x415, turkicVowel},
    {0x416, turkicVoiceless},
    {0x417, turkicVoiced},
    {0x418, turkicGlide | turkicWeak},
    {0x419, turkicGlide},
    {0x41A, turkicVoiceless | turkicFront},
    {0x41B, turkicLiquid},
    {0x41C, turkicNasal},
    {0x41D, turkicNasal},
    {0x41E, turkicVowel | turkicBack | turkicRound},
    {0x41F, turkicVoiceless},
    {0x420, turkicGlide},
    {0x421, turkicVoiceless},
    {0x422, turkicVoiceless},
    {0x423, turkicGlide | turkicWeak | turkicBack | turkicRound},
    {0x424, turkicVoiceless},
    {0x425, turkicVoiceless},
    {0x426, turkicVoiceless},
    {0x427, turkicVoiceless},
    {0x428, turkicVoiceless},
    {0x429, turkicVoiceless},
    {0x42A, turkicSign | turkicVoiced},
    {0x42B, turkicVowel | turkicBack},
    {0x42C, turkicSign | turkicFront | turkicVoiced},
    {0x42D, turkicVowel},
    {0x42E, turkicGlide | turkicWeak | turkicRound},
    {0x42F, turkicVowel | turkicBack},
    {0x430, turkicVowel | turkicBack},
    {0x431, turkicVoiceless},
    {0x432, turkicVoiceless},
    {0x433, turkicVoiceless | turkicFront},
    {0x434, turkicVoiceless},
    {0x435, turkicVowel},
    {0x436, turkicVoiceless},
    {0x437, turkicVoiced},
    {0x438, turkicGlide | turkicWeak},
    {0x439, turkicGlide},
    {0x43A, turkicVoiceless | turkicFront},
    {0x43B, turkicLiquid},
    {0x43C, turkicNasal},
    {0x43D, turkicNasal},
    {0x43E, turkicVowel | turkicBack | turkicRound},
    {0x43F, turkicVoiceless},
    {0x440, turkicGlide},
    {0x441, turkicVoiceless},
    {0x442, turkicVoiceless},
    {0x443, turkicGlide | turkicWeak | turkicBack | turkicRound},
    {0x444, turkicVoiceless},
    {0x445, turkicVoiceless},
    {0x446, turkicVoiceless},
    {0x447, turkicVoiceless},
    {0x448, turkicVoiceless},
    {0x449, turkicVoiceless},
    {0x44A, turkicSign | turkicVoiced},
    {0x44B, turkicVowel | turkicBack},
    {0x44C, turkicSign | turkicFront | turkicVoiced},
    {0x44D, turkicVowel},
    {0x44E, turkicGlide | turkicWeak | turkicRound},
    {0x44F, turkicVowel | turkicBack},
    {0x451, turkicVowel | turkicBack | turkicRound},
    {0x456, turkicVowel},
    {0x492, turkicVoiceless},
    {0x493, turkicVoiceless},
    {0x49A, turkicVoiceless},
    {0x49B, turkicVoiceless},
    {0x4A2, turkicNasal},
    {0x4A3, turkicNasal},
    {0x4AE, turkicVowel | turkicRound},
    {0x4AF, turkicVowel | turkicRound},
    {0x4B0, turkicVowel | turkicBack | turkicRound},
    {0x4B1, turkicVowel | turkicBack | turkicRound},
    {0x4BA, turkicVoiceless},
    {0x4BB, turkicVoiceless},
    {0x4D8, turkicVowel},
    {0x4D9, turkicVowel},
    {0x4E8, turkicVowel | turkicRound},
    {0x4E9, turkicVowel | turkicRound},
};

// A letter's name, by its capital: a consonant with ы, к and г with і,
// a vowel as itself (the abbreviation guidance of resmihat.kz).
typedef struct KazakhName
{
    uint16_t capital;
    const char* name;
} KazakhName;

static const KazakhName s_names[] = {
    {0x401, "\xD1\x91"},         {0x406, "\xD1\x96"},         {0x410, "\xD0\xB0"},
    {0x411, "\xD0\xB1\xD1\x8B"}, {0x412, "\xD0\xB2\xD1\x8B"}, {0x413, "\xD0\xB3\xD1\x96"},
    {0x414, "\xD0\xB4\xD1\x8B"}, {0x415, "\xD0\xB5"},         {0x416, "\xD0\xB6\xD1\x8B"},
    {0x417, "\xD0\xB7\xD1\x8B"}, {0x418, "\xD0\xB8"},         {0x41A, "\xD0\xBA\xD1\x96"},
    {0x41B, "\xD0\xBB\xD1\x8B"}, {0x41C, "\xD0\xBC\xD1\x8B"}, {0x41D, "\xD0\xBD\xD1\x8B"},
    {0x41E, "\xD0\xBE"},         {0x41F, "\xD0\xBF\xD1\x8B"}, {0x420, "\xD1\x80\xD1\x8B"},
    {0x421, "\xD1\x81\xD1\x8B"}, {0x422, "\xD1\x82\xD1\x8B"}, {0x423, "\xD1\x83"},
    {0x424, "\xD1\x84\xD1\x8B"}, {0x425, "\xD1\x85\xD1\x8B"}, {0x426, "\xD1\x86\xD1\x8B"},
    {0x427, "\xD1\x87\xD1\x8B"}, {0x428, "\xD1\x88\xD1\x8B"}, {0x429, "\xD1\x89\xD1\x8B"},
    {0x42B, "\xD1\x8B"},         {0x42D, "\xD1\x8D"},         {0x42E, "\xD1\x8E"},
    {0x42F, "\xD1\x8F"},         {0x492, "\xD2\x93\xD1\x8B"}, {0x49A, "\xD2\x9B\xD1\x8B"},
    {0x4A2, "\xD1\x8B\xD2\xA3"}, {0x4AE, "\xD2\xAF"},         {0x4B0, "\xD2\xB1"},
    {0x4BA, "\xD2\xBB\xD1\x8B"}, {0x4D8, "\xD3\x99"},         {0x4E8, "\xD3\xA9"},
};

static const TurkicOnset s_onsets[] = {
    // The plural: l after a vowel or glide, d after l, a nasal or z, zh; t after a voiceless
    // consonant.
    {'L',
     {{0x43B, 0x43B},
      {0x43B, 0x43B},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The accusative: n after a vowel.
    {'N',
     {{0x43D, 0x43D},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The genitive: n after a vowel or a nasal.
    {'P',
     {{0x43D, 0x43D},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x43D, 0x43D},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The dative.
    {'G',
     {{0x493, 0x433},
      {0x493, 0x433},
      {0x493, 0x433},
      {0x493, 0x433},
      {0x493, 0x433},
      {0x49B, 0x43A}}},
    // The dative after a possessive: none after its nasal.
    {'H', {{0x493, 0x433}, {0x493, 0x433}, {0x493, 0x433}, {0, 0}, {0x493, 0x433}, {0x49B, 0x43A}}},
    // The locative.
    {'D',
     {{0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The ablative: n after a nasal.
    {'B',
     {{0x434, 0x434},
      {0x434, 0x434},
      {0x434, 0x434},
      {0x43D, 0x43D},
      {0x434, 0x434},
      {0x442, 0x442}}},
    // The instrumental.
    {'M',
     {{0x43C, 0x43C},
      {0x43C, 0x43C},
      {0x43C, 0x43C},
      {0x43C, 0x43C},
      {0x431, 0x431},
      {0x43F, 0x43F}}},
    // The question particle.
    {'Q',
     {{0x43C, 0x43C},
      {0x43C, 0x43C},
      {0x43C, 0x43C},
      {0x431, 0x431},
      {0x431, 0x431},
      {0x43F, 0x43F}}},
    // The third person's s, after a vowel only.
    {'S', {{0x441, 0x441}}},
};

static const char* KazakhLetterName(uint32_t c)
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

// A final п, к or қ after a vowel is voiced before a vowel: б, г, ғ
// (кітабы, жүрегі, тарағы, акведугі), in names too (Айтбегі); a final ь
// is dropped (автомобилі, Абелі). No dictionary of the words that keep a
// stop is free to use, so every word follows the rule.
static bool KazakhStem(const uint8_t* word, size_t length, uint8_t marks, TurkicSuffixes* suffixes,
                       TurkicStem* out)
{
    if (!suffixes->vowelFirst)
    {
        return false;
    }
    (void)marks;
    *out = (TurkicStem){.kept = length};
    size_t finalStart = 0;
    uint32_t last = mlocTurkicPrevious(word, length, &finalStart);
    if (last == 0x44C || last == 0x42C)
    {
        out->kept = finalStart;
        return true;
    }
    uint32_t voiced = last == 0x43F ? 0x431 : last == 0x43A ? 0x433 : last == 0x49B ? 0x493 : 0;
    if (voiced == 0 || finalStart == 0)
    {
        return false;
    }
    size_t beforeStart = 0;
    uint16_t before =
        mlocTurkicLetterInfo(&mlocKazakh, mlocTurkicPrevious(word, finalStart, &beforeStart));
    if ((before & turkicClassMask) != turkicVowel && (before & turkicWeak) == 0)
    {
        return false;
    }
    out->kept = finalStart;
    out->tail[0] = (char)(0xC0 | voiced >> 6);
    out->tail[1] = (char)(0x80 | (voiced & 0x3F));
    out->tailLength = 2;
    return true;
}

const TurkicLanguage mlocKazakh = {
    .letters = s_letters,
    .letterCount = sizeof(s_letters) / sizeof(s_letters[0]),
    .scriptFirst = 0x400,
    .scriptLast = 0x52F,
    .a = {{0x430, 0x430}, {0x435, 0x435}},
    .i = {{0x44B, 0x44B}, {0x456, 0x456}},
    .onsets = s_onsets,
    .onsetCount = sizeof(s_onsets) / sizeof(s_onsets[0]),
    .vowels = nullptr,
    .vowelCount = 0,
    .roundsFirstOnly = false,
    .mark = '-',
    .markOn = turkicMarkForeign,
    .periodOrdinals = false,
    .finalYAsI = false,
    .lexicon = false,
    .stemNames = true,
    .pluralInThirdPlural = false,
    .plural = "LA\xD1\x80",
    .possessives = {{"V\xD0\xBC", "V\xD2\xA3", "SI"},
                    {"V\xD0\xBCI\xD0\xB7", "V\xD2\xA3LA\xD1\x80", "SI"}},
    .cases =
        {
            [turkicNominative] = {"", "", ""},
            [turkicGenitive] = {"PI\xD2\xA3", "PI\xD2\xA3", "PI\xD2\xA3"},
            [turkicDative] = {"GA",
                              "\xD0\xBD"
                              "A",
                              "HA"},
            [turkicAccusative] = {"NI", "\xD0\xBD", "NI"},
            [turkicLocative] = {"DA",
                                "\xD0\xBD\xD0\xB4"
                                "A",
                                "DA"},
            [turkicAblative] = {"BA\xD0\xBD",
                                "\xD0\xBD"
                                "A\xD0\xBD",
                                "BA\xD0\xBD"},
            [turkicInstrumental] = {"M\xD0\xB5\xD0\xBD", "M\xD0\xB5\xD0\xBD", "M\xD0\xB5\xD0\xBD"},
        },
    .copula = nullptr,
    .question = " QA",
    .letterName = KazakhLetterName,
    .numberWord = nullptr,
    .stem = KazakhStem,
};
