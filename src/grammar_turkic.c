// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Turkish suffixes (grammar_turkic.h, record mloc-0046), as the TDK
// spelling guide gives them: two-way and four-way vowel harmony, d to t
// after a voiceless consonant, the buffer consonants y, n and s after a
// vowel, and the n a third person's possessive puts before a case.

#include "grammar_turkic.h"

#include <string.h>

// The vowels as one byte: back a, ı, o, u ('a', 'i', 'o', 'u') and front
// e, i, ö, ü ('e', 'I', 'O', 'U').
static bool IsBack(char vowel)
{
    return vowel == 'a' || vowel == 'i' || vowel == 'o' || vowel == 'u';
}

static bool IsRounded(char vowel)
{
    return vowel == 'o' || vowel == 'u' || vowel == 'O' || vowel == 'U';
}

// The vowels Turkish reads, as one byte each, and the accented Latin
// vowels read as the Turkish vowel nearest them.
typedef struct VowelOf
{
    uint16_t codePoint;
    char vowel;
} VowelOf;

static const VowelOf s_vowels[] = {
    {'a', 'a'},  {'A', 'a'},   {0xE2, 'a'}, {0xC2, 'a'},  {0xE1, 'a'},  {0xC1, 'a'},  {0xE0, 'a'},
    {0xC0, 'a'}, {0xE3, 'a'},  {0xC3, 'a'}, {0xE5, 'a'},  {0xC5, 'a'},  {'e', 'e'},   {'E', 'e'},
    {0xE9, 'e'}, {0xC9, 'e'},  {0xE8, 'e'}, {0xC8, 'e'},  {0xEA, 'e'},  {0xCA, 'e'},  {0xEB, 'e'},
    {0xCB, 'e'}, {0xE4, 'e'},  {0xC4, 'e'}, {0xE6, 'e'},  {0xC6, 'e'},  {0x131, 'i'}, {'I', 'i'},
    {'i', 'I'},  {0x130, 'I'}, {0xEE, 'I'}, {0xCE, 'I'},  {0xED, 'I'},  {0xCD, 'I'},  {0xEC, 'I'},
    {0xCC, 'I'}, {0xEF, 'I'},  {0xCF, 'I'}, {'o', 'o'},   {'O', 'o'},   {0xF3, 'o'},  {0xD3, 'o'},
    {0xF2, 'o'}, {0xD2, 'o'},  {0xF4, 'o'}, {0xD4, 'o'},  {0xF5, 'o'},  {0xD5, 'o'},  {0xF6, 'O'},
    {0xD6, 'O'}, {0xF8, 'O'},  {0xD8, 'O'}, {0x153, 'O'}, {0x152, 'O'}, {'u', 'u'},   {'U', 'u'},
    {0xFB, 'u'}, {0xDB, 'u'},  {0xFA, 'u'}, {0xDA, 'u'},  {0xF9, 'u'},  {0xD9, 'u'},  {0xFC, 'U'},
    {0xDC, 'U'},
};

// The voiceless consonants (fıstıkçı şahap), x and q, said with k, and ß.
static const uint16_t s_voiceless[] = {'f', 'F', 's', 'S', 't', 'T',  'k',  'K',   'h',   'H', 'p',
                                       'P', 'x', 'X', 'q', 'Q', 0xE7, 0xC7, 0x15F, 0x15E, 0xDF};

// A code point read as a Turkish letter: its vowel byte, or 0 for a
// consonant, with whether the consonant is voiceless. The caller has
// kept only letters, so any other is a voiced consonant.
static void Letter(uint32_t c, char* vowelOut, bool* voicelessOut)
{
    *vowelOut = 0;
    *voicelessOut = false;
    for (size_t i = 0; i < sizeof(s_vowels) / sizeof(s_vowels[0]); i++)
    {
        if (s_vowels[i].codePoint == c)
        {
            *vowelOut = s_vowels[i].vowel;
            return;
        }
    }
    for (size_t i = 0; i < sizeof(s_voiceless) / sizeof(s_voiceless[0]); i++)
    {
        if (s_voiceless[i] == c)
        {
            *voicelessOut = true;
            return;
        }
    }
}

// Decodes the code point that ends at text[end], valid UTF-8; its first
// byte's index in startOut.
static uint32_t Previous(const uint8_t* text, size_t end, size_t* startOut)
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

// The loanwords the spelling guides name whose suffixes take front
// vowels after a back last vowel (saate, rolü, alkolü, harfi, hali), as
// ASCII lowercase.
static const char* const s_frontLoans[] = {"alkol",   "gol",    "hal", "harf",
                                           "kontrol", "petrol", "rol", "saat"};

