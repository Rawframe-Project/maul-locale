// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The other Turkic languages' suffixes (maul-locale/grammar.h): Kazakh,
// Kyrgyz, Azerbaijani, Uzbek and Turkmen cases, plural, possessives and
// question particle on names, common nouns, numbers and abbreviations, as
// apertium-kaz, apertium-kir and apertium-tuk generate them and the
// Azerbaijani norms and Uzbek rules write them; their marks, their stems
// and their refusals.

#include "test_harness.h"

#include "maul-locale/grammar.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocInflector s_kk;
static mlocInflector s_ky;
static mlocInflector s_az;
static mlocInflector s_uz;
static mlocInflector s_tk;
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

// Whether a text takes the cases' suffixes given, genitive, dative,
// accusative, locative, ablative and (count 6) instrumental.
static bool CasesOf(const mlocInflector* inflector, const char* text, bool common,
                    const char* const* forms, int count)
{
    static const mlocGrammaticalCase s_cases[6] = {mloc_caseGenitive,   mloc_caseDative,
                                                   mloc_caseAccusative, mloc_caseLocative,
                                                   mloc_caseAblative,   mloc_caseInstrumental};
    bool good = true;
    for (int i = 0; i < count; i++)
    {
        mlocInflection r = common ? Common(s_cases[i]) : Case(s_cases[i]);
        good = Gives(inflector, text, &r, forms[i]) && good;
    }
    return good;
}

static bool Cases(const char* text, bool common, const char* const forms[6])
{
    return CasesOf(&s_kk, text, common, forms, 6);
}

