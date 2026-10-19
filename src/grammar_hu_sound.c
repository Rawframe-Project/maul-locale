// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// How a Hungarian word sounds (grammar_hu_sound.h, record mloc-0048).

#include "grammar_hu_sound.h"

#include <string.h>

static uint8_t VowelInfo(uint32_t c)
{
    switch (c)
    {
    case 'a':
    case 0xE0:
    case 0xE2:
    case 0xE3:
    case 0xE5:
    case 'o':
    case 0xF2:
    case 0xF4:
    case 0xF5:
        return hungarianVowelAny | hungarianVowelBack | hungarianVowelShort;
    case 0xE1:
    case 0xF3:
    case 'u':
    case 0xFA:
    case 0xF9:
    case 0xFB:
        return hungarianVowelAny | hungarianVowelBack;
    case 'e':
    case 0xE8:
    case 0xEA:
    case 0xEB:
    case 0xE4:
    case 0xE6:
        return hungarianVowelAny | hungarianVowelShort;
    case 0xE9:
    case 'i':
    case 0xED:
    case 0xEC:
    case 0xEE:
    case 0xEF:
        return hungarianVowelAny | hungarianVowelNeutral;
    case 0xF6:
    case 0xF8:
    case 0x153:
        return hungarianVowelAny | hungarianVowelRound | hungarianVowelShort;
    case 0x151:
    case 0xFC:
    case 0x171:
        return hungarianVowelAny | hungarianVowelRound;
    default:
        return 0;
    }
}

// The long vowel a short one is lengthened to (a á, e é, o ó, ö ő), a
// capital to a capital; 0 for none.
uint32_t mlocHungarianLongVowel(uint32_t c)
{
    switch (c)
    {
    case 'a':
        return 0xE1;
    case 'e':
        return 0xE9;
    case 'o':
        return 0xF3;
    case 0xF6:
        return 0x151;
    case 'A':
        return 0xC1;
    case 'E':
        return 0xC9;
    case 'O':
        return 0xD3;
    case 0xD6:
        return 0x150;
    default:
        return 0;
    }
}

// Decodes the code point that ends at text[end] (valid UTF-8); its first
// byte's index in startOut.
uint32_t mlocHungarianPrevious(const uint8_t* text, size_t end, size_t* startOut)
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

// The small letter of a Latin capital (ASCII, Latin-1, Latin Extended-A:
// Ő, Ű); other code points as they are.
uint32_t mlocHungarianSmall(uint32_t c)
{
    if ((c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
    {
        return c + 0x20;
    }
    bool evenCapital = (c >= 0x100 && c <= 0x137 && c != 0x130) || (c >= 0x14A && c <= 0x177);
    bool oddCapital = (c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E);
    if ((evenCapital && c % 2 == 0) || (oddCapital && c % 2 == 1))
    {
        return c + 1;
    }
    return c;
}

uint32_t mlocHungarianCapital(uint32_t c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 0xE0 && c <= 0xFE && c != 0xF7))
    {
        return c - 0x20;
    }
    return mlocHungarianSmall(c - 1) == c ? c - 1 : c;
}

// Reads the letters that end a text (after its last hyphen or space).
void mlocHungarianReadLetters(const uint8_t* text, size_t length, HungarianLetters* out)
{
    uint32_t reversed[hungarianLetters];
    size_t at[hungarianLetters];
    size_t n = 0;
    size_t end = length;
    while (end > 0 && n < hungarianLetters)
    {
        size_t start = end - 1;
        uint32_t c = text[start] < 0x80 ? text[start] : mlocHungarianPrevious(text, end, &start);
        if (c == '-' || c == ' ')
        {
            break;
        }
        reversed[n] = mlocHungarianSmall(c);
        at[n] = start;
        n++;
        end = start;
    }
    out->count = n;
    for (size_t i = 0; i < n; i++)
    {
        out->c[i] = reversed[n - 1 - i];
        out->at[i] = at[n - 1 - i];
    }
}

// Whether the letters before end are the UTF-8 string s.
bool mlocHungarianEndsWith(const HungarianLetters* w, size_t end, const char* s)
{
    size_t at = strlen(s);
    // Most strings end in an ASCII letter: their last byte decides at once.
    if (at > 0 && end > 0 && ((uint8_t)s[at - 1] < 0x80 || w->c[end - 1] < 0x80) &&
        w->c[end - 1] != (uint8_t)s[at - 1])
    {
        return false;
    }
    while (at > 0)
    {
        if (end == 0)
        {
            return false;
        }
        size_t start = 0;
        if (mlocHungarianPrevious((const uint8_t*)s, at, &start) != w->c[end - 1])
        {
            return false;
        }
        at = start;
        end--;
    }
    return true;
}

