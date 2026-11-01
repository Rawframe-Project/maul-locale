// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Korean particles. A Hangul syllable's final is (cp - 0xAC00) % 28
// (Unicode 3.12): 0 none, 8 ㄹ. A compatibility jamo is its own letter; a
// Hanja reads by the generated table.

#include "grammar_ko.h"

#include "invariant.h"

#include "generated/hanja_finals.h"

#include <string.h>

// A particle's forms: after a consonant, after a vowel; whether ㄹ takes
// the vowel's; the consonant form's bytes that the vowel's lacks before
// it, or 0 where the two differ otherwise (both are written whole).
typedef struct Particle
{
    const char* consonant;
    const char* vowel;
    bool rieulAsVowel;
    uint8_t extra;
} Particle;

static const Particle s_particles[17] = {
    [mloc_particleTopic] = {"\xEC\x9D\x80", "\xEB\x8A\x94", false, 0},
    [mloc_particleSubject] = {"\xEC\x9D\xB4", "\xEA\xB0\x80", false, 0},
    [mloc_particleObject] = {"\xEC\x9D\x84", "\xEB\xA5\xBC", false, 0},
    [mloc_particleComitative] = {"\xEA\xB3\xBC", "\xEC\x99\x80", false, 0},
    [mloc_particleComitativeSpoken] = {"\xEC\x9D\xB4\xEB\x9E\x91", "\xEB\x9E\x91", false, 3},
    [mloc_particleVocative] = {"\xEC\x95\x84", "\xEC\x95\xBC", false, 0},
    [mloc_particleCopulaPolite] = {"\xEC\x9D\xB4\xEC\x97\x90\xEC\x9A\x94",
                                   "\xEC\x98\x88\xEC\x9A\x94", false, 0},
    [mloc_particleCopula] = {"\xEC\x9D\xB4\xEB\x8B\xA4", "\xEB\x8B\xA4", false, 3},
    [mloc_particleDirection] = {"\xEC\x9C\xBC\xEB\xA1\x9C", "\xEB\xA1\x9C", true, 3},
    [mloc_particleCapacity] = {"\xEC\x9C\xBC\xEB\xA1\x9C\xEC\x84\x9C", "\xEB\xA1\x9C\xEC\x84\x9C",
                               true, 3},
    [mloc_particleMeans] = {"\xEC\x9C\xBC\xEB\xA1\x9C\xEC\x8D\xA8", "\xEB\xA1\x9C\xEC\x8D\xA8",
                            true, 3},
    [mloc_particleSource] = {"\xEC\x9C\xBC\xEB\xA1\x9C\xEB\xB6\x80\xED\x84\xB0",
                             "\xEB\xA1\x9C\xEB\xB6\x80\xED\x84\xB0", true, 3},
    [mloc_particleChoice] = {"\xEC\x9D\xB4\xEB\x82\x98", "\xEB\x82\x98", false, 3},
    [mloc_particleAny] = {"\xEC\x9D\xB4\xEB\x93\xA0", "\xEB\x93\xA0", false, 3},
    [mloc_particleQuotation] = {"\xEC\x9D\xB4\xEB\x9D\xBC", "\xEB\x9D\xBC", false, 3},
    [mloc_particleDefinition] = {"\xEC\x9D\xB4\xEB\x9E\x80", "\xEB\x9E\x80", false, 3},
};
static_assert(sizeof(s_particles) / sizeof(s_particles[0]) == mloc_particleDefinition + 1,
              "a form for each particle");

bool mlocKoreanRequestValid(const mlocInflection* request)
{
    return request->grammaticalCase == mloc_caseNominative && !request->plural &&
           request->possessorPerson == 0 && !request->possessorPlural && !request->copula &&
           !request->question && (unsigned)request->particle <= mloc_particleDefinition;
}

// The code point that ends text[0, end); 0 for none.
static uint32_t Last(const uint8_t* text, size_t end)
{
    if (end == 0)
    {
        return 0;
    }
    size_t start = end - 1;
    while (start > 0 && end - start < 4 && (text[start] & 0xC0) == 0x80)
    {
        start--;
    }
    uint32_t c = text[start];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (start + extra + 1 != end)
    {
        return 0;
    }
    c &= extra == 0 ? 0x7F : 0x3F >> extra;
    for (size_t i = 1; i <= extra; i++)
    {
        c = c << 6 | (text[start + i] & 0x3Fu);
    }
    return c;
}

// Each block's table holds a two-bit final for each of its code points.
static_assert(sizeof(s_hanjaFinals3400) * 4 == 0x4DBF - 0x3400 + 1, "the 3400 block's finals");
static_assert(sizeof(s_hanjaFinals4E00) * 4 == 0x9FFF - 0x4E00 + 1, "the 4E00 block's finals");
static_assert(sizeof(s_hanjaFinalsF900) * 4 == 0xFAFF - 0xF900 + 1, "the F900 block's finals");

static KoreanFinal Hanja(uint32_t c)
{
    static const struct
    {
        uint32_t first;
        uint32_t last;
        const uint8_t* finals;
    } s_blocks[3] = {
        {0x3400, 0x4DBF, s_hanjaFinals3400},
        {0x4E00, 0x9FFF, s_hanjaFinals4E00},
        {0xF900, 0xFAFF, s_hanjaFinalsF900},
    };
    for (size_t i = 0; i < 3; i++)
    {
        if (c >= s_blocks[i].first && c <= s_blocks[i].last)
        {
            uint32_t at = c - s_blocks[i].first;
            return (KoreanFinal)(s_blocks[i].finals[at / 4] >> (at % 4 * 2) & 3);
        }
    }
    return koreanUnknown;
}

