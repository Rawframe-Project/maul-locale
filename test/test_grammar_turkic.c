// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The other Turkic languages' suffixes (maul-locale/grammar.h, record
// mloc-0047): Kazakh cases, plural, possessives and question particle
// on names, common nouns, numbers and abbreviations, as apertium-kaz
// generates them; its hyphen, its stems, and its refusals.

#include "test_harness.h"

#include "maul-locale/grammar.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocInflector s_kk;
static char s_text[512];

static mlocInflection Case(mlocGrammaticalCase grammaticalCase)
{
    mlocInflection r;
    memset(&r, 0, sizeof(r));
    r.grammaticalCase = grammaticalCase;
    return r;
}

static mlocInflection Common(mlocGrammaticalCase grammaticalCase)
{
    mlocInflection r = Case(grammaticalCase);
    r.commonNoun = true;
    return r;
}

static bool Gives(const mlocInflector* inflector, const char* text, const mlocInflection* request,
                  const char* expected)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocInflectText(inflector, text, strlen(text), request, &output);
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
static bool Cases(const char* text, bool common, const char* const forms[6])
{
    static const mlocGrammaticalCase s_cases[6] = {mloc_caseGenitive,   mloc_caseDative,
                                                   mloc_caseAccusative, mloc_caseLocative,
                                                   mloc_caseAblative,   mloc_caseInstrumental};
    bool good = true;
    for (int i = 0; i < 6; i++)
    {
        mlocInflection r = common ? Common(s_cases[i]) : Case(s_cases[i]);
        good = Gives(&s_kk, text, &r, forms[i]) && good;
    }
    return good;
}

static void TestKazakhCases(void)
{
    CHECK(Cases("Алматы", false,
                (const char* const[]){"Алматының", "Алматыға", "Алматыны", "Алматыда", "Алматыдан",
                                      "Алматымен"}) &&
              Cases("Асқар", false,
                    (const char* const[]){"Асқардың", "Асқарға", "Асқарды", "Асқарда", "Асқардан",
                                          "Асқармен"}) &&
              Cases("Айгүл", false,
                    (const char* const[]){"Айгүлдің", "Айгүлге", "Айгүлді", "Айгүлде", "Айгүлден",
                                          "Айгүлмен"}) &&
              Cases("Адам", false,
                    (const char* const[]){"Адамның", "Адамға", "Адамды", "Адамда", "Адамнан",
                                          "Адаммен"}),
          "names joined, the onset by the last sound: a vowel, a glide, l, a nasal");
    CHECK(Cases("кітап", true,
                (const char* const[]){"кітаптың", "кітапқа", "кітапты", "кітапта", "кітаптан",
                                      "кітаппен"}) &&
              Cases("тәж", true,
                    (const char* const[]){"тәжтің", "тәжке", "тәжті", "тәжте", "тәжтен", "тәжпен"}),
          "after a voiceless consonant, and zh read as one");
    mlocInflection plural = Common(mloc_caseNominative);
    plural.plural = true;
    mlocInflection instrumental = Common(mloc_caseInstrumental);
    mlocInflection genitive = Common(mloc_caseGenitive);
    CHECK(Gives(&s_kk, "кітап", &plural, "кітаптар") && Gives(&s_kk, "тау", &plural, "таулар") &&
              Gives(&s_kk, "көл", &plural, "көлдер") && Gives(&s_kk, "қыз", &plural, "қыздар") &&
              Gives(&s_kk, "тәж", &plural, "тәжтер") &&
              Gives(&s_kk, "қыз", &instrumental, "қызбен") &&
              Gives(&s_kk, "тау", &genitive, "таудың"),
          "the plural's l, d, t; the instrumental's b after z");
}

