// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Finnish words as code points (record mloc-0049): reading, harmony,
// the patterns the types' rows write, consonant gradation.

#include "grammar_fi_word.h"

#include "maul-unicode/encoding.h"

#include <string.h>

uint32_t mlocFinnishPrevious(const uint8_t* text, size_t end, size_t* startOut)
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

uint32_t mlocFinnishSmall(uint32_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7) ? c + 0x20 : c;
}

// Reads the letters after a text's last space or hyphen, the last
// finnishLettersRead of them at most; fromOut receives where they begin.
void mlocFinnishReadWord(const uint8_t* text, size_t length, FinnishWord* out, size_t* fromOut)
{
    size_t start = length;
    for (size_t letters = 0; start > 0 && letters < finnishLettersRead; letters++)
    {
        size_t before = 0;
        uint32_t c = mlocFinnishPrevious(text, start, &before);
        if (c == ' ' || c == '-')
        {
            break;
        }
        start = before;
    }
    *fromOut = start;
    out->count = 0;
    for (size_t at = start; at < length && out->count < finnishLetterRoom;)
    {
        size_t next = at + 1;
        while (next < length && (text[next] & 0xC0) == 0x80)
        {
            next++;
        }
        size_t first = 0;
        out->c[out->count++] = mlocFinnishPrevious(text, next, &first);
        at = next;
    }
}

bool mlocFinnishIsVowel(uint32_t c)
{
    c = mlocFinnishSmall(c);
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u' || c == 'y' || c == 0xE4 ||
           c == 0xF6 || c == 0xE5 || c == 0xE9 || c == 0xFC;
}

// The harmony by the last vowel that is not e or i: back for a, o, u
// (Tampereella, Hämeenlinnassa), front for ä, ö, y and for none
// (Malmössä, Kemissä).
bool mlocFinnishIsBack(const FinnishWord* w)
{
    for (size_t i = w->count; i > 0; i--)
    {
        uint32_t c = mlocFinnishSmall(w->c[i - 1]);
        if (c == 'a' || c == 'o' || c == 'u' || c == 0xE5)
        {
            return true;
        }
        if (c == 0xE4 || c == 0xF6 || c == 'y' || c == 0xFC)
        {
            return false;
        }
    }
    return false;
}

void mlocFinnishPush(FinnishWord* w, uint32_t c)
{
    if (w->count < finnishLetterRoom)
    {
        w->c[w->count++] = c;
    }
}

// The last vowel written, or a.
static uint32_t LastVowel(const FinnishWord* w)
{
    for (size_t i = w->count; i > 0; i--)
    {
        if (mlocFinnishIsVowel(w->c[i - 1]))
        {
            return mlocFinnishSmall(w->c[i - 1]);
        }
    }
    return 'a';
}

// Appends a pattern: letters as they are; A, O, U by harmony (a or ä, o
// or ö, u or y); V the last vowel again; L the original word's last
// letter.
void mlocFinnishAppend(FinnishWord* w, const char* pattern, bool back, uint32_t last)
{
    if (pattern == nullptr)
    {
        return;
    }
    for (const char* p = pattern; *p != '\0'; p++)
    {
        switch (*p)
        {
        case 'A':
            mlocFinnishPush(w, back ? 'a' : 0xE4);
            break;
        case 'O':
            mlocFinnishPush(w, back ? 'o' : 0xF6);
            break;
        case 'U':
            mlocFinnishPush(w, back ? 'u' : 'y');
            break;
        case 'V':
            mlocFinnishPush(w, LastVowel(w));
            break;
        case 'L':
            mlocFinnishPush(w, last);
            break;
        default:
            mlocFinnishPush(w, (uint8_t)*p);
            break;
        }
    }
}

// The gradation letters' strong and weak consonants.
static const char* const s_strong[13] = {"kk", "pp", "tt", "k",  "p", "t", "nk",
                                         "mp", "lt", "nt", "rt", "k", "k"};
static const char* const s_weak[13] = {"k",  "p",  "t",  "",   "v", "d", "ng",
                                       "mm", "ll", "nn", "rr", "j", "v"};

// Turns the last of a stem's consonants a gradation letter changes,
// before a vowel, into the other grade.
// Whether the letters from one on are the last syllable's vowel group
// and end, three letters at most (the consonant before them is the one
// that grades: katu, hammas, onneton).
static bool LastSyllable(const FinnishWord* w, size_t from)
{
    if (w->count - from > 3)
    {
        return false;
    }
    bool consonant = false;
    for (size_t i = from; i < w->count; i++)
    {
        if (!mlocFinnishIsVowel(w->c[i]))
        {
            consonant = true;
        }
        else if (consonant)
        {
            return false;
        }
    }
    return true;
}

// Whether the grade's consonants stand at a word's letter at, before
// the last syllable's vowel.
static bool GradesAt(const FinnishWord* w, size_t at, const char* from, size_t n)
{
    if (at + n >= w->count || !mlocFinnishIsVowel(w->c[at + n]) || !LastSyllable(w, at + n))
    {
        return false;
    }
    for (size_t i = 0; i < n; i++)
    {
        if (mlocFinnishSmall(w->c[at + i]) != (uint8_t)from[i])
        {
            return false;
        }
    }
    return true;
}

// A k gone between two same vowels after a long vowel or a diphthong
// leaves an apostrophe (vaaka: vaa'an; but koko: koon), a diphthong's i
// before a vowel is written j (aika: ajan). w ends where the k was.
static void KGone(FinnishWord* w, uint32_t next)
{
    size_t at = w->count;
    if (at < 2 || !mlocFinnishIsVowel(w->c[at - 2]))
    {
        return;
    }
    uint32_t before = mlocFinnishSmall(w->c[at - 1]);
    if (before == mlocFinnishSmall(next))
    {
        mlocFinnishPush(w, '\'');
    }
    else if (before == 'i' && mlocFinnishIsVowel(next))
    {
        w->c[at - 1] = w->c[at - 1] == 'I' ? 'J' : 'j';
    }
}

void mlocFinnishGrade(FinnishWord* w, char letter, bool toWeak)
{
    if (letter < 'A' || letter > 'M')
    {
        return;
    }
    const char* from = toWeak ? s_strong[letter - 'A'] : s_weak[letter - 'A'];
    const char* to = toWeak ? s_weak[letter - 'A'] : s_strong[letter - 'A'];
    size_t n = strlen(from);
    if (n == 0)
    {
        return;
    }
    for (size_t at = w->count > n ? w->count - n : 0; at > 0; at--)
    {
        if (!GradesAt(w, at, from, n))
        {
            continue;
        }
        FinnishWord rest = {0};
        for (size_t i = at + n; i < w->count; i++)
        {
            mlocFinnishPush(&rest, w->c[i]);
        }
        w->count = at;
        mlocFinnishAppend(w, to, true, 0);
        if (*to == '\0')
        {
            KGone(w, rest.c[0]);
        }
        for (size_t i = 0; i < rest.count; i++)
        {
            mlocFinnishPush(w, rest.c[i]);
        }
        return;
    }
}