KoreanFinal mlocKoreanFinal(const uint8_t* text, size_t length)
{
    uint32_t c = Last(text, length);
    if (c >= 0xAC00 && c <= 0xD7A3)
    {
        uint32_t final = (c - 0xAC00) % 28;
        return final == 0 ? koreanVowel : final == 8 ? koreanRieul : koreanConsonant;
    }
    if (c >= 0x3131 && c <= 0x318E)
    {
        // Compatibility jamo: consonants to U+314E, then vowels.
        return c == 0x3139 ? koreanRieul : c <= 0x314E ? koreanConsonant : koreanVowel;
    }
    return Hanja(c);
}

const char* mlocKoreanLetterName(const uint8_t* text, size_t end)
{
    static const char* const s_names[26] = {
        "\xEC\x97\x90\xEC\x9D\xB4",
        "\xEB\xB9\x84",
        "\xEC\x94\xA8",
        "\xEB\x94\x94",
        "\xEC\x9D\xB4",
        "\xEC\x97\x90\xED\x94\x84",
        "\xEC\xA7\x80",
        "\xEC\x97\x90\xEC\x9D\xB4\xEC\xB9\x98",
        "\xEC\x95\x84\xEC\x9D\xB4",
        "\xEC\xA0\x9C\xEC\x9D\xB4",
        "\xEC\xBC\x80\xEC\x9D\xB4",
        "\xEC\x97\x98",
        "\xEC\x97\xA0",
        "\xEC\x97\x94",
        "\xEC\x98\xA4",
        "\xED\x94\xBC",
        "\xED\x81\x90",
        "\xEC\x95\x84\xEB\xA5\xB4",
        "\xEC\x97\x90\xEC\x8A\xA4",
        "\xED\x8B\xB0",
        "\xEC\x9C\xA0",
        "\xEB\xB8\x8C\xEC\x9D\xB4",
        "\xEB\x8D\x94\xEB\xB8\x94\xEC\x9C\xA0",
        "\xEC\x97\x91\xEC\x8A\xA4",
        "\xEC\x99\x80\xEC\x9D\xB4",
        "\xEC\xA0\x9C\xED\x8A\xB8",
    };
    uint32_t c = Last(text, end);
    c = c >= 'A' && c <= 'Z' ? c + 0x20 : c;
    return c >= 'a' && c <= 'z' ? s_names[c - 'a'] : nullptr;
}

const char* mlocKoreanSubjectStem(const uint8_t* word, size_t length)
{
    static const char* const s_pronouns[4][2] = {
        {"\xEB\x82\x98", "\xEB\x82\xB4"},
        {"\xEB\x84\x88", "\xEB\x84\xA4"},
        {"\xEC\xA0\x80", "\xEC\xA0\x9C"},
        {"\xEB\x88\x84\xEA\xB5\xAC", "\xEB\x88\x84"},
    };
    for (size_t i = 0; i < 4; i++)
    {
        if (length == strlen(s_pronouns[i][0]) && memcmp(word, s_pronouns[i][0], length) == 0)
        {
            return s_pronouns[i][1];
        }
    }
    return nullptr;
}

// Copies text into out[*at, room); false when it does not fit.
static bool Put(const char* text, size_t length, char* out, size_t room, size_t* at)
{
    if (*at + length > room)
    {
        return false;
    }
    memcpy(out + *at, text, length);
    *at += length;
    return true;
}

size_t mlocKoreanParticleText(mlocKoreanParticle particle, KoreanFinal final, char* out,
                              size_t room)
{
    if ((unsigned)particle == 0 || (unsigned)particle > mloc_particleDefinition)
    {
        return 0;
    }
    const Particle* p = &s_particles[particle];
    if (p->consonant == nullptr || p->vowel == nullptr)
    {
        return 0;
    }
    bool vowel = final == koreanVowel || (final == koreanRieul && p->rieulAsVowel);
    const char* one = final == koreanUnknown ? nullptr : vowel ? p->vowel : p->consonant;
    size_t at = 0;
    bool fits = true;
    if (one != nullptr)
    {
        fits = Put(one, strlen(one), out, room, &at);
    }
    else if (p->extra > 0)
    {
        // (으)로: the consonant form's own syllables, then the vowel form.
        MLOC_ASSERT(strcmp(p->consonant + p->extra, p->vowel) == 0);
        fits = Put("(", 1, out, room, &at) && Put(p->consonant, p->extra, out, room, &at) &&
               Put(")", 1, out, room, &at) && Put(p->vowel, strlen(p->vowel), out, room, &at);
    }
    else
    {
        // 은(는): the consonant form, then the vowel form in parentheses.
        fits = Put(p->consonant, strlen(p->consonant), out, room, &at) &&
               Put("(", 1, out, room, &at) && Put(p->vowel, strlen(p->vowel), out, room, &at) &&
               Put(")", 1, out, room, &at);
    }
    return fits ? at : 0;
}
