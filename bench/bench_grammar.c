// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The grammar part's benchmark cases (records mloc-0047 to mloc-0052).

#include "bench_grammar.h"

#include "maul-locale/decimal.h"
#include "maul-locale/grammar.h"
#include "maul-locale/list.h"
#include "maul-locale/message.h"

#include <stdio.h>
#include <string.h>

static mlocInflector s_tr;
static mlocInflector s_fi;
static mlocInflector s_hu;
static mlocInflector s_ko;
static mlocLocale s_trLocale;
static mlocLocale s_frLocale;
static mlocDecimal s_year;
static mlocMessageFormatter s_formatter;
static uint8_t s_compiled[1024];
static uint8_t s_bound[8192];
static char s_text[256];
static size_t s_length;
static volatile size_t s_sink;

static void Inflect(const mlocInflector* inflector, const char* text, const mlocInflection* r)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    (void)mlocInflectText(inflector, text, strlen(text), r, &output);
    s_length = output.length;
    s_sink += output.length;
}

static void InflectorInit(void)
{
    mlocInflector inflector;
    s_sink += (size_t)mlocInitInflector(&inflector, BenchDataSet(), &s_trLocale);
}

static void NameDative(void)
{
    static const mlocInflection r = {.grammaticalCase = mloc_caseDative};
    Inflect(&s_tr, "Rodrigo", &r);
}

static void CommonPlural(void)
{
    static const mlocInflection r = {
        .grammaticalCase = mloc_caseAccusative, .plural = true, .commonNoun = true};
    Inflect(&s_tr, "kitap", &r);
}

static void NumberLocative(void)
{
    static const mlocInflection r = {.grammaticalCase = mloc_caseLocative};
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    (void)mlocInflectNumber(&s_tr, nullptr, &s_year, &r, &output);
    s_length = output.length;
    s_sink += output.length;
}

static void FinnishName(void)
{
    static const mlocInflection r = {.grammaticalCase = mloc_caseInessive};
    Inflect(&s_fi, "Helsinki", &r);
}

static void HungarianWord(void)
{
    static const mlocInflection r = {.grammaticalCase = mloc_caseInessive, .commonNoun = true};
    Inflect(&s_hu, "ház", &r);
}

static void KoreanParticle(void)
{
    static const mlocInflection r = {.particle = mloc_particleTopic};
    Inflect(&s_ko, "사과", &r);
}

static void ListGender(void)
{
    static const mlocPersonGender persons[3] = {mloc_personFemale, mloc_personMale,
                                                mloc_personFemale};
    mlocPersonGender gender;
    s_sink += (size_t)mlocGetListGender(BenchDataSet(), &s_frLocale, persons, 3, &gender);
    s_sink += (size_t)gender;
}

static void MessageCase(void)
{
    static const mlocMessageArgument name = {
        "n", 1, {mloc_messageValueString, 0, {.string = {"Rodrigo", 7}}}};
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    (void)mlocFormatMessage(s_bound, &name, 1, &output, nullptr);
    s_length = output.length;
    s_sink += output.length;
}

static bool Make(const char* tag, mlocLocale* locale, mlocInflector* out)
{
    return mlocParseLocale(tag, strlen(tag), locale).status == mloc_success &&
           mlocInitInflector(out, BenchDataSet(), locale) == mloc_success;
}

bool PrepareGrammarBench(void)
{
    mlocLocale fi;
    mlocLocale hu;
    mlocLocale ko;
    static const char s_message[] = "{$n :mloc:case case=dative}";
    size_t compiled = 0;
    size_t bound = 0;
    mlocMessageError error;
    return Make("tr", &s_trLocale, &s_tr) && Make("fi", &fi, &s_fi) && Make("hu", &hu, &s_hu) &&
           Make("ko", &ko, &s_ko) && mlocParseLocale("fr", 2, &s_frLocale).status == mloc_success &&
           mlocParseDecimal("1985", 4, &s_year).status == mloc_success &&
           mlocInitMessageFormatter(&s_formatter, BenchDataSet(), &s_trLocale, nullptr) ==
               mloc_success &&
           mlocCompileMessage(s_message, sizeof(s_message) - 1, s_compiled, sizeof(s_compiled),
                              &compiled, &error)
                   .status == mloc_success &&
           mlocBindMessage(&s_formatter, s_compiled, compiled, s_bound, sizeof(s_bound), &bound) ==
               mloc_success;
}

void ShowGrammarBench(void)
{
    void (*const cases[])(void) = {NameDative,    CommonPlural,   NumberLocative, FinnishName,
                                   HungarianWord, KoreanParticle, MessageCase};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        cases[i]();
        printf("[%.*s]\n", (int)s_length, s_text);
    }
}

const BenchCase g_grammarCases[] = {
    {"inflector-init", InflectorInit, 500000}, {"inflect-name", NameDative, 2000000},
    {"inflect-common", CommonPlural, 2000000}, {"inflect-number", NumberLocative, 1000000},
    {"inflect-fi", FinnishName, 1000000},      {"inflect-hu", HungarianWord, 1000000},
    {"inflect-ko", KoreanParticle, 2000000},   {"list-gender", ListGender, 1000000},
    {"message-case", MessageCase, 1000000},
};
const size_t g_grammarCaseCount = sizeof(g_grammarCases) / sizeof(g_grammarCases[0]);