static void TestKazakhPossessives(void)
{
    mlocInflection r = Common(mloc_caseNominative);
    r.possessorPerson = 1;
    bool good = Gives(&s_kk, "кітап", &r, "кітабым") && Gives(&s_kk, "бала", &r, "балам");
    r.possessorPerson = 3;
    good = Gives(&s_kk, "кітап", &r, "кітабы") && Gives(&s_kk, "бала", &r, "баласы") && good;
    r.possessorPerson = 1;
    r.possessorPlural = true;
    good = Gives(&s_kk, "бала", &r, "баламыз") && good;
    r.possessorPerson = 2;
    good = Gives(&s_kk, "бала", &r, "балаңдар") && good;
    CHECK(good, "the possessives, the stop voiced before a vowel");
    mlocInflection mine = Common(mloc_caseDative);
    mine.possessorPerson = 1;
    mlocInflection mineOf = Common(mloc_caseGenitive);
    mineOf.possessorPerson = 1;
    mlocInflection oursOf = mineOf;
    oursOf.possessorPlural = true;
    mlocInflection yours = Common(mloc_caseDative);
    yours.possessorPerson = 2;
    yours.possessorPlural = true;
    CHECK(Gives(&s_kk, "кітап", &mine, "кітабыма") &&
              Gives(&s_kk, "кітап", &mineOf, "кітабымның") &&
              Gives(&s_kk, "кітап", &oursOf, "кітабымыздың") &&
              Gives(&s_kk, "кітап", &yours, "кітабыңдарға"),
          "after a first or second person: the dative without its onset, the genitive's n");
    static const mlocGrammaticalCase s_cases[5] = {mloc_caseDative, mloc_caseAccusative,
                                                   mloc_caseLocative, mloc_caseAblative,
                                                   mloc_caseInstrumental};
    static const char* const s_forms[5] = {"кітабына", "кітабын", "кітабында", "кітабынан",
                                           "кітабымен"};
    good = true;
    for (int i = 0; i < 5; i++)
    {
        mlocInflection third = Common(s_cases[i]);
        third.possessorPerson = 3;
        good = Gives(&s_kk, "кітап", &third, s_forms[i]) && good;
    }
    mlocInflection theirs = Common(mloc_caseNominative);
    theirs.plural = true;
    theirs.possessorPerson = 3;
    CHECK(good && Gives(&s_kk, "кітап", &theirs, "кітаптары"),
          "after a third person: the pronominal n; a plural before it");
}

static void TestKazakhWords(void)
{
    mlocInflection third = Common(mloc_caseNominative);
    third.possessorPerson = 3;
    mlocInflection dative = Common(mloc_caseDative);
    mlocInflection genitive = Common(mloc_caseGenitive);
    mlocInflection plural = Common(mloc_caseNominative);
    plural.plural = true;
    mlocInflection instrumental = Common(mloc_caseInstrumental);
    CHECK(Gives(&s_kk, "парк", &dative, "паркке") &&
              Gives(&s_kk, "автомобиль", &third, "автомобилі") &&
              Gives(&s_kk, "повесть", &genitive, "повестьнің") &&
              Gives(&s_kk, "повесть", &plural, "повестьлер") &&
              Gives(&s_kk, "князь", &genitive, "князьдің") &&
              Gives(&s_kk, "князь", &instrumental, "князьмен") &&
              Gives(&s_kk, "жүрек", &third, "жүрегі"),
          "loans: a final k makes the suffixes front, a soft sign after a voiceless consonant "
          "is said as a vowel and after another as l, and is dropped before a vowel");
    mlocInflection dativeWord = Common(mloc_caseDative);
    CHECK(Gives(&s_kk, "акведук", &third, "акведугі") && Gives(&s_kk, "ту", &dativeWord, "туға") &&
              Gives(&s_kk, "бетъ", &dativeWord, "бетъке"),
          "a stop voiced after a glide; a word whose only vowel is a glide; a final hard sign "
          "read past");
    mlocInflection name = Case(mloc_caseNominative);
    name.possessorPerson = 3;
    CHECK(Gives(&s_kk, "Айтбек", &name, "Айтбегі"), "a name's stop is voiced too");
    mlocInflection question = Case(mloc_caseNominative);
    question.question = true;
    mlocInflection dativeQuestion = Case(mloc_caseDative);
    dativeQuestion.question = true;
    CHECK(Gives(&s_kk, "кітап", &question, "кітап па") &&
              Gives(&s_kk, "Адам", &question, "Адам ба") &&
              Gives(&s_kk, "көл", &question, "көл ме") &&
              Gives(&s_kk, "қыз", &question, "қыз ба") &&
              Gives(&s_kk, "Алматы", &dativeQuestion, "Алматыға ма"),
          "the question particle: ma, ba, pa by the last sound, a word of its own");
}

