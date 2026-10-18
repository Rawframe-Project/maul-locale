// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Grammatical suffixes (maul-locale/grammar.h, record mloc-0046): Turkish
// cases, plural, possessives, copula and question particle on names,
// common nouns, numbers and abbreviations as the TDK spelling guide
// writes them; the apostrophe, the readings and hints, the parts and the
// refusals.

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "test_harness.h"

#include "maul-locale/grammar.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocDataSet s_builtin;
static mlocInflector s_tr;
static char s_text[512];

// A request for a case alone.
static mlocInflection Case(mlocGrammaticalCase grammaticalCase)
{
    mlocInflection r;
    memset(&r, 0, sizeof(r));
    r.grammaticalCase = grammaticalCase;
    return r;
}

// Whether a text inflects as asked to the text expected.
static bool Gives(const char* text, const mlocInflection* request, const char* expected)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocInflectText(&s_tr, text, strlen(text), request, &output);
    bool good = r == mloc_success && output.length == strlen(expected) &&
                memcmp(s_text, expected, output.length) == 0;
    if (!good)
    {
        printf("  [%s] -> %d [%.*s], not [%s]\n", text, (int)r, (int)output.length, s_text,
               expected);
    }
    return good;
}

// Whether a text takes the six cases' suffixes given, genitive, dative,
// accusative, locative, ablative and instrumental.
static bool Cases(const char* text, const char* const forms[6])
{
    static const mlocGrammaticalCase s_cases[6] = {mloc_caseGenitive,   mloc_caseDative,
                                                   mloc_caseAccusative, mloc_caseLocative,
                                                   mloc_caseAblative,   mloc_caseInstrumental};
    bool good = true;
    for (int i = 0; i < 6; i++)
    {
        mlocInflection r = Case(s_cases[i]);
        good = Gives(text, &r, forms[i]) && good;
    }
    return good;
}