// A final y read as the vowel i: not the second letter of gy, ly, ny or
// ty (Kölcsey, Zichy; Kodály keeps ly).
static bool YAsI(const HungarianLetters* w, size_t i)
{
    if (w->c[i] != 'y' || i + 1 != w->count)
    {
        return false;
    }
    uint32_t before = i > 0 ? w->c[i - 1] : 0;
    return before != 'g' && before != 'l' && before != 'n' && before != 't';
}

uint8_t mlocHungarianLetterVowel(const HungarianLetters* w, size_t i)
{
    return YAsI(w, i) ? (uint8_t)(hungarianVowelAny | hungarianVowelNeutral) : VowelInfo(w->c[i]);
}

// The harmony by the last vowel that is not i, í or é. One of them after
// a back vowel leaves the word back (kávé, papír, Mali), two or more
// make it front (analízis, Euklidész), as the oracle counts them; the
// last vowel's rounding is read only for a front word.
HungarianHarmony mlocHungarianReadHarmony(const HungarianLetters* w)
{
    size_t neutral = 0;
    for (size_t i = w->count; i > 0; i--)
    {
        uint8_t v = mlocHungarianLetterVowel(w, i - 1);
        if ((v & hungarianVowelAny) == 0)
        {
            continue;
        }
        if ((v & hungarianVowelNeutral) != 0)
        {
            neutral++;
            continue;
        }
        if ((v & hungarianVowelBack) != 0)
        {
            return neutral <= 1 ? hungarianHarmonyBack : hungarianHarmonyFront;
        }
        return neutral == 0 && (v & hungarianVowelRound) != 0 ? hungarianHarmonyRound
                                                              : hungarianHarmonyFront;
    }
    return hungarianHarmonyFront;
}

// A written ending and the consonant it says.
typedef struct Spelling
{
    const char* written;
    const char* sound;
    bool modern;
} Spelling;

// Longest first: the long digraphs, the digraphs, then old and foreign
// spellings (AkH 12, 86, 160, 216).
static const Spelling s_spellings[] = {
    {"ddzs", "dzs", true}, {"dzs", "dzs", true}, {"ccs", "cs", true}, {"ddz", "dz", true},
    {"ggy", "gy", true},   {"lly", "ly", true},  {"nny", "ny", true}, {"ssz", "sz", true},
    {"tty", "ty", true},   {"zzs", "zs", true},  {"cs", "cs", true},  {"dz", "dz", true},
    {"gy", "gy", true},    {"ly", "ly", true},   {"ny", "ny", true},  {"sz", "sz", true},
    {"ty", "ty", true},    {"zs", "zs", true},   {"ch", "cs", false}, {"cz", "c", false},
    {"tz", "c", false},    {"ts", "cs", false},  {"th", "t", false},  {"gh", "g", false},
    {"ck", "k", false},    {"sh", "s", false},   {"ph", "f", false},  {"x", "sz", false},
    {"w", "v", false},     {"q", "k", false},
};

static bool IsDirectT(const char* sound)
{
    static const char* const s_direct[] = {"j", "l", "ly", "n", "ny", "r", "s", "sz", "z", "zs"};
    for (size_t i = 0; i < sizeof(s_direct) / sizeof(s_direct[0]); i++)
    {
        if (strcmp(sound, s_direct[i]) == 0)
        {
            return true;
        }
    }
    return false;
}