// Kyrgyz has no instrumental case: five forms.
static bool KyrgyzCases(const char* text, bool common, const char* const forms[5])
{
    return CasesOf(&s_ky, text, common, forms, 5);
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

static void TestKyrgyzCases(void)
{
    CHECK(KyrgyzCases("тоо", true,
                      (const char* const[]){"тоонун", "тоого", "тоону", "тоодо", "тоодон"}) &&
              KyrgyzCases("кул", true,
                          (const char* const[]){"кулдун", "кулга", "кулду", "кулда", "кулдан"}) &&
              KyrgyzCases("көл", true,
                          (const char* const[]){"көлдүн", "көлгө", "көлдү", "көлдө", "көлдөн"}) &&
              KyrgyzCases(
                  "китеп", true,
                  (const char* const[]){"китептин", "китепке", "китепти", "китепте", "китептен"}) &&
              KyrgyzCases("кар", true,
                          (const char* const[]){"кардын", "карга", "карды", "карда", "кардан"}),
          "four-way harmony, A unrounded after у; the onsets by the last sound");
    mlocInflection plural = Common(mloc_caseNominative);
    plural.plural = true;
    CHECK(Gives(&s_ky, "тоо", &plural, "тоолор") && Gives(&s_ky, "кул", &plural, "кулдар") &&
              Gives(&s_ky, "көл", &plural, "көлдөр") && Gives(&s_ky, "үй", &plural, "үйлөр") &&
              Gives(&s_ky, "булут", &plural, "булуттар") && Gives(&s_ky, "ай", &plural, "айлар"),
          "the plural's l, d, t and four vowels");
}

static void TestKyrgyzPossessives(void)
{
    mlocInflection r = Common(mloc_caseNominative);
    r.possessorPerson = 1;
    bool good = Gives(&s_ky, "китеп", &r, "китебим") && Gives(&s_ky, "той", &r, "тоюм");
    r.possessorPerson = 3;
    good = Gives(&s_ky, "китеп", &r, "китеби") && Gives(&s_ky, "тоо", &r, "тоосу") && good;
    r.possessorPerson = 1;
    r.possessorPlural = true;
    good = Gives(&s_ky, "бала", &r, "балабыз") && Gives(&s_ky, "китеп", &r, "китебибиз") && good;
    r.possessorPerson = 2;
    good = Gives(&s_ky, "тоо", &r, "тооңор") && Gives(&s_ky, "көл", &r, "көлүңөр") && good;
    CHECK(good, "the possessives; й and у written ю");
    mlocInflection mine = Common(mloc_caseDative);
    mine.possessorPerson = 1;
    mlocInflection mineOf = Common(mloc_caseGenitive);
    mineOf.possessorPerson = 1;
    mlocInflection ours = Common(mloc_caseDative);
    ours.possessorPerson = 1;
    ours.possessorPlural = true;
    mlocInflection third = Common(mloc_caseDative);
    third.possessorPerson = 3;
    CHECK(Gives(&s_ky, "китеп", &mine, "китебиме") && Gives(&s_ky, "тоо", &mine, "тоомо") &&
              Gives(&s_ky, "бала", &mineOf, "баламдын") &&
              Gives(&s_ky, "бала", &ours, "балабызга") && Gives(&s_ky, "тоо", &third, "тоосуна") &&
              Gives(&s_ky, "той", &third, "тоюна"),
          "the cases after a possessive");
}

static void TestKyrgyzWords(void)
{
    mlocInflection genitive = Common(mloc_caseGenitive);
    mlocInflection dative = Common(mloc_caseDative);
    mlocInflection third = Common(mloc_caseNominative);
    third.possessorPerson = 3;
    CHECK(Gives(&s_ky, "князь", &genitive, "княздын") &&
              Gives(&s_ky, "лагерь", &dative, "лагерге") &&
              Gives(&s_ky, "автомобиль", &third, "автомобили") &&
              Gives(&s_ky, "ми", &genitive, "минин"),
          "a soft sign dropped before any suffix; и a front vowel");
    mlocInflection nameDative = Case(mloc_caseDative);
    mlocInflection name = Case(mloc_caseNominative);
    name.possessorPerson = 3;
    CHECK(Gives(&s_ky, "Бишкек", &nameDative, "Бишкекке") && Gives(&s_ky, "Айбек", &name, "Айбеги"),
          "names joined, their stop voiced");
    mlocInflection nameGenitive = Case(mloc_caseGenitive);
    mlocInflection ablative = Case(mloc_caseAblative);
    CHECK(Gives(&s_ky, "АКШ", &nameGenitive, "АКШнын") &&
              Gives(&s_ky, "БУУ", &nameGenitive, "БУУнун") &&
              Gives(&s_ky, "УКМК", &nameDative, "УКМКга") &&
              Gives(&s_ky, "350", &ablative, "350дөн") && Gives(&s_ky, "5", &nameDative, "5ке"),
          "abbreviations and digits joined, as the press writes them");
    mlocInflection letters = Case(mloc_caseDative);
    letters.reading = mloc_readingLetters;
    mlocOutput signOutput = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(Gives(&s_ky, "я", &letters, "яга") && Gives(&s_ky, "ө", &letters, "өгө") &&
              Gives(&s_ky, "ё", &letters, "ёго") && Gives(&s_ky, "к", &letters, "кга") &&
              mlocInflectText(&s_ky, "Ь", strlen("Ь"), &letters, &signOutput) == mloc_errorInvalid,
          "lowercase letters read by their names; a sign has none");
    mlocInflection question = Common(mloc_caseNominative);
    question.question = true;
    CHECK(Gives(&s_ky, "китеп", &question, "китеппи") &&
              Gives(&s_ky, "бала", &question, "балабы") &&
              Gives(&s_ky, "тоо", &question, "тообу") && Gives(&s_ky, "көл", &question, "көлбү"),
          "the question particle joined");
    mlocInflection instrumental = Case(mloc_caseInstrumental);
    mlocInflection copula = Case(mloc_caseNominative);
    copula.copula = true;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_ky, "тоо", strlen("тоо"), &instrumental, &output) ==
                  mloc_errorInvalid &&
              mlocInflectText(&s_ky, "тоо", strlen("тоо"), &copula, &output) == mloc_errorInvalid,
          "no instrumental case and no copula suffix in Kyrgyz");
}

