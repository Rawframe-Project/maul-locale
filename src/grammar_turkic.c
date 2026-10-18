// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Turkic suffix engine (grammar_turkic.h, records mloc-0046 and
// mloc-0047): a spoken end read by a language's letters, and templates
// written by its vowels and onsets.

#include "grammar_turkic.h"

#include <string.h>

// The Latin letters any Turkic language reads as Turkish does, the
// accented vowels as the Turkish vowel nearest them; a Latin letter not
// here is a voiced consonant.
static const TurkicLetter s_latin[] = {
    {'A', turkicVowel | turkicBack},
    {'E', turkicVowel},
    {'F', turkicVoiceless},
    {'H', turkicVoiceless},
    {'I', turkicVowel | turkicBack},
    {'K', turkicVoiceless},
    {'O', turkicVowel | turkicBack | turkicRound},
    {'P', turkicVoiceless},
    {'Q', turkicVoiceless},
    {'S', turkicVoiceless},
    {'T', turkicVoiceless},
    {'U', turkicVowel | turkicBack | turkicRound},
    {'X', turkicVoiceless},
    {'a', turkicVowel | turkicBack},
    {'e', turkicVowel},
    {'f', turkicVoiceless},
    {'h', turkicVoiceless},
    {'i', turkicVowel},
    {'k', turkicVoiceless},
    {'o', turkicVowel | turkicBack | turkicRound},
    {'p', turkicVoiceless},
    {'q', turkicVoiceless},
    {'s', turkicVoiceless},
    {'t', turkicVoiceless},
    {'u', turkicVowel | turkicBack | turkicRound},
    {'x', turkicVoiceless},
    {0xC0, turkicVowel | turkicBack},
    {0xC1, turkicVowel | turkicBack},
    {0xC2, turkicVowel | turkicBack},
    {0xC3, turkicVowel | turkicBack},
    {0xC4, turkicVowel},
    {0xC5, turkicVowel | turkicBack},
    {0xC6, turkicVowel},
    {0xC7, turkicVoiceless},
    {0xC8, turkicVowel},
    {0xC9, turkicVowel},
    {0xCA, turkicVowel},
    {0xCB, turkicVowel},
    {0xCC, turkicVowel},
    {0xCD, turkicVowel},
    {0xCE, turkicVowel},
    {0xCF, turkicVowel},
    {0xD2, turkicVowel | turkicBack | turkicRound},
    {0xD3, turkicVowel | turkicBack | turkicRound},
    {0xD4, turkicVowel | turkicBack | turkicRound},
    {0xD5, turkicVowel | turkicBack | turkicRound},
    {0xD6, turkicVowel | turkicRound},
    {0xD8, turkicVowel | turkicRound},
    {0xD9, turkicVowel | turkicBack | turkicRound},
    {0xDA, turkicVowel | turkicBack | turkicRound},
    {0xDB, turkicVowel | turkicBack | turkicRound},
    {0xDC, turkicVowel | turkicRound},
    {0xDF, turkicVoiceless},
    {0xE0, turkicVowel | turkicBack},
    {0xE1, turkicVowel | turkicBack},
    {0xE2, turkicVowel | turkicBack},
    {0xE3, turkicVowel | turkicBack},
    {0xE4, turkicVowel},
    {0xE5, turkicVowel | turkicBack},
    {0xE6, turkicVowel},
    {0xE7, turkicVoiceless},
    {0xE8, turkicVowel},
    {0xE9, turkicVowel},
    {0xEA, turkicVowel},
    {0xEB, turkicVowel},
    {0xEC, turkicVowel},
    {0xED, turkicVowel},
    {0xEE, turkicVowel},
    {0xEF, turkicVowel},
    {0xF2, turkicVowel | turkicBack | turkicRound},
    {0xF3, turkicVowel | turkicBack | turkicRound},
    {0xF4, turkicVowel | turkicBack | turkicRound},
    {0xF5, turkicVowel | turkicBack | turkicRound},
    {0xF6, turkicVowel | turkicRound},
    {0xF8, turkicVowel | turkicRound},
    {0xF9, turkicVowel | turkicBack | turkicRound},
    {0xFA, turkicVowel | turkicBack | turkicRound},
    {0xFB, turkicVowel | turkicBack | turkicRound},
    {0xFC, turkicVowel | turkicRound},
    {0x130, turkicVowel},
    {0x131, turkicVowel | turkicBack},
    {0x152, turkicVowel | turkicRound},
    {0x153, turkicVowel | turkicRound},
    {0x15E, turkicVoiceless},
    {0x15F, turkicVoiceless},
};