static bool FrontLoan(const uint8_t* word, size_t length)
{
    char lower[8];
    if (length > sizeof(lower))
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        uint8_t b = word[i];
        // Only ASCII letters; an uppercase I is Turkish's ı, which no
        // entry has.
        if (b >= 0x80 || b == 'I')
        {
            return false;
        }
        lower[i] = (char)(b >= 'A' && b <= 'Z' ? b + 32 : b);
    }
    for (size_t i = 0; i < sizeof(s_frontLoans) / sizeof(s_frontLoans[0]); i++)
    {
        if (strlen(s_frontLoans[i]) == length && memcmp(s_frontLoans[i], lower, length) == 0)
        {
            return true;
        }
    }
    return false;
}

static char Front(char vowel)
{
    return vowel == 'a'   ? 'e'
           : vowel == 'i' ? 'I'
           : vowel == 'o' ? 'O'
           : vowel == 'u' ? 'U'
                          : vowel;
}

bool mlocTurkicSound(const uint8_t* word, size_t length, TurkicSound* out)
{
    *out = (TurkicSound){0};
    bool first = true;
    bool previousConsonant = false;
    size_t end = length;
    while (end > 0)
    {
        size_t start = 0;
        uint32_t c = Previous(word, end, &start);
        char vowel = 0;
        bool voiceless = false;
        Letter(c, &vowel, &voiceless);
        if (first)
        {
            out->finalVowel = vowel != 0;
            out->voiceless = voiceless;
            first = false;
            // A final y after a consonant is said as i (Harry, Kennedy):
            // no Turkish word ends so.
            if ((c == 'y' || c == 'Y') && start > 0)
            {
                size_t before = 0;
                char v = 0;
                bool unvoiced = false;
                Letter(Previous(word, start, &before), &v, &unvoiced);
                previousConsonant = v == 0;
                if (previousConsonant)
                {
                    out->vowel = 'I';
                    out->finalVowel = true;
                    return true;
                }
            }
        }
        if (vowel != 0)
        {
            out->vowel = FrontLoan(word, length) ? Front(vowel) : vowel;
            return true;
        }
        end = start;
    }
    return false;
}

