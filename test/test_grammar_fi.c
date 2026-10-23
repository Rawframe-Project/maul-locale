// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Finnish suffixes (maul-locale/grammar.h): the types' stems and
// gradation, harmony, the cases in both numbers, possessives and the
// question clitic, names by their ending, the colon after digits and
// capitals as the Kielenhuollon käsikirja writes it, the apostrophe of an
// unsaid end, and the refusals.

#include "test_harness.h"

#include "maul-locale/grammar.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocInflector s_fi;
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

static bool Gives(const char* text, const mlocInflection* request, const char* expected)
{
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    mlocResult r = mlocInflectText(&s_fi, text, strlen(text), request, &output);
    bool good = r == mloc_success && output.length == strlen(expected) &&
                memcmp(s_text, expected, output.length) == 0;
    if (!good)
    {
        printf("  [%s] -> %d [%.*s], not [%s]\n", text, (int)r, (int)output.length, s_text,
               expected);
    }
    return good;
}

// Whether a common noun takes the forms given in the cases given.
static bool Forms(const char* text, const mlocGrammaticalCase* cases, const char* const* forms,
                  int count)
{
    bool good = true;
    for (int i = 0; i < count; i++)
    {
        mlocInflection r = Common(cases[i]);
        good = Gives(text, &r, forms[i]) && good;
    }
    return good;
}

static const mlocGrammaticalCase s_cases[13] = {
    mloc_caseGenitive,  mloc_casePartitive,   mloc_caseInessive, mloc_caseElative,
    mloc_caseIllative,  mloc_caseAdessive,    mloc_caseAblative, mloc_caseAllative,
    mloc_caseEssive,    mloc_caseTranslative, mloc_caseAbessive, mloc_caseInstrumental,
    mloc_caseComitative};

static void TestTypes(void)
{
    CHECK(Forms("katu", s_cases,
                (const char* const[]){"kadun", "katua", "kadussa", "kadusta", "katuun", "kadulla",
                                      "kadulta", "kadulle", "katuna", "kaduksi", "kadutta",
                                      "kaduin", "katuineen"},
                13),
          "type 1 with t:d: weak before a closed syllable");
    CHECK(Forms("käsi", s_cases,
                (const char* const[]){"käden", "kättä", "kädessä", "kädestä", "käteen", "kädellä",
                                      "kädeltä", "kädelle", "kätenä", "kädeksi", "kädettä", "käsin",
                                      "käsineen"},
                13),
          "type 27, front");
    static const mlocGrammaticalCase s_three[3] = {mloc_caseGenitive, mloc_casePartitive,
                                                   mloc_caseIllative};
    CHECK(Forms("nainen", s_three, (const char* const[]){"naisen", "naista", "naiseen"}, 3) &&
              Forms("vastaus", s_three,
                    (const char* const[]){"vastauksen", "vastausta", "vastaukseen"}, 3) &&
              Forms("rengas", s_three, (const char* const[]){"renkaan", "rengasta", "renkaaseen"},
                    3) &&
              Forms("maa", s_three, (const char* const[]){"maan", "maata", "maahan"}, 3) &&
              Forms("suo", s_three, (const char* const[]){"suon", "suota", "suohon"}, 3) &&
              Forms("hame", s_three, (const char* const[]){"hameen", "hametta", "hameeseen"}, 3) &&
              Forms("ovi", s_three, (const char* const[]){"oven", "ovea", "oveen"}, 3),
          "types 38, 39, 41 strengthened, 18, 19, 48, 7");
    mlocInflection plural = Common(mloc_casePartitive);
    plural.plural = true;
    mlocInflection genitive = Common(mloc_caseGenitive);
    genitive.plural = true;
    mlocInflection into = Common(mloc_caseIllative);
    into.plural = true;
    mlocInflection inside = Common(mloc_caseInessive);
    CHECK(Gives("viikunanlehti", &inside, "viikunanlehdessä") &&
              Gives("ajoneuvokäsi", &inside, "ajoneuvokädessä"),
          "a compound's harmony is its last word's");
    CHECK(Gives("raaka-aine", &inside, "raaka-aineessa") &&
              Gives("Kalevala-seura", &inside, "Kalevala-seurassa") &&
              Gives("kolmesataakuusikymmentäviisipäiväinenkalenterivuosijärjestelmäsuunnitelma",
                    &inside,
                    "kolmesataakuusikymmentäviisipäiväinenkalenterivuosijärjestelmäsuunnitelmassa"),
          "the word after a hyphen, and a word longer than the letters read");
    CHECK(Gives("vaaka", &inside, "vaa'assa") && Gives("aika", &inside, "ajassa") &&
              Gives("poika", &inside, "pojassa") && Gives("koko", &inside, "koossa") &&
              Gives("liuku", &inside, "liu'ussa"),
          "a k gone: the apostrophe and j");
    CHECK(Gives("kala", &plural, "kaloja") && Gives("koira", &plural, "koiria") &&
              Gives("maa", &plural, "maita") && Gives("kala", &genitive, "kalojen") &&
              Gives("ovi", &genitive, "ovien") && Gives("katu", &into, "katuihin") &&
              Gives("tau", &into, "tauihin") && Gives("kalleus", &genitive, "kalleuksien") &&

              Gives("hiiltouuni", &genitive, "hiiltouunien") && Gives("vapaa", &into, "vapaisiin"),
          "the plural stems and their partitive, genitive and illative");
}