// A letter in a table sorted by code point, or false.
static bool Find(const TurkicLetter* table, size_t count, uint32_t c, uint16_t* infoOut)
{
    size_t low = 0;
    size_t high = count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (table[middle].codePoint < c)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    if (low < count && table[low].codePoint == c)
    {
        *infoOut = table[low].info;
        return true;
    }
    return false;
}

uint16_t mlocTurkicLetterInfo(const TurkicLanguage* language, uint32_t c)
{
    uint16_t info = turkicVoiced;
    if (!Find(language->letters, language->letterCount, c, &info))
    {
        (void)Find(s_latin, sizeof(s_latin) / sizeof(s_latin[0]), c, &info);
    }
    return info;
}

uint32_t mlocTurkicPrevious(const uint8_t* text, size_t end, size_t* startOut)
{
    size_t at = end - 1;
    while (at > 0 && (text[at] & 0xC0) == 0x80)
    {
        at--;
    }
    *startOut = at;
    uint32_t c = text[at];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
    for (size_t i = 1; i <= extra; i++)
    {
        c = c << 6 | (text[at + i] & 0x3F);
    }
    return c;
}

// A request's case as a template row, or turkicCases for one no Turkic
// language has.
static size_t CaseRow(mlocGrammaticalCase grammaticalCase)
{
    switch (grammaticalCase)
    {
    case mloc_caseNominative:
        return turkicNominative;
    case mloc_caseGenitive:
        return turkicGenitive;
    case mloc_caseDative:
        return turkicDative;
    case mloc_caseAccusative:
        return turkicAccusative;
    case mloc_caseLocative:
        return turkicLocative;
    case mloc_caseAblative:
        return turkicAblative;
    case mloc_caseInstrumental:
        return turkicInstrumental;
    default:
        return turkicCases;
    }
}

bool mlocTurkicRequestValid(const TurkicLanguage* language, const mlocInflection* r)
{
    size_t row = CaseRow(r->grammaticalCase);
    return row < turkicCases && language->cases[row][turkicAfterStem] != nullptr &&
           (!r->copula || language->copula != nullptr) && r->possessorPerson <= 3 &&
           (r->possessorPerson != 0 || !r->possessorPlural);
}

// Whether a final y after a consonant is said as i (Harry, Kennedy): no
// Turkish word ends so.
static bool FinalYAsI(const TurkicLanguage* language, const uint8_t* word, size_t start, uint32_t c)
{
    if (!language->finalYAsI || (c != 'y' && c != 'Y') || start == 0)
    {
        return false;
    }
    size_t before = 0;
    uint16_t info = mlocTurkicLetterInfo(language, mlocTurkicPrevious(word, start, &before));
    return (info & turkicClassMask) != turkicVowel;
}

// Takes a vowel's harmony; front makes it front.
static void Harmony(uint16_t info, bool front, TurkicSound* out)
{
    out->vowel = true;
    out->back = (info & turkicBack) != 0 && !front;
    out->round = (info & turkicRound) != 0;
    out->flat = (info & turkicFlat) != 0;
}

// The class a final soft sign leaves (apertium-kaz's reading of loans):
// a vowel's after a voiceless consonant (повестьнің, повестьлер), a
// liquid's after another (князьдің, князьмен).
static uint8_t SoftClass(uint16_t before)
{
    return (before & turkicClassMask) == turkicVoiceless ? turkicVowel : turkicLiquid;
}

bool mlocTurkicSound(const TurkicLanguage* language, const uint8_t* word, size_t length,
                     bool inverse, TurkicSound* out)
{
    *out = (TurkicSound){0};
    bool first = true;
    bool soft = false;
    bool front = inverse;
    uint16_t weak = 0;
    size_t end = length;
    while (end > 0)
    {
        size_t start = 0;
        uint32_t c = mlocTurkicPrevious(word, end, &start);
        uint16_t info = mlocTurkicLetterInfo(language, c);
        end = start;
        if ((info & turkicSign) != 0)
        {
            // A sign after the last vowel: ь makes the suffixes front.
            bool fronts = (info & turkicFront) != 0 && weak == 0;
            front = front || fronts;
            soft = soft || (first && fronts);
            continue;
        }
        if (first)
        {
            first = false;
            out->final = soft ? SoftClass(info) : (uint8_t)(info & turkicClassMask);
            // A final consonant that makes the suffixes front (Kazakh к).
            front = front || ((info & turkicFront) != 0 && !soft);
            if (FinalYAsI(language, word, start, c))
            {
                out->final = turkicVowel;
                Harmony(turkicVowel, false, out);
                return true;
            }
        }
        if ((info & turkicWeak) != 0)
        {
            weak = weak != 0 ? weak : info;
        }
        else if ((info & turkicClassMask) == turkicVowel)
        {
            Harmony(info, front, out);
            return true;
        }
    }
    if (weak != 0)
    {
        Harmony(weak, front, out);
        return true;
    }
    return false;
}