const char* mlocTurkicLetterName(uint32_t c)
{
    static const char* const s_latin[26] = {"a",
                                            "be",
                                            "ce",
                                            "de",
                                            "e",
                                            "fe",
                                            "ge",
                                            "he",
                                            "\xC4\xB1",
                                            "je",
                                            "ke",
                                            "le",
                                            "me",
                                            "ne",
                                            "o",
                                            "pe",
                                            "k\xC3\xBB",
                                            "re",
                                            "se",
                                            "te",
                                            "u",
                                            "ve",
                                            "\xC3\xA7ift ve",
                                            "iks",
                                            "ye",
                                            "ze"};
    if (c >= 'A' && c <= 'Z')
    {
        return s_latin[c - 'A'];
    }
    if (c >= 'a' && c <= 'z')
    {
        return c == 'i' ? "i" : s_latin[c - 'a'];
    }
    switch (c)
    {
    case 0xC7:
    case 0xE7:
        return "\xC3\xA7"
               "e";
    case 0x11E:
    case 0x11F:
        return "yumu\xC5\x9F"
               "ak ge";
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

bool mlocTurkicRequestValid(const mlocInflection* r)
{
    switch (r->grammaticalCase)
    {
    case mloc_caseNominative:
    case mloc_caseGenitive:
    case mloc_caseDative:
    case mloc_caseAccusative:
    case mloc_caseInstrumental:
    case mloc_caseAblative:
    case mloc_caseLocative:
        break;
    default:
        return false;
    }
    return r->possessorPerson <= 3 && (r->possessorPerson != 0 || !r->possessorPlural);
}

// Appends bytes that end in a vowel.
static void PutVowel(const char* bytes, TurkicSound* sound, TurkicSuffixes* out)
{
    size_t n = strlen(bytes);
    memcpy(out->text + out->length, bytes, n);
    out->length = (uint8_t)(out->length + n);
    sound->finalVowel = true;
    sound->voiceless = false;
}

// Appends the two-way vowel: a after a back vowel, e after a front one.
static void PutA(TurkicSound* sound, TurkicSuffixes* out)
{
    sound->vowel = IsBack(sound->vowel) ? 'a' : 'e';
    PutVowel(sound->vowel == 'a' ? "a" : "e", sound, out);
}

// Appends the four-way vowel: ı, i, u or ü by backness and rounding.
static void PutI(TurkicSound* sound, TurkicSuffixes* out)
{
    bool back = IsBack(sound->vowel);
    bool round = IsRounded(sound->vowel);
    sound->vowel = back ? (round ? 'u' : 'i') : (round ? 'U' : 'I');
    PutVowel(back ? (round ? "u" : "\xC4\xB1") : (round ? "\xC3\xBC" : "i"), sound, out);
}

// Appends a consonant (D as d or t) or a space.
static void PutConsonant(char c, TurkicSound* sound, TurkicSuffixes* out)
{
    char letter = c == 'D' ? (sound->voiceless ? 't' : 'd') : c;
    out->text[out->length++] = letter;
    if (letter != ' ')
    {
        sound->finalVowel = false;
        sound->voiceless = strchr("fstkhp", letter) != nullptr;
    }
}

// Appends a suffix written in archiphonemes: A for a or e, I for ı, i,
// u or ü, D for d or t; Y, N and S for y, n and s after a vowel only, V
// for the four-way vowel after a consonant only; a space; and the
// consonants as they are.
static void Append(const char* morpheme, TurkicSound* sound, TurkicSuffixes* out)
{
    for (const char* m = morpheme; *m != '\0'; m++)
    {
        char c = *m;
        bool buffer = c == 'Y' || c == 'N' || c == 'S';
        if (c == 'A')
        {
            PutA(sound, out);
        }
        else if (c == 'I' || (c == 'V' && !sound->finalVowel))
        {
            PutI(sound, out);
        }
        else if (buffer && sound->finalVowel)
        {
            PutConsonant(c == 'Y' ? 'y' : c == 'N' ? 'n' : 's', sound, out);
        }
        else if (c != 'V' && !buffer)
        {
            PutConsonant(c, sound, out);
        }
    }
}

// A case's suffix, after a third person's possessive (pronominal n) or
// not.
static const char* CaseSuffix(mlocGrammaticalCase grammaticalCase, bool afterThird)
{
    switch (grammaticalCase)
    {
    case mloc_caseGenitive:
        return afterThird ? "nIn" : "NIn";
    case mloc_caseDative:
        return afterThird ? "nA" : "YA";
    case mloc_caseAccusative:
        return afterThird ? "nI" : "YI";
    case mloc_caseLocative:
        return afterThird ? "nDA" : "DA";
    case mloc_caseAblative:
        return afterThird ? "nDAn" : "DAn";
    case mloc_caseInstrumental:
        return "YlA";
    default:
        return "";
    }
}

void mlocTurkicSuffixes(TurkicSound sound, const mlocInflection* r, TurkicSuffixes* out)
{
    *out = (TurkicSuffixes){0};
    bool third = r->possessorPerson == 3;
    // A third person plural's possessive after a plural is written once
    // (evleri: their houses).
    if (r->plural && !(third && r->possessorPlural))
    {
        Append("lAr", &sound, out);
    }
    if (r->possessorPerson != 0)
    {
        static const char* const s_possessives[2][3] = {{"Vm", "Vn", "SI"},
                                                        {"VmIz", "VnIz", "lArI"}};
        Append(s_possessives[r->possessorPlural ? 1 : 0][r->possessorPerson - 1], &sound, out);
    }
    Append(CaseSuffix(r->grammaticalCase, third), &sound, out);
    if (r->copula)
    {
        Append("DIr", &sound, out);
    }
    out->particle = out->length;
    if (r->question)
    {
        Append(" mI", &sound, out);
    }
    // Every suffix but the plural's (l), a consonant possessive's after a
    // vowel (s, m, n) and the case's d, t or y after a vowel begins with a
    // vowel when the stem ends in a consonant.
    out->vowelFirst =
        out->particle > 0 && out->text[0] != 'l' && out->text[0] != 'd' && out->text[0] != 't';
}

bool mlocTurkicSoften(const uint8_t* word, size_t length, const char** replacementOut,
                      size_t* replacedOut)
{
    int vowels = 0;
    for (size_t end = length; end > 0;)
    {
        size_t start = 0;
        char vowel = 0;
        bool voiceless = false;
        Letter(Previous(word, end, &start), &vowel, &voiceless);
        vowels += vowel != 0 ? 1 : 0;
        end = start;
    }
    if (vowels < 2 || length < 2)
    {
        return false;
    }
    uint8_t last = word[length - 1];
    if (last == 'p')
    {
        *replacementOut = "b";
        *replacedOut = 1;
        return true;
    }
    if (length >= 2 && word[length - 2] == 0xC3 && last == 0xA7)
    {
        *replacementOut = "c";
        *replacedOut = 2;
        return true;
    }
    if (last != 'k')
    {
        return false;
    }
    size_t before = 0;
    char vowel = 0;
    bool voiceless = false;
    uint32_t previous = Previous(word, length - 1, &before);
    Letter(previous, &vowel, &voiceless);
    if (previous == 'n')
    {
        *replacementOut = "g";
        *replacedOut = 1;
        return true;
    }
    if (vowel != 0)
    {
        *replacementOut = "\xC4\x9F";
        *replacedOut = 1;
        return true;
    }
    return false;
}