static void TestKazakhMarks(void)
{
    mlocInflection genitive = Case(mloc_caseGenitive);
    mlocInflection dative = Case(mloc_caseDative);
    CHECK(Gives(&s_kk, "ТМД", &genitive, "ТМД-ның") && Gives(&s_kk, "ҚР", &dative, "ҚР-ға") &&
              Gives(&s_kk, "ЖОО", &genitive, "ЖОО-ның"),
          "an abbreviation by its letters' names, a consonant read with ы, a hyphen before "
          "the suffix");
    mlocInflection letters = Case(mloc_caseDative);
    letters.reading = mloc_readingLetters;
    CHECK(Gives(&s_kk, "я", &letters, "я-ға") && Gives(&s_kk, "і", &letters, "і-ге") &&
              Gives(&s_kk, "қ", &letters, "қ-ға") && Gives(&s_kk, "ё", &letters, "ё-ға"),
          "lowercase letters read by their names");
    CHECK(Gives(&s_kk, "5", &dative, "5-ке") && Gives(&s_kk, "2020", &dative, "2020-ға") &&
              Gives(&s_kk, "10", &genitive, "10-ның"),
          "digits by their spoken last word, a hyphen before the suffix");
    CHECK(Gives(&s_kk, "Google", &dative, "Google-ге"), "a word in the Latin script, a hyphen");
    mlocInflection common = Common(mloc_caseDative);
    CHECK(Gives(&s_kk, "Google", &common, "Googleге"), "a common noun takes no mark");
    mlocDecimal number;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocParseDecimal("1234", 4, &number).status == mloc_success &&
              mlocInflectNumber(&s_kk, nullptr, &number, &dative, &output) == mloc_success &&
              output.length == 11 &&
              memcmp(s_text,
                     "1\xC2\xA0"
                     "234-ке",
                     11) == 0,
          "a number formatted by the locale, by its spoken last word");
}

static void TestKazakhRefusals(void)
{
    mlocInflection copula = Case(mloc_caseNominative);
    copula.copula = true;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_kk, "Алматы", strlen("Алматы"), &copula, &output) == mloc_errorInvalid,
          "no copula suffix in Kazakh");
    mlocLocale latin;
    mlocInflector inflector;
    CHECK(mlocParseLocale("kk-Latn", 7, &latin).status == mloc_success &&
              mlocInitInflector(&inflector, &s_data, &latin) == mloc_errorNotFound,
          "Kazakh in another script than Cyrillic is refused");
    mlocLocale cyrillic;
    CHECK(mlocParseLocale("kk-Cyrl-KZ", 10, &cyrillic).status == mloc_success &&
              mlocInitInflector(&inflector, &s_data, &cyrillic) == mloc_success,
          "Kazakh in Cyrillic named");
    mlocInflection letters = Case(mloc_caseDative);
    letters.reading = mloc_readingLetters;
    CHECK(mlocInflectText(&s_kk, "Ь", strlen("Ь"), &letters, &output) == mloc_errorInvalid,
          "a sign has no name to read");
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
        fprintf(stderr, "test_grammar_turkic: cannot open %s\n", MLOC_SPELLOUT_FILE);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    mlocLocale kk;
    if (mlocParseLocale("kk", 2, &kk).status != mloc_success ||
        mlocInitInflector(&s_kk, &s_data, &kk) != mloc_success)
    {
        fputs("test_grammar_turkic: cannot make the inflector\n", stderr);
        return 2;
    }
    TestKazakhCases();
    TestKazakhPossessives();
    TestKazakhWords();
    TestKazakhMarks();
    TestKazakhRefusals();
    return s_failures == 0 ? 0 : 1;
}