static void TestNames(void)
{
    mlocInflection genitive = Case(mloc_caseGenitive);
    mlocInflection inside = Case(mloc_caseInessive);
    mlocInflection partitive = Case(mloc_casePartitive);
    CHECK(Gives("Pekka", &genitive, "Pekan") && Gives("Helsinki", &inside, "Helsingissä") &&
              Gives("Tampere", &genitive, "Tampereen") &&
              Gives("Virtanen", &genitive, "Virtasen") && Gives("Bush", &partitive, "Bushia") &&
              Gives("Hämeenlinna", &inside, "Hämeenlinnassa") &&
              Gives("Malmö", &inside, "Malmössä"),
          "names by their ending, graded where a double stop or nk ends them; harmony");
    mlocInflection into = Case(mloc_caseIllative);
    CHECK(Gives("Espoo", &into, "Espooseen") && Gives("Nokia", &partitive, "Nokiaa") &&
              Gives("Jaana", &partitive, "Jaanaa") && Gives("Kuopio", &into, "Kuopioon") &&
              Gives("Lappi", &into, "Lappiin") && Gives("Sirkka", &genitive, "Sirkan") &&
              Gives("Öljymäki", &genitive, "Öljymäen"),
          "names by a long vowel, -ia, -a, a diphthong's end, -i; grades; a capital Ö");
    mlocInflection many = Case(mloc_casePartitive);
    many.plural = true;
    CHECK(Gives("Nokia", &many, "Nokioita") && Gives("Jaana", &many, "Jaanoja"),
          "the plural of -ia and of -a names");
    CHECK(Gives("Isokangas", &genitive, "Isokankaan") &&
              Gives("Kotij\xC3\xA4rvi", &genitive, "Kotij\xC3\xA4rven"),
          "the place words of six bytes, the longest");
    mlocInflection hinted = Case(mloc_caseInessive);
    hinted.hint = "bordoo";
    hinted.hintLength = strlen(hinted.hint);
    CHECK(Gives("Bordeaux", &hinted, "Bordeaux'ssa"), "an unsaid end takes an apostrophe");
}

static void TestPossessives(void)
{
    mlocInflection r = Common(mloc_caseNominative);
    r.possessorPerson = 1;
    mlocInflection inside = Common(mloc_caseInessive);
    inside.possessorPerson = 3;
    mlocInflection into = Common(mloc_caseIllative);
    into.possessorPerson = 2;
    mlocInflection question = Common(mloc_caseInessive);
    question.question = true;
    mlocInflection with = Common(mloc_caseComitative);
    with.possessorPerson = 1;
    mlocInflection withTheirs = Common(mloc_caseComitative);
    withTheirs.possessorPerson = 3;
    CHECK(Gives("vaimo", &with, "vaimoineni") && Gives("vaimo", &withTheirs, "vaimoineen"),
          "the comitative's possessives");
    mlocInflection becoming = Common(mloc_caseTranslative);
    becoming.possessorPerson = 1;
    CHECK(Gives("talo", &becoming, "talokseni"),
          "the translative's -ksi as -kse before a possessive");
    CHECK(Gives("katu", &r, "katuni") && Gives("talo", &inside, "talossaan") &&
              Gives("talo", &into, "taloosi") && Gives("talo", &question, "talossako") &&
              Gives("kylä", &question, "kylässäkö"),
          "the possessives and the question clitic");
}

static void TestMarked(void)
{
    mlocInflection partitive = Case(mloc_casePartitive);
    mlocInflection into = Case(mloc_caseIllative);
    mlocInflection inside = Case(mloc_caseInessive);
    mlocInflection genitive = Case(mloc_caseGenitive);
    CHECK(Gives("1", &partitive, "1:tä") && Gives("3", &partitive, "3:a") &&
              Gives("5", &into, "5:een") && Gives("17", &into, "17:ään") &&
              Gives("17", &partitive, "17:ää") && Gives("100", &partitive, "100:aa") &&
              Gives("100", &into, "100:aan") && Gives("1000", &partitive, "1000:ta") &&
              Gives("20", &into, "20:een") && Gives("5", &inside, "5:ssä") &&
              Gives("3", &inside, "3:ssa"),
          "digits as the Kielenhuollon käsikirja writes them");
    CHECK(Gives("5.", &genitive, "5:nnen") && Gives("5.", &into, "5:nteen") &&
              Gives("3.", &partitive, "3:tta"),
          "ordinals");
    CHECK(Gives("1.", &inside, "1:sessä") && Gives("5.", &inside, "5:nnessä") &&
              Gives("Ä", &inside, "Ä:ssä") && Gives("Å", &inside, "Å:ssa") &&
              Gives("Ö", &inside, "Ö:ssä"),
          "an ordinal's other cases; the letters ä and å");
    CHECK(Gives("EU", &inside, "EU:ssa") && Gives("EU", &into, "EU:hun") &&
              Gives("YK", &genitive, "YK:n") && Gives("BBC", &inside, "BBC:ssä") &&
              Gives("EU", &partitive, "EU:ta"),
          "abbreviations by their letters' names");
    mlocInflection plural = Case(mloc_caseInessive);
    plural.plural = true;
    mlocInflection copula = Case(mloc_caseInessive);
    copula.copula = true;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_fi, "5", 1, &plural, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_fi, "talo", 4, &copula, &output) == mloc_errorInvalid,
          "no plural of digits, no copula");
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
        fprintf(stderr, "test_grammar_fi: cannot open %s\n", MLOC_SPELLOUT_FILE);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    mlocLocale fi;
    if (mlocParseLocale("fi", 2, &fi).status != mloc_success ||
        mlocInitInflector(&s_fi, &s_data, &fi) != mloc_success)
    {
        fputs("test_grammar_fi: cannot make the inflector\n", stderr);
        return 2;
    }
    TestTypes();
    TestNames();
    TestPossessives();
    TestMarked();
    return s_failures == 0 ? 0 : 1;
}