static void TestAzerbaijani(void)
{
    CHECK(CasesOf(&s_az, "ev", true, (const char* const[]){"evin", "evə", "evi", "evdə", "evdən"},
                  5) &&
              CasesOf(&s_az, "kitab", true,
                      (const char* const[]){"kitabın", "kitaba", "kitabı", "kitabda", "kitabdan"},
                      5) &&
              CasesOf(&s_az, "Bakı", false,
                      (const char* const[]){"Bakının", "Bakıya", "Bakını", "Bakıda", "Bakıdan"},
                      5) &&
              CasesOf(&s_az, "Əli", false,
                      (const char* const[]){"Əlinin", "Əliyə", "Əlini", "Əlidə", "Əlidən"}, 5),
          "two-way A with ə, no d to t, the buffers; names joined");
    mlocInflection genitive = Case(mloc_caseGenitive);
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection accusative = Case(mloc_caseAccusative);
    mlocInflection locative = Case(mloc_caseLocative);
    mlocInflection ablative = Case(mloc_caseAblative);
    mlocInflection third = Case(mloc_caseNominative);
    third.possessorPerson = 3;
    CHECK(Gives(&s_az, "AMEA", &ablative, "AMEA-dan") &&
              Gives(&s_az, "MDB", &genitive, "MDB-nin") && Gives(&s_az, "BMT", &dative, "BMT-yə") &&
              Gives(&s_az, "BDU", &dative, "BDU-ya"),
          "the norms' abbreviations: a hyphen, the last letter's name");
    CHECK(Gives(&s_az, "2", &dative, "2-yə") && Gives(&s_az, "3", &locative, "3-də") &&
              Gives(&s_az, "5", &accusative, "5-i") && Gives(&s_az, "6", &genitive, "6-nın") &&
              Gives(&s_az, "17", &third, "17-si") && Gives(&s_az, "20", &ablative, "20-dən"),
          "the norms' numbers: a hyphen, the spoken last word");
    mlocInflection hinted = Case(mloc_caseGenitive);
    hinted.hint = "maykrosoft";
    hinted.hintLength = strlen("maykrosoft");
    CHECK(Gives(&s_az, "Microsoft", &hinted, "Microsoft-un") &&
              Gives(&s_az, "Microsoft", &genitive, "Microsoftun"),
          "a name a hint says is foreign takes the hyphen");
    mlocInflection commonThird = Common(mloc_caseNominative);
    commonThird.possessorPerson = 3;
    mlocInflection commonDative = Common(mloc_caseDative);
    CHECK(Gives(&s_az, "çörək", &commonThird, "çörəyi") &&
              Gives(&s_az, "ayaq", &commonThird, "ayağı") &&
              Gives(&s_az, "obelisk", &commonThird, "obeliski") &&
              Gives(&s_az, "ok", &commonThird, "oku") &&
              Gives(&s_az, "çörək", &commonDative, "çörəyə") &&
              Gives(&s_az, "Tofiq", &third, "Tofiqi"),
          "a common noun's k and q after a vowel in two syllables written y and ğ; one syllable, "
          "a k after a consonant and names keep them");
    mlocInflection plural = Common(mloc_caseNominative);
    plural.plural = true;
    mlocInflection theirs = plural;
    theirs.possessorPerson = 3;
    theirs.possessorPlural = true;
    mlocInflection copula = Common(mloc_caseLocative);
    copula.copula = true;
    mlocInflection question = Common(mloc_caseDative);
    question.question = true;
    CHECK(Gives(&s_az, "ev", &plural, "evlər") && Gives(&s_az, "ev", &theirs, "evləri") &&
              Gives(&s_az, "ev", &copula, "evdədir") && Gives(&s_az, "ev", &question, "evəmi"),
          "the plural, the third plural's possessive once, the copula, the joined question");
    mlocInflection instrumental = Case(mloc_caseInstrumental);
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocLocale cyrillic;
    mlocInflector inflector;
    CHECK(mlocInflectText(&s_az, "ev", 2, &instrumental, &output) == mloc_errorInvalid &&
              mlocParseLocale("az-Cyrl", 7, &cyrillic).status == mloc_success &&
              mlocInitInflector(&inflector, &s_data, &cyrillic) == mloc_errorNotFound,
          "no instrumental case; Azerbaijani in Cyrillic refused");
    mlocInflection letters = Case(mloc_caseDative);
    letters.reading = mloc_readingLetters;
    CHECK(Gives(&s_az, "ə", &letters, "ə-yə") && Gives(&s_az, "Q", &letters, "Q-yə") &&
              Gives(&s_az, "x", &letters, "x-yə") && Gives(&s_az, "Ğ", &letters, "Ğ-yə") &&
              Gives(&s_az, "Z", &letters, "Z-yə") &&
              mlocInflectText(&s_az, "W", 1, &letters, &output) == mloc_errorInvalid,
          "letters by their names; W has none");
}