static void TestNames(void)
{
    CHECK(Cases("Ay\xC5\x9F"
                "e",
                (const char* const[]){"Ay\xC5\x9F"
                                      "e'nin",
                                      "Ay\xC5\x9F"
                                      "e'ye",
                                      "Ay\xC5\x9F"
                                      "e'yi",
                                      "Ay\xC5\x9F"
                                      "e'de",
                                      "Ay\xC5\x9F"
                                      "e'den",
                                      "Ay\xC5\x9F"
                                      "e'yle"}) &&
              Cases("Ahmet", (const char* const[]){"Ahmet'in", "Ahmet'e", "Ahmet'i", "Ahmet'te",
                                                   "Ahmet'ten", "Ahmet'le"}) &&
              Cases("Ankara",
                    (const char* const[]){"Ankara'n\xC4\xB1n", "Ankara'ya", "Ankara'y\xC4\xB1",
                                          "Ankara'da", "Ankara'dan", "Ankara'yla"}) &&
              Cases("O\xC4\x9Fuz",
                    (const char* const[]){"O\xC4\x9Fuz'un", "O\xC4\x9Fuz'a", "O\xC4\x9Fuz'u",
                                          "O\xC4\x9Fuz'da", "O\xC4\x9Fuz'dan", "O\xC4\x9Fuz'la"}) &&
              Cases("\xC3\x9Cmit",
                    (const char* const[]){"\xC3\x9Cmit'in", "\xC3\x9Cmit'e", "\xC3\x9Cmit'i",
                                          "\xC3\x9Cmit'te", "\xC3\x9Cmit'ten", "\xC3\x9Cmit'le"}) &&
              Cases("\xC4\xB0stanbul",
                    (const char* const[]){"\xC4\xB0stanbul'un", "\xC4\xB0stanbul'a",
                                          "\xC4\xB0stanbul'u", "\xC4\xB0stanbul'da",
                                          "\xC4\xB0stanbul'dan", "\xC4\xB0stanbul'la"}),
          "names take each case by harmony, d to t and the buffer consonants");
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection genitive = Case(mloc_caseGenitive);
    CHECK(Gives("Burak", &genitive, "Burak'\xC4\xB1n") && Gives("Rodrigo", &dative, "Rodrigo'ya") &&
              Gives("Mehmet Ali", &dative, "Mehmet Ali'ye"),
          "a name keeps its final stop; the last word decides");
    mlocInflection plural = dative;
    plural.plural = true;
    CHECK(Gives("Ahmet", &plural, "Ahmetlere") && Gives("Ay\xC5\x9F"
                                                        "e",
                                                        &plural,
                                                        "Ay\xC5\x9F"
                                                        "elere"),
          "a name's plural is joined without an apostrophe, as TDK writes it");
    mlocInflection foreign = Case(mloc_caseNominative);
    foreign.plural = true;
    foreign.hint = "nis";
    foreign.hintLength = 3;
    CHECK(Gives("Nice", &foreign, "Nice'ler"), "a foreign name, hinted, keeps its apostrophe");
    mlocInflection own = Case(mloc_caseNominative);
    own.possessorPerson = 3;
    mlocInflection ownDative = dative;
    ownDative.possessorPerson = 3;
    mlocInflection mine = Case(mloc_caseNominative);
    mine.possessorPerson = 1;
    mlocInflection yours = Case(mloc_caseNominative);
    yours.possessorPerson = 2;
    yours.possessorPlural = true;
    CHECK(Gives("Ali", &own, "Ali'si") && Gives("Ahmet", &own, "Ahmet'i") &&
              Gives("Ali", &ownDative, "Ali'sine") && Gives("Ahmet", &ownDative, "Ahmet'ine") &&
              Gives("Ali", &mine, "Ali'm") && Gives("Ahmet", &mine, "Ahmet'im") &&
              Gives("Ali", &yours, "Ali'niz") && Gives("Ahmet", &yours, "Ahmet'iniz"),
          "possessives, and the n a third person's puts before a case");
    mlocInflection is = Case(mloc_caseNominative);
    is.copula = true;
    CHECK(Gives("Ali", &is, "Ali'dir") && Gives("Ahmet", &is, "Ahmet'tir") &&
              Gives("Ankara", &is, "Ankara'd\xC4\xB1r") &&
              Gives("O\xC4\x9Fuz", &is, "O\xC4\x9Fuz'dur"),
          "the copula");
    mlocInflection question = Case(mloc_caseNominative);
    question.question = true;
    mlocInflection dativeQuestion = dative;
    dativeQuestion.question = true;
    CHECK(Gives("Ali", &question, "Ali mi") && Gives("O\xC4\x9Fuz", &question, "O\xC4\x9Fuz mu") &&
              Gives("Ankara", &dativeQuestion, "Ankara'ya m\xC4\xB1") &&
              Gives("\xC3\x9Cmit", &dativeQuestion, "\xC3\x9Cmit'e mi"),
          "the question particle, a word of its own");
    mlocInflection none = Case(mloc_caseNominative);
    CHECK(Gives("Ahmet", &none, "Ahmet"), "nothing asked, nothing added");
}

static void TestNumbers(void)
{
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection locative = Case(mloc_caseLocative);
    mlocInflection genitive = Case(mloc_caseGenitive);
    CHECK(Gives("5", &dative, "5'e") && Gives("5", &locative, "5'te") &&
              Gives("6", &dative, "6'ya") && Gives("6", &genitive, "6'n\xC4\xB1n") &&
              Gives("1985", &locative, "1985'te") && Gives("10", &genitive, "10'un") &&
              Gives("40", &locative, "40'ta") && Gives("100", &genitive, "100'\xC3\xBCn") &&
              Gives("1000", &dative, "1000'e") && Gives("4", &genitive, "4'\xC3\xBCn") &&
              Gives("0", &dative, "0'a") && Gives("60", &locative, "60'ta") &&
              Gives("1000000", &genitive, "1000000'un"),
          "a number by the last word it is spoken with");
    CHECK(Gives("1.985", &locative, "1.985'te") && Gives("2,5", &dative, "2,5'e") &&
              Gives("2,20", &dative, "2,20'ye") && Gives("%50", &dative, "%50'ye") &&
              Gives("MP3", &dative, "MP3'e") && Gives("5.", &dative, "5.'ye"),
          "grouped, fractions read as their digits' number, a percent, an ordinal");
    mlocInflection plural = locative;
    plural.plural = true;
    CHECK(Gives("1990", &plural, "1990'larda"), "a number's plural keeps the apostrophe");
    CHECK(Gives("9", &dative, "9'a") && Gives("1999", &locative, "1999'da") &&
              Gives("1.000", &dative, "1.000'e"),
          "a nine, and a grouped thousand read whole");
    CHECK(Gives("1.2.10", &dative, "1.2.10'a") && Gives("1,000.5", &dative, "1,000.5'e") &&
              Gives("iOS 17.4", &dative, "iOS 17.4'e"),
          "a version and a number written otherwise than the locale writes them, by their last "
          "digits");
}