// Appends a code point and takes its sound.
static void Put(const TurkicLanguage* language, uint32_t c, TurkicSound* sound, TurkicSuffixes* out)
{
    uint8_t* at = (uint8_t*)out->text + out->length;
    size_t n = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
    if (n == 1)
    {
        at[0] = (uint8_t)c;
    }
    else if (n == 2)
    {
        at[0] = (uint8_t)(0xC0 | c >> 6);
        at[1] = (uint8_t)(0x80 | (c & 0x3F));
    }
    else
    {
        at[0] = (uint8_t)(0xE0 | c >> 12);
        at[1] = (uint8_t)(0x80 | (c >> 6 & 0x3F));
        at[2] = (uint8_t)(0x80 | (c & 0x3F));
    }
    out->length = (uint8_t)(out->length + n);
    if (c == ' ')
    {
        return;
    }
    uint16_t info = mlocTurkicLetterInfo(language, c);
    sound->final = (uint8_t)(info & turkicClassMask);
    if (sound->final == turkicVowel)
    {
        Harmony(info, false, sound);
    }
}

// The onset a symbol stands for, or null.
static const TurkicOnset* OnsetOf(const TurkicLanguage* language, uint32_t symbol)
{
    for (size_t i = 0; i < language->onsetCount; i++)
    {
        if ((uint32_t)language->onsets[i].symbol == symbol)
        {
            return &language->onsets[i];
        }
    }
    return nullptr;
}

// Decodes the code point at text[*at] (UTF-8, valid) and moves past it.
static uint32_t Next(const char* text, size_t* at)
{
    const uint8_t* s = (const uint8_t*)text + *at;
    size_t extra = s[0] >= 0xF0 ? 3 : s[0] >= 0xE0 ? 2 : s[0] >= 0xC0 ? 1 : 0;
    uint32_t c = s[0] & (extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F);
    for (size_t i = 1; i <= extra; i++)
    {
        c = c << 6 | (s[i] & 0x3F);
    }
    *at += extra + 1;
    return c;
}

// Appends one template symbol or letter.
static void AppendOne(const TurkicLanguage* language, uint32_t c, TurkicSound* sound,
                      TurkicSuffixes* out)
{
    size_t front = sound->back ? 0 : 1;
    size_t round = sound->round ? 1 : 0;
    const TurkicOnset* onset = OnsetOf(language, c);
    if (c == 'A')
    {
        Put(language, language->a[front][sound->flat ? 0 : round], sound, out);
    }
    else if (c == 'I' || (c == 'V' && sound->final != turkicVowel))
    {
        Put(language, language->i[front][round], sound, out);
    }
    else if (onset != nullptr)
    {
        uint16_t form = onset->forms[sound->final][front];
        if (form != 0)
        {
            Put(language, form, sound, out);
        }
    }
    else if (c != 'V')
    {
        Put(language, c, sound, out);
    }
}

// Appends a template.
static void Append(const TurkicLanguage* language, const char* morpheme, TurkicSound* sound,
                   TurkicSuffixes* out)
{
    for (size_t at = 0; morpheme[at] != '\0';)
    {
        AppendOne(language, Next(morpheme, &at), sound, out);
    }
}

// Whether the suffixes before the particle begin with a vowel.
static bool BeginsWithVowel(const TurkicLanguage* language, const TurkicSuffixes* s)
{
    if (s->particle == 0)
    {
        return false;
    }
    size_t at = 0;
    uint32_t c = Next(s->text, &at);
    return (mlocTurkicLetterInfo(language, c) & turkicClassMask) == turkicVowel;
}

void mlocTurkicSuffixes(const TurkicLanguage* language, TurkicSound sound, const mlocInflection* r,
                        bool endsThird, TurkicSuffixes* out)
{
    *out = (TurkicSuffixes){0};
    bool third = r->possessorPerson == 3 || endsThird;
    if (r->plural && !(language->pluralInThirdPlural && third && r->possessorPlural))
    {
        Append(language, language->plural, &sound, out);
    }
    if (r->possessorPerson != 0)
    {
        Append(language, language->possessives[r->possessorPlural ? 1 : 0][r->possessorPerson - 1],
               &sound, out);
    }
    size_t variant = third                     ? turkicAfterThird
                     : r->possessorPerson != 0 ? turkicAfterPossessive
                                               : turkicAfterStem;
    Append(language, language->cases[CaseRow(r->grammaticalCase)][variant], &sound, out);
    if (r->copula)
    {
        Append(language, language->copula, &sound, out);
    }
    out->particle = out->length;
    if (r->question)
    {
        Append(language, language->question, &sound, out);
    }
    out->vowelFirst = BeginsWithVowel(language, out);
}
