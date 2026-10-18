// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for the inflector (records mloc-0046 to mloc-0048),
// Turkish, Kazakh, Kyrgyz, Azerbaijani, Uzbek, Turkmen or Hungarian, with
// the spell-out file beside the compiled-in data.
// The first two bytes choose the request (case, plural, possessor,
// copula, question, common noun, reading) and the language, the third
// where the text ends and the hint begins (0 for no hint); the rest is
// the text. An inflection refuses with invalid input, a missing number
// reading or capacity alone; it measures what it then writes, writes the
// same twice, writes valid UTF-8 that begins with the text (but a stem's
// changed end), and its parts tile it. The text read as a decimal is
// inflected as a number too.

#include "maul-locale/data.h"
#include "maul-locale/grammar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// Whether bytes are UTF-8: well-formed sequences, no surrogates, nothing
// past U+10FFFF.
static bool Utf8(const uint8_t* s, size_t n)
{
    for (size_t i = 0; i < n;)
    {
        uint8_t b = s[i];
        size_t extra = b < 0x80                ? 0
                       : b >= 0xC2 && b < 0xE0 ? 1
                       : b >= 0xE0 && b < 0xF0 ? 2
                       : b >= 0xF0 && b < 0xF5 ? 3
                                               : 9;
        if (extra == 9)
        {
            return false;
        }
        uint32_t c = b & (extra == 0 ? 0x7F : extra == 1 ? 0x1F : extra == 2 ? 0x0F : 0x07);
        for (size_t k = 1; k <= extra; k++)
        {
            if (i + k >= n || (s[i + k] & 0xC0) != 0x80)
            {
                return false;
            }
            c = c << 6 | (s[i + k] & 0x3F);
        }
        if ((extra == 2 && (c < 0x800 || (c >= 0xD800 && c < 0xE000))) ||
            (extra == 3 && (c < 0x10000 || c > 0x10FFFF)))
        {
            return false;
        }
        i += extra + 1;
    }
    return true;
}

// The languages drawn, by three bits of the request, and whether each
// changes a name's stem.
enum
{
    languageSlots = 8,
};
static const char* const s_languages[languageSlots] = {"tr", "kk", "ky", "az",
                                                       "uz", "tk", "hu", "fi"};
static const bool s_stemsNames[languageSlots] = {false, true, true, false, true, true, true, true};

// A language's inflector, with the spell-out rules when the file opens.
static const mlocInflector* Inflector(size_t language)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_rules[4 << 20];
    static mlocInflector s_inflectors[languageSlots];
    static int s_state;
    if (s_state == 0)
    {
        mlocOpenBuiltinData(&s_sources[0]);
        size_t count = 1;
        FILE* file = fopen(MLOC_SPELLOUT_FILE, "rb");
        if (file != nullptr)
        {
            size_t read = fread(s_rules, 1, sizeof(s_rules), file);
            fclose(file);
            if (read < sizeof(s_rules) &&
                mlocOpenData(&s_sources[1], s_rules, read) == mloc_success)
            {
                count = 2;
            }
        }
        mlocDataSet set = {s_sources, count};
        for (size_t i = 0; i < languageSlots; i++)
        {
            mlocLocale locale;
            Require(mlocParseLocale(s_languages[i], 2, &locale).status == mloc_success &&
                    mlocInitInflector(&s_inflectors[i], &set, &locale) == mloc_success);
        }
        s_state = 1;
    }
    return &s_inflectors[language];
}

// The parts a call reported: the inflection's must tile its text, a
// number's lie inside the stem.
typedef struct Tiling
{
    size_t next;
    size_t stemEnd;
    bool good;
} Tiling;

static void Tile(void* context, uint32_t part, size_t start, size_t length)
{
    Tiling* t = context;
    if (part < mloc_inflectionPartStem)
    {
        t->good = t->good && part <= mloc_numberPartUnit && start + length <= t->stemEnd;
        return;
    }
    t->good = t->good && part <= mloc_inflectionPartParticle && start == t->next && length > 0;
    t->next = start + length;
    t->stemEnd = part == mloc_inflectionPartStem ? t->next : t->stemEnd;
}