static void TestAbbreviations(void)
{
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection genitive = Case(mloc_caseGenitive);
    CHECK(Gives("TDK", &dative, "TDK'ye") && Gives("TDK", &genitive, "TDK'nin") &&
              Gives("ABD", &dative, "ABD'ye") && Gives("TBMM", &genitive, "TBMM'nin") &&
              Gives("AB", &dative, "AB'ye") && Gives("PTT", &dative, "PTT'ye") &&
              Gives("PwC", &dative, "PwC'ye"),
          "capitals by their last letter's name; a word without a vowel by its letters");
    mlocInflection lettersDative = dative;
    lettersDative.reading = mloc_readingLetters;
    CHECK(Gives("AZ", &dative, "AZ'ye") && Gives("xyz", &lettersDative, "xyz'ye"),
          "the last letter of the alphabet, as a capital and as a small letter");
    mlocInflection word = dative;
    word.reading = mloc_readingWord;
    mlocInflection letters = dative;
    letters.reading = mloc_readingLetters;
    CHECK(Gives("NATO", &word, "NATO'ya") && Gives("Nato", &letters, "Nato'ya"),
          "the reading says which");
    mlocInflection hinted = dative;
    hinted.hint = "jak";
    hinted.hintLength = 3;
    mlocInflection hintedLocative = Case(mloc_caseLocative);
    hintedLocative.hint = "jak";
    hintedLocative.hintLength = 3;
    CHECK(Gives("Jacques", &hinted, "Jacques'a") &&
              Gives("Jacques", &hintedLocative, "Jacques'ta") &&
              Gives("Harry", &dative, "Harry'ye") && Gives("Kennedy", &genitive, "Kennedy'nin"),
          "a hint read instead of the text; a final y after a consonant said as i");
}

static void TestCommonNouns(void)
{
    mlocInflection accusative = Case(mloc_caseAccusative);
    accusative.commonNoun = true;
    mlocInflection dative = Case(mloc_caseDative);
    dative.commonNoun = true;
    mlocInflection locative = Case(mloc_caseLocative);
    locative.commonNoun = true;
    CHECK(Gives("kitap", &accusative, "kitab\xC4\xB1") && Gives("kitap", &dative, "kitaba") &&
              Gives("kitap", &locative, "kitapta") &&
              Gives("a\xC4\x9F"
                    "a\xC3\xA7",
                    &accusative,
                    "a\xC4\x9F"
                    "ac\xC4\xB1") &&
              Gives("k\xC3\xB6pek", &dative,
                    "k\xC3\xB6pe\xC4\x9F"
                    "e") &&
              Gives("ev", &dative, "eve") && Gives("ev", &locative, "evde"),
          "a common noun: no apostrophe, its final stop softened before a vowel");
    CHECK(Gives("top", &accusative, "topu") && Gives("ahenk", &accusative, "ahengi"),
          "one syllable keeps its stop; nk softens to ng");
    CHECK(Gives("saat", &dative, "saate") && Gives("saat", &accusative, "saati") &&
              Gives("rol", &accusative, "rol\xC3\xBC") && Gives("alkol", &dative, "alkole") &&
              Gives("harf", &accusative, "harfi"),
          "the loanwords that break harmony");
    mlocInflection mine = Case(mloc_caseNominative);
    mine.commonNoun = true;
    mine.possessorPerson = 1;
    mlocInflection theirs = Case(mloc_caseNominative);
    theirs.commonNoun = true;
    theirs.plural = true;
    theirs.possessorPerson = 3;
    theirs.possessorPlural = true;
    CHECK(Gives("kitap", &mine, "kitab\xC4\xB1m") && Gives("kitap", &theirs, "kitaplar\xC4\xB1"),
          "possessives on a common noun; a plural's third person plural written once");
    mlocInflection quoted = Case(mloc_caseDative);
    // A period's abbreviation is read as its word, which only a hint says.
    mlocInflection doctor = quoted;
    doctor.hint = "doktor";
    doctor.hintLength = 6;
    CHECK(Gives("\xE2\x80\x9CHamlet\xE2\x80\x9D", &quoted,
                "\xE2\x80\x9CHamlet\xE2\x80\x9D"
                "e") &&
              Gives("Dr.", &doctor, "Dr.a"),
          "none after a closing quotation mark or a final period");
}