static void TestUzbek(void)
{
    CHECK(
        CasesOf(&s_uz, "kitob", true,
                (const char* const[]){"kitobning", "kitobga", "kitobni", "kitobda", "kitobdan"},
                5) &&
            CasesOf(&s_uz, "tilak", true,
                    (const char* const[]){"tilakning", "tilakka", "tilakni", "tilakda", "tilakdan"},
                    5) &&
            CasesOf(&s_uz, "qishloq", true,
                    (const char* const[]){"qishloqning", "qishloqqa", "qishloqni", "qishloqda",
                                          "qishloqdan"},
                    5) &&
            CasesOf(&s_uz, "togʻ", true,
                    (const char* const[]){"togʻning", "togʻga", "togʻni", "togʻda", "togʻdan"}, 5),
        "no harmony; -ka after k, -qa after q, -ga after gʻ (item 37)");
    mlocInflection r = Common(mloc_caseNominative);
    r.possessorPerson = 1;
    bool good = Gives(&s_uz, "yurak", &r, "yuragim") && Gives(&s_uz, "bola", &r, "bolam");
    r.possessorPerson = 2;
    good = Gives(&s_uz, "tilak", &r, "tilaging") && Gives(&s_uz, "bola", &r, "bolang") && good;
    r.possessorPerson = 3;
    good = Gives(&s_uz, "qishloq", &r, "qishlogʻi") && Gives(&s_uz, "bola", &r, "bolasi") &&
           Gives(&s_uz, "bek", &r, "begi") && Gives(&s_uz, "yoʻq", &r, "yoʻgʻi") &&
           Gives(&s_uz, "toʻq", &r, "toʻqi") && Gives(&s_uz, "xalq", &r, "xalqi") &&
           Gives(&s_uz, "oʻrtoq", &r, "oʻrtogʻi") && good;
    CHECK(good, "the possessives; k and q after a vowel in two syllables written g and gʻ "
                "(item 34), bek and yoʻq too, one syllable and a cluster keeping them");
    mlocInflection third = Common(mloc_caseDative);
    third.possessorPerson = 3;
    mlocInflection theirs = Common(mloc_caseNominative);
    theirs.plural = true;
    theirs.possessorPerson = 3;
    theirs.possessorPlural = true;
    mlocInflection copula = Common(mloc_caseLocative);
    copula.copula = true;
    mlocInflection question = Common(mloc_caseNominative);
    question.question = true;
    CHECK(Gives(&s_uz, "uy", &third, "uyiga") && Gives(&s_uz, "kitob", &theirs, "kitoblari") &&
              Gives(&s_uz, "uy", &copula, "uydadir") && Gives(&s_uz, "kitob", &question, "kitobmi"),
          "no pronominal n; the third plural once; the copula and question joined");
    mlocInflection genitive = Case(mloc_caseGenitive);
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection name = Case(mloc_caseNominative);
    name.possessorPerson = 3;
    CHECK(Gives(&s_uz, "ToshDU", &genitive, "ToshDUning") &&
              Gives(&s_uz, "BMT", &dative, "BMTga") && Gives(&s_uz, "Ulugʻbek", &name, "Ulugʻbegi"),
          "abbreviations joined (item 49); a name's stop written g");
    CHECK(Gives(&s_uz, "5", &dative, "5ga") && Gives(&s_uz, "50", &dative, "50ka") &&
              Gives(&s_uz, "2", &name, "2si") && Gives(&s_uz, "100", &dative, "100ga") &&
              Gives(&s_uz, "3000", &name, "3000i") && Gives(&s_uz, "0", &name, "0i") &&
              Gives(&s_uz, "1,5", &dative, "1,5ga") && Gives(&s_uz, "40", &dative, "40qa"),
          "numbers by their last word (ellik, ikki, yuz, ming, nol, besh, qirq), joined");
    mlocDecimal huge;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocParseDecimal("1e36", 4, &huge).status == mloc_success &&
              mlocInflectNumber(&s_uz, nullptr, &huge, &dative, &output) == mloc_errorNotFound,
          "a power with no Uzbek word is refused");
    mlocInflection letters = Case(mloc_caseDative);
    letters.reading = mloc_readingLetters;
    mlocInflection instrumental = Case(mloc_caseInstrumental);
    CHECK(Gives(&s_uz, "Z", &letters, "Zga") && Gives(&s_uz, "q", &letters, "qga") &&
              mlocInflectText(&s_uz, "C", 1, &letters, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_uz, "uy", 2, &instrumental, &output) == mloc_errorInvalid,
          "letters by their names, C none; no instrumental");
}

