// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Turkish for the Turkic engine (grammar_turkic.h, record mloc-0046), as
// the TDK spelling guide gives it: two-way and four-way vowel harmony, d
// to t after a voiceless consonant, the buffer consonants y, n and s
// after a vowel, the n a third person's possessive puts before a case,
// the apostrophe on names, numbers and abbreviations, and a common
// noun's stem by the lexicon of marks.

#include "grammar_lexicon.h"
#include "grammar_turkic.h"

#include <string.h>

static const TurkicOnset s_onsets[] = {
    // D: d, t after a voiceless consonant.
    {'D', {{'d', 'd'}, {'d', 'd'}, {'d', 'd'}, {'d', 'd'}, {'d', 'd'}, {'t', 't'}}},
    // The buffers, after a vowel only.
    {'Y', {{'y', 'y'}}},
    {'N', {{'n', 'n'}}},
    {'S', {{'s', 's'}}},
};

static const char* TurkishLetterName(uint32_t c)
{
    static const char* const s_names[26] = {"a",
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
        return s_names[c - 'A'];
    }
    if (c >= 'a' && c <= 'z')
    {
        return c == 'i' ? "i" : s_names[c - 'a'];
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

// The softened form of a final stop before a vowel: b for p, c for ç, d
// for t, ğ for k and g, g for k after n.
static const char* Softened(uint32_t last, uint32_t before)
{
    switch (last)
    {
    case 'p':
        return "b";
    case 0xE7:
        return "c";
    case 't':
        return "d";
    case 'k':
        return before == 'n' ? "g" : "\xC4\x9F";
    case 'g':
        return "\xC4\x9F";
    default:
        return nullptr;
    }
}

// Appends bytes to a stem's tail, when they fit.
static void Tail(TurkicStem* stem, const char* bytes, size_t n)
{
    if (stem->tailLength + n <= sizeof(stem->tail))
    {
        memcpy(stem->tail + stem->tailLength, bytes, n);
        stem->tailLength = (uint8_t)(stem->tailLength + n);
    }
}

// By the rule the lexicon's marks are written against (Zemberek's), a
// final ç, k, p or t softens in a word of two syllables or more without
// inverse harmony and a final nk or og in any word, the marks saying
// otherwise; a marked word doubles its final consonant or drops its
// last vowel.
static bool TurkishStem(const uint8_t* word, size_t length, uint8_t marks, TurkicSuffixes* suffixes,
                        TurkicStem* out)
{
    if (!suffixes->vowelFirst)
    {
        return false;
    }
    *out = (TurkicStem){.kept = length};
    int vowels = 0;
    size_t lastVowel = length;
    size_t lastVowelEnd = length;
    for (size_t end = length; end > 0;)
    {
        size_t start = 0;
        uint16_t info = mlocTurkicLetterInfo(&mlocTurkish, mlocTurkicPrevious(word, end, &start));
        if ((info & turkicClassMask) == turkicVowel && vowels++ == 0)
        {
            lastVowel = start;
            lastVowelEnd = end;
        }
        end = start;
    }
    if (length == 0 || vowels == 0)
    {
        return false;
    }
    size_t finalStart = 0;
    uint32_t last = mlocTurkicPrevious(word, length, &finalStart);
    size_t beforeStart = 0;
    uint32_t before = finalStart > 0 ? mlocTurkicPrevious(word, finalStart, &beforeStart) : 0;
    const char* softened = Softened(last, before);
    bool stop = last == 'p' || last == 0xE7 || last == 't' || last == 'k';
    bool byRule = (stop && vowels >= 2 && (marks & turkicInverse) == 0) ||
                  (last == 'k' && before == 'n') || (last == 'g' && before == 'o');
    bool softens =
        softened != nullptr && (byRule ? (marks & turkicKeeps) == 0 : (marks & turkicSoftens) != 0);
    const char* finalBytes = softens ? softened : (const char*)word + finalStart;
    size_t finalLength = softens ? strlen(softened) : length - finalStart;
    bool drops = (marks & turkicDropsVowel) != 0 && vowels >= 2 && lastVowelEnd < length;
    if (!softens && !drops && (marks & turkicDoubles) == 0)
    {
        return false;
    }
    if (drops)
    {
        // The consonants after the dropped vowel, the final one softened.
        out->kept = lastVowel;
        Tail(out, (const char*)word + lastVowelEnd, finalStart - lastVowelEnd);
    }
    else
    {
        out->kept = finalStart;
    }
    Tail(out, finalBytes, finalLength);
    if ((marks & turkicDoubles) != 0)
    {
        Tail(out, finalBytes, finalLength);
    }
    return true;
}

const TurkicLanguage mlocTurkish = {
    .letters = nullptr,
    .letterCount = 0,
    .scriptFirst = 0,
    .scriptLast = 0x24F,
    .a = {{'a', 'a'}, {'e', 'e'}},
    .i = {{0x131, 'u'}, {'i', 0xFC}},
    .onsets = s_onsets,
    .onsetCount = sizeof(s_onsets) / sizeof(s_onsets[0]),
    .mark = '\'',
    .markOn = turkicMarkNames,
    .periodOrdinals = true,
    .finalYAsI = true,
    .lexicon = true,
    .stemNames = false,
    .pluralInThirdPlural = true,
    .plural = "lAr",
    .possessives = {{"Vm", "Vn", "SI"}, {"VmIz", "VnIz", "lArI"}},
    .cases =
        {
            [turkicNominative] = {"", "", ""},
            [turkicGenitive] = {"NIn", "nIn", "NIn"},
            [turkicDative] = {"YA", "nA", "YA"},
            [turkicAccusative] = {"YI", "nI", "YI"},
            [turkicLocative] = {"DA", "nDA", "DA"},
            [turkicAblative] = {"DAn", "nDAn", "DAn"},
            [turkicInstrumental] = {"YlA", "YlA", "YlA"},
        },
    .copula = "DIr",
    .question = " mI",
    .letterName = TurkishLetterName,
    .stem = TurkishStem,
};