// Reads the final consonant: its sound, its spelling's kind and length.
static void ReadConsonant(const HungarianLetters* w, HungarianSound* s)
{
    const char* sound = nullptr;
    size_t written = 1;
    s->modern = true;
    // Every spelling ends in one of these letters.
    uint32_t end = w->c[w->count - 1];
    bool spelled = end < 0x80 && strchr("hkqswxyz", (int)end) != nullptr;
    for (size_t i = 0; spelled && i < sizeof(s_spellings) / sizeof(s_spellings[0]); i++)
    {
        if (mlocHungarianEndsWith(w, w->count, s_spellings[i].written))
        {
            sound = s_spellings[i].sound;
            written = strlen(s_spellings[i].written);
            s->modern = s_spellings[i].modern;
            break;
        }
    }
    uint32_t last = w->c[w->count - 1];
    char single[2] = {(char)(last < 0x80 ? last : '?'), 0};
    if (sound == nullptr)
    {
        sound = single;
    }
    s->isLong = sound != single ? written > strlen(sound) && s->modern
                                : w->count >= 2 && w->c[w->count - 2] == last;
    s->writtenLength = (uint8_t)written;
    s->soundLength = (uint8_t)strlen(sound);
    memcpy(s->sound, sound, s->soundLength);
    s->sound[s->soundLength] = '\0';
    // Not x's ksz (Marxot).
    s->directT = IsDirectT(s->sound) && !(written == 1 && last == 'x');
    size_t before = w->count - (s->isLong && sound == single ? 2 : written);
    s->cluster = before > 0 && (mlocHungarianLetterVowel(w, before - 1) & hungarianVowelAny) == 0;
}

void mlocHungarianReadSound(const HungarianLetters* w, HungarianSound* s)
{
    *s = (HungarianSound){0};
    for (size_t i = 0; i < w->count; i++)
    {
        bool vowel = (mlocHungarianLetterVowel(w, i) & hungarianVowelAny) != 0;
        s->hasVowel = s->hasVowel || vowel;
        s->vowels = (uint8_t)(s->vowels + (vowel && s->vowels < 255 ? 1 : 0));
    }
    if (w->count == 0)
    {
        return;
    }
    s->harmony = mlocHungarianReadHarmony(w);
    uint8_t v = mlocHungarianLetterVowel(w, w->count - 1);
    if ((v & hungarianVowelAny) != 0)
    {
        s->vowelEnd = true;
        s->vowel = (v & hungarianVowelShort) != 0 ? w->c[w->count - 1] : 0;
        return;
    }
    ReadConsonant(w, s);
}

const char* mlocHungarianLetterName(const uint8_t* text, size_t end)
{
    static const char* const s_names[26] = {"a",
                                            "b\xC3\xA9",
                                            "c\xC3\xA9",
                                            "d\xC3\xA9",
                                            "e",
                                            "ef",
                                            "g\xC3\xA9",
                                            "h\xC3\xA1",
                                            "i",
                                            "j\xC3\xA9",
                                            "k\xC3\xA1",
                                            "el",
                                            "em",
                                            "en",
                                            "o",
                                            "p\xC3\xA9",
                                            "k\xC3\xBA",
                                            "er",
                                            "es",
                                            "t\xC3\xA9",
                                            "u",
                                            "v\xC3\xA9",
                                            "dupla v\xC3\xA9",
                                            "iksz",
                                            "ipszilon",
                                            "z\xC3\xA9"};
    static const Spelling s_digraphs[] = {
        {"dzs", "dzs\xC3\xA9", true}, {"cs", "cs\xC3\xA9", true}, {"dz", "dz\xC3\xA9", true},
        {"gy", "gy\xC3\xA9", true},   {"ly", "elipszilon", true}, {"ny", "eny", true},
        {"sz", "esz", true},          {"ty", "ty\xC3\xA9", true}, {"zs", "zs\xC3\xA9", true}};
    HungarianLetters w;
    mlocHungarianReadLetters(text, end, &w);
    if (w.count == 0)
    {
        return nullptr;
    }
    for (size_t i = 0; i < sizeof(s_digraphs) / sizeof(s_digraphs[0]); i++)
    {
        if (mlocHungarianEndsWith(&w, w.count, s_digraphs[i].written))
        {
            return s_digraphs[i].sound;
        }
    }
    uint32_t c = w.c[w.count - 1];
    if (c >= 'a' && c <= 'z')
    {
        return s_names[c - 'a'];
    }
    // The long and rounded vowels are named by themselves.
    switch (c)
    {
    case 0xE1:
        return "\xC3\xA1";
    case 0xE9:
        return "\xC3\xA9";
    case 0xED:
        return "\xC3\xAD";
    case 0xF3:
        return "\xC3\xB3";
    case 0xF6:
        return "\xC3\xB6";
    case 0x151:
        return "\xC5\x91";
    case 0xFA:
        return "\xC3\xBA";
    case 0xFC:
        return "\xC3\xBC";
    case 0x171:
        return "\xC5\xB1";
    default:
        return nullptr;
    }
}