// The parts an inflection reported.
typedef struct Parts
{
    uint32_t kinds[8];
    size_t starts[8];
    size_t lengths[8];
    size_t count;
} Parts;

static void Collect(void* context, uint32_t part, size_t start, size_t length)
{
    Parts* p = context;
    if (p->count < 8)
    {
        p->kinds[p->count] = part;
        p->starts[p->count] = start;
        p->lengths[p->count] = length;
    }
    p->count++;
}

static void TestParts(void)
{
    Parts parts = {{0}, {0}, {0}, 0};
    mlocOutput output = {s_text, sizeof(s_text), 0, Collect, &parts};
    mlocInflection r = Case(mloc_caseDative);
    r.question = true;
    CHECK(mlocInflectText(&s_tr, "Ahmet", 5, &r, &output) == mloc_success && output.length == 10 &&
              memcmp(s_text, "Ahmet'e mi", 10) == 0 && parts.count == 4 &&
              parts.kinds[0] == mloc_inflectionPartStem && parts.lengths[0] == 5 &&
              parts.kinds[1] == mloc_inflectionPartApostrophe && parts.starts[1] == 5 &&
              parts.kinds[2] == mloc_inflectionPartSuffix && parts.starts[2] == 6 &&
              parts.kinds[3] == mloc_inflectionPartParticle && parts.starts[3] == 7 &&
              parts.lengths[3] == 3,
          "the stem, apostrophe, suffix and particle as parts");
    mlocDecimal year;
    CHECK(mlocParseDecimal("1985", 4, &year).status == mloc_success, "a decimal is read");
    parts.count = 0;
    output.length = 0;
    mlocInflection locative = Case(mloc_caseLocative);
    CHECK(mlocInflectNumber(&s_tr, nullptr, &year, &locative, &output) == mloc_success &&
              output.length == 8 && memcmp(s_text, "1.985'te", 8) == 0 && parts.count == 6 &&
              parts.kinds[0] == mloc_inflectionPartStem && parts.lengths[0] == 5 &&
              parts.kinds[1] == mloc_numberPartInteger && parts.lengths[1] == 1 &&
              parts.kinds[2] == mloc_numberPartGroup && parts.starts[2] == 1 &&
              parts.kinds[3] == mloc_numberPartInteger && parts.starts[3] == 2 &&
              parts.kinds[4] == mloc_inflectionPartApostrophe && parts.starts[4] == 5 &&
              parts.kinds[5] == mloc_inflectionPartSuffix && parts.starts[5] == 6 &&
              parts.lengths[5] == 2,
          "a number formatted by the locale and inflected, its own parts in the stem");
    mlocNumberFormatter percent;
    mlocNumberOptions options = {.style = mloc_numberPercent};
    mlocDecimal half;
    mlocLocale tr;
    mlocInflection dative = Case(mloc_caseDative);
    output.part = nullptr;
    CHECK(mlocParseDecimal("0.5", 3, &half).status == mloc_success &&
              mlocParseLocale("tr", 2, &tr).status == mloc_success &&
              mlocInitNumberFormatter(&percent, &s_builtin, &tr, &options) == mloc_success &&
              mlocInflectNumber(&s_tr, &percent, &half, &dative, &output) == mloc_success &&
              output.length == 6 && memcmp(s_text, "%50'ye", 6) == 0,
          "a percent by its own formatter, read as it is written");
}