// The request: the case by three bits of the first byte and the second
// byte's last (Hungarian's cases in the upper eight, so the seeds written
// before them keep their meaning).
static mlocInflection RequestOf(const uint8_t* bytes)
{
    static const mlocGrammaticalCase s_cases[16] = {
        mloc_caseNominative,  mloc_caseGenitive,    mloc_caseDative,       mloc_caseAccusative,
        mloc_caseLocative,    mloc_caseAblative,    mloc_caseInstrumental, mloc_caseVocative,
        mloc_caseInessive,    mloc_caseSuperessive, mloc_caseAdessive,     mloc_caseIllative,
        mloc_caseTranslative, mloc_caseEssive,      mloc_caseAllative,     mloc_caseElative};
    mlocInflection r;
    memset(&r, 0, sizeof(r));
    r.grammaticalCase = s_cases[(bytes[0] & 7) | (bytes[1] >> 7) << 3];
    r.plural = (bytes[0] & 8) != 0;
    r.possessorPerson = (uint8_t)(bytes[0] >> 4 & 3);
    r.possessorPlural = (bytes[0] & 0x40) != 0 && r.possessorPerson != 0;
    r.copula = (bytes[0] & 0x80) != 0;
    r.question = (bytes[1] & 1) != 0;
    r.commonNoun = (bytes[1] & 2) != 0;
    r.reading = (mlocInflectionReading)(bytes[1] >> 2 & 3);
    return r;
}

// Checks one call's contract; text and length are null and 0 for a
// number.
static void Check(const mlocInflector* f, bool stemsNames, const char* text, size_t length,
                  const mlocDecimal* number, const mlocInflection* r)
{
    static char s_out[8192];
    static char s_again[8192];
    mlocOutput measure = {nullptr, 0, 0, nullptr, nullptr};
    mlocResult measured = text != nullptr ? mlocInflectText(f, text, length, r, &measure)
                                          : mlocInflectNumber(f, nullptr, number, r, &measure);
    mlocOutput out = {s_out, sizeof(s_out), 0, nullptr, nullptr};
    mlocResult result = text != nullptr ? mlocInflectText(f, text, length, r, &out)
                                        : mlocInflectNumber(f, nullptr, number, r, &out);
    Require(result == mloc_success || result == mloc_errorInvalid || result == mloc_errorNotFound ||
            result == mloc_errorCapacity);
    if (result != mloc_success)
    {
        Require(measured == result || result == mloc_errorCapacity);
        return;
    }
    Require(measured == mloc_errorCapacity && measure.length == out.length);
    Require(Utf8((const uint8_t*)s_out, out.length));
    Tiling tiling = {0, 0, true};
    mlocOutput again = {s_again, sizeof(s_again), 0, Tile, &tiling};
    Require((text != nullptr ? mlocInflectText(f, text, length, r, &again)
                             : mlocInflectNumber(f, nullptr, number, r, &again)) == mloc_success);
    Require(again.length == out.length && memcmp(s_again, s_out, out.length) == 0);
    Require(tiling.good && tiling.next == out.length);
    // The text begins the output, but a stem's changed end (a common
    // noun's, a Kazakh, Kyrgyz, Uzbek, Turkmen or Hungarian name's): its final stop, its
    // dropped or lengthened vowel and what follows it, or a compound's possessive, at most 8
    // bytes.
    if (text != nullptr)
    {
        size_t kept = !r->commonNoun && !stemsNames ? length : length >= 8 ? length - 8 : 0;
        Require(out.length >= kept && memcmp(s_out, text, kept) == 0);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 3)
    {
        return 0;
    }
    size_t language = (size_t)(data[1] >> 4 & 7);
    bool stemsNames = s_stemsNames[language];
    const mlocInflector* f = Inflector(language);
    mlocInflection r = RequestOf(data);
    // Finnish has no dative, locative or vocative: its partitive, abessive
    // and comitative take their places.
    if (language == 7)
    {
        r.grammaticalCase = r.grammaticalCase == mloc_caseDative     ? mloc_casePartitive
                            : r.grammaticalCase == mloc_caseLocative ? mloc_caseAbessive
                            : r.grammaticalCase == mloc_caseVocative ? mloc_caseComitative
                                                                     : r.grammaticalCase;
    }
    const char* text = (const char*)data + 3;
    size_t length = size - 3;
    size_t split = data[2];
    if (split > 0 && split < length)
    {
        r.hint = text + split;
        r.hintLength = length - split;
        length = split;
    }
    Check(f, stemsNames, text, length, nullptr, &r);
    mlocDecimal number;
    if (mlocParseDecimal(text, length, &number).status == mloc_success)
    {
        Check(f, stemsNames, nullptr, 0, &number, &r);
    }
    return 0;
}