static void TestTurkmen(void)
{
    CHECK(CasesOf(&s_tk, "kitap", true,
                  (const char* const[]){"kitabyň", "kitaba", "kitaby", "kitapda", "kitapdan"}, 5) &&
              CasesOf(&s_tk, "göz", true,
                      (const char* const[]){"gözüň", "göze", "gözi", "gözde", "gözden"}, 5) &&
              CasesOf(&s_tk, "ene", true,
                      (const char* const[]){"eneniň", "enä", "eneni", "enede", "eneden"}, 5) &&
              CasesOf(
                  &s_tk, "okuwçy", true,
                  (const char* const[]){"okuwçynyň", "okuwça", "okuwçyny", "okuwçyda", "okuwçydan"},
                  5),
          "the cases; the genitive rounded after ö and ü, the accusative never; the "
          "dative fused with a final vowel (enä, okuwça)");
    mlocInflection r = Common(mloc_caseNominative);
    r.possessorPerson = 1;
    bool good = Gives(&s_tk, "ýol", &r, "ýolum") && Gives(&s_tk, "göz", &r, "gözüm") &&
                Gives(&s_tk, "direktor", &r, "direktorum") && Gives(&s_tk, "el", &r, "elim") &&
                Gives(&s_tk, "ene", &r, "enem") && Gives(&s_tk, "ýürek", &r, "ýüregim");
    r.possessorPerson = 2;
    good = Gives(&s_tk, "ýol", &r, "ýoluň") && good;
    r.possessorPerson = 3;
    good = Gives(&s_tk, "göz", &r, "gözü") && Gives(&s_tk, "ýol", &r, "ýoly") &&
           Gives(&s_tk, "ene", &r, "enesi") && good;
    r.possessorPlural = true;
    r.possessorPerson = 1;
    good = Gives(&s_tk, "göz", &r, "gözümiz") && good;
    CHECK(good, "the possessives rounded after any rounded vowel, the first suffix vowel only "
                "(gözümiz); the third person's after ö and ü");
    mlocInflection accusative = Common(mloc_caseAccusative);
    CHECK(Gives(&s_tk, "baýlyk", &accusative, "baýlygy") &&
              Gives(&s_tk, "agaç", &accusative, "agajy") &&
              Gives(&s_tk, "gazet", &accusative, "gazeti") &&
              Gives(&s_tk, "ýurt", &accusative, "ýurty") &&
              Gives(&s_tk, "halk", &accusative, "halky") && Gives(&s_tk, "ok", &accusative, "oky"),
          "p, ç and k after a vowel in two syllables voiced; t, a cluster and one syllable "
          "kept");
    mlocInflection theirs = Common(mloc_caseNominative);
    theirs.plural = true;
    theirs.possessorPerson = 3;
    theirs.possessorPlural = true;
    mlocInflection copula = Common(mloc_caseLocative);
    copula.copula = true;
    mlocInflection question = Common(mloc_caseNominative);
    question.question = true;
    CHECK(Gives(&s_tk, "kitap", &theirs, "kitaplary") && Gives(&s_tk, "öý", &copula, "öýdedir") &&
              Gives(&s_tk, "kitap", &question, "kitapmy"),
          "the third plural once; the copula and question joined");
    mlocInflection genitive = Case(mloc_caseGenitive);
    mlocInflection dative = Case(mloc_caseDative);
    mlocInflection ablative = Case(mloc_caseAblative);
    CHECK(Gives(&s_tk, "Aşgabat", &dative, "Aşgabada") &&
              Gives(&s_tk, "Türkmenabat", &genitive, "Türkmenabadyň") &&
              Gives(&s_tk, "Merw", &dative, "Merwe") && Gives(&s_tk, "BMG", &genitive, "BMG-niň") &&
              Gives(&s_tk, "AFK", &genitive, "AFK-nyň") && Gives(&s_tk, "BMG", &dative, "BMG-ä"),
          "names joined, the t of -abat voiced; abbreviations by their letters after a hyphen");
    CHECK(Gives(&s_tk, "1700", &ablative, "1700-den") && Gives(&s_tk, "5", &dative, "5-e") &&
              Gives(&s_tk, "40", &genitive, "40-yň") && Gives(&s_tk, "100", &genitive, "100-üň") &&
              Gives(&s_tk, "0", &dative, "0-a") && Gives(&s_tk, "60", &dative, "60-a") &&
              Gives(&s_tk, "3000", &dative, "3000-e") &&
              Gives(&s_tk, "1000000", &dative, "1000000-a"),
          "numbers by their last word (müň, bäş, kyrk, ýüz, "
          "nol, altmyş, million) after a hyphen");
    mlocDecimal huge;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocParseDecimal("1e21", 4, &huge).status == mloc_success &&
              mlocInflectNumber(&s_tk, nullptr, &huge, &dative, &output) == mloc_errorNotFound,
          "a power with no Turkmen word is refused");
    mlocInflection letters = Case(mloc_caseDative);
    letters.reading = mloc_readingLetters;
    mlocInflection instrumental = Case(mloc_caseInstrumental);
    CHECK(Gives(&s_tk, "K", &letters, "K-a") && Gives(&s_tk, "Z", &letters, "Z-ä") &&
              Gives(&s_tk, "Ž", &letters, "Ž-ä") &&
              mlocInflectText(&s_tk, "Q", 1, &letters, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_tk, "el", 2, &instrumental, &output) == mloc_errorInvalid,
          "letters by their names, Q none; no instrumental");
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
    mlocLocale ky;
    if (mlocParseLocale("ky", 2, &ky).status != mloc_success ||
        mlocInitInflector(&s_ky, &s_data, &ky) != mloc_success)
    {
        fputs("test_grammar_turkic: cannot make the Kyrgyz inflector\n", stderr);
        return 2;
    }
    mlocLocale az;
    if (mlocParseLocale("az", 2, &az).status != mloc_success ||
        mlocInitInflector(&s_az, &s_data, &az) != mloc_success)
    {
        fputs("test_grammar_turkic: cannot make the Azerbaijani inflector\n", stderr);
        return 2;
    }
    mlocLocale uz;
    if (mlocParseLocale("uz", 2, &uz).status != mloc_success ||
        mlocInitInflector(&s_uz, &s_data, &uz) != mloc_success)
    {
        fputs("test_grammar_turkic: cannot make the Uzbek inflector\n", stderr);
        return 2;
    }
    mlocLocale tk;
    if (mlocParseLocale("tk", 2, &tk).status != mloc_success ||
        mlocInitInflector(&s_tk, &s_data, &tk) != mloc_success)
    {
        fputs("test_grammar_turkic: cannot make the Turkmen inflector\n", stderr);
        return 2;
    }
    TestKazakhCases();
    TestKazakhPossessives();
    TestKazakhWords();
    TestKazakhMarks();
    TestKazakhRefusals();
    TestKyrgyzCases();
    TestKyrgyzPossessives();
    TestKyrgyzWords();
    TestAzerbaijani();
    TestUzbek();
    TestTurkmen();
    return s_failures == 0 ? 0 : 1;
}