static void TestRefusals(void)
{
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection vocative = Case(mloc_caseVocative);
    mlocInflection fourth = Case(mloc_caseNominative);
    fourth.possessorPerson = 4;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_tr, "", 0, &dative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, "\xC3", 1, &dative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, "!!", 2, &dative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, "Ali", 3, &vocative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, "Ali", 3, &fourth, &output) == mloc_errorInvalid &&
              mlocInflectText(nullptr, "Ali", 3, &dative, &output) == mloc_errorInvalid,
          "empty text, bad UTF-8, nothing to read, a case or person Turkish lacks");
    CHECK(mlocInflectText(&s_tr, "Ali\xF0\x9F\x98\x80", 7, &dative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, "Ali!", 4, &dative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, "\xE2\x80\x9D", 3, &dative, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tr, ".", 1, &dative, &output) == mloc_errorInvalid,
          "an emoji, punctuation, a closing mark or a period alone end nothing to read");
    static const char s_long[] = "12345678901234567890123456789012345678901234567890";
    CHECK(mlocInflectText(&s_tr, s_long, sizeof(s_long) - 1, &dative, &output) == mloc_errorInvalid,
          "a number past a decimal's 40 digits is refused");
    static char s_word[301];
    memset(s_word, 'a', 300);
    output.length = 0;
    CHECK(mlocInflectText(&s_tr, s_word, 300, &dative, &output) == mloc_success &&
              output.length == 303 && memcmp(s_text + 300, "'ya", 3) == 0 && s_text[299] == 'a',
          "a word past the spoken room is read by its last letters");
    mlocOutput measure = {nullptr, 0, 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_tr, "Ali", 3, &dative, &measure) == mloc_errorCapacity &&
              measure.length == 6,
          "a null buffer of no capacity measures");
    mlocDecimal five;
    mlocDecimal huge;
    CHECK(mlocParseDecimal("5", 1, &five).status == mloc_success &&
              mlocParseDecimal("1e2000", 6, &huge).status == mloc_success &&
              mlocInflectNumber(&s_tr, nullptr, &five, &dative, nullptr) == mloc_errorInvalid &&
              mlocInflectNumber(&s_tr, nullptr, &huge, &dative, &output) == mloc_errorInvalid,
          "a number without an output, or whose text passes 1,024 bytes, is refused");
    mlocOutput small = {s_text, 4, 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_tr, "Ankara", 6, &dative, &small) == mloc_errorCapacity &&
              small.length == 9,
          "a text that does not fit reports its length");
    mlocInflector other;
    mlocLocale en;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocInitInflector(&other, &s_data, &en) == mloc_errorNotFound,
          "a language without rules is refused");
    mlocInflector bare;
    mlocLocale tr;
    CHECK(mlocParseLocale("tr", 2, &tr).status == mloc_success &&
              mlocInitInflector(&bare, &s_builtin, &tr) == mloc_success &&
              mlocInflectText(&bare, "5", 1, &dative, &output) == mloc_errorNotFound &&
              mlocInflectText(&bare, "Ali", 3, &dative, &output) == mloc_success,
          "without the spell-out rules a number is refused, a word is not");
}

int main(void)
{
    static mlocData sources[2];
    alignas(8) static uint8_t spellout[8 << 20];
    mlocOpenBuiltinData(&sources[0]);
    FILE* file = fopen(MLOC_SPELLOUT_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(spellout, 1, sizeof(spellout), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    if (size == 0 || mlocOpenData(&sources[1], spellout, size) != mloc_success)
    {
        fprintf(stderr, "test_grammar: cannot open %s\n", MLOC_SPELLOUT_FILE);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    s_builtin = (mlocDataSet){sources, 1};
    mlocLocale tr;
    if (mlocParseLocale("tr", 2, &tr).status != mloc_success ||
        mlocInitInflector(&s_tr, &s_data, &tr) != mloc_success)
    {
        fputs("test_grammar: cannot make the inflector\n", stderr);
        return 2;
    }
    TestNames();
    TestNumbers();
    TestAbbreviations();
    TestCommonNouns();
    TestParts();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
