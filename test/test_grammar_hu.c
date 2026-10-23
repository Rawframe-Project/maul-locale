// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Hungarian suffixes (maul-locale/grammar.h), as the Academy's rules (AkH
// 12) write them: harmony, lengthening, linking vowels, the assimilated v
// of -val and -vá, the table's stems, the possessives, compounds, names,
// numbers, abbreviations and the refusals.

#include "test_harness.h"

#include "maul-locale/grammar.h"

#include <stdio.h>
#include <string.h>

static mlocDataSet s_data;
static mlocInflector s_hu;
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
    mlocResult r = mlocInflectText(&s_hu, text, strlen(text), request, &output);
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

static void TestCases(void)
{
    static const mlocGrammaticalCase s_all[16] = {
        mloc_caseAccusative,  mloc_caseDative,      mloc_caseInstrumental, mloc_caseCausal,
        mloc_caseTranslative, mloc_caseTerminative, mloc_caseEssive,       mloc_caseInessive,
        mloc_caseSuperessive, mloc_caseAdessive,    mloc_caseIllative,     mloc_caseSublative,
        mloc_caseDelative,    mloc_caseElative,     mloc_caseAblative,     mloc_caseAllative};
    CHECK(Forms("ház", s_all,
                (const char* const[]){"házat", "háznak", "házzal", "házért", "házzá", "házig",
                                      "házként", "házban", "házon", "háznál", "házba", "házra",
                                      "házról", "házból", "háztól", "házhoz"},
                16),
          "the sixteen cases of a back lowering stem");
    CHECK(Forms("tükör", s_all,
                (const char* const[]){"tükröt", "tükörnek", "tükörrel", "tükörért", "tükörré",
                                      "tükörig", "tükörként", "tükörben", "tükrön", "tükörnél",
                                      "tükörbe", "tükörre", "tükörről", "tükörből", "tükörtől",
                                      "tükörhöz"},
                16),
          "a front rounded stem that drops its vowel before a linking vowel");
    CHECK(Forms("alma", s_all,
                (const char* const[]){"almát", "almának", "almával", "almáért", "almává", "almáig",
                                      "almaként", "almában", "almán", "almánál", "almába", "almára",
                                      "almáról", "almából", "almától", "almához"},
                16),
          "a final a lengthened before all but -ként (AkH 29)");
    static const mlocGrammaticalCase s_four[4] = {mloc_caseAccusative, mloc_caseInstrumental,
                                                  mloc_caseSuperessive, mloc_caseInessive};
    CHECK(
        Forms("kert", s_four, (const char* const[]){"kertet", "kerttel", "kerten", "kertben"}, 4) &&
            Forms("papír", s_four,
                  (const char* const[]){"papírt", "papírral", "papíron", "papírban"}, 4) &&
            Forms("kávé", s_four, (const char* const[]){"kávét", "kávéval", "kávén", "kávéban"},
                  4) &&
            Forms("analízis", s_four,
                  (const char* const[]){"analízist", "analízissel", "analízisen", "analízisben"},
                  4) &&
            Forms("híd", s_four, (const char* const[]){"hidat", "híddal", "hídon", "hídban"}, 4),
        "harmony: one i, í or é after a back vowel back, two front, hídon by the table");
}

static void TestStems(void)
{
    mlocInflection accusative = Common(mloc_caseAccusative);
    mlocInflection superessive = Common(mloc_caseSuperessive);
    mlocInflection plural = Common(mloc_caseNominative);
    plural.plural = true;
    CHECK(Gives("víz", &accusative, "vizet") && Gives("víz", &superessive, "vízen") &&
              Gives("bokor", &accusative, "bokrot") && Gives("bokor", &superessive, "bokron") &&
              Gives("ló", &plural, "lovak") && Gives("ló", &superessive, "lovon") &&
              Gives("kő", &accusative, "követ") && Gives("úr", &plural, "urak") &&
              Gives("nyugalom", &accusative, "nyugalmat"),
          "the table's stems: shortened, dropped, v; the superessive where it says; -alom");
    mlocInflection third = Common(mloc_caseNominative);
    third.possessorPerson = 3;
    CHECK(Gives("út", &third, "útja") && Gives("út", &accusative, "utat") &&
              Gives("víz", &third, "vize") && Gives("ló", &third, "lova"),
          "the third person's possessive on the word as it is, or on the changed stem");
    mlocInflection dative = Case(mloc_caseDative);
    CHECK(Gives("Út", &accusative, "Utat") && Gives("Víz", &accusative, "Vizet") &&
              Gives("napló", &accusative, "naplót") && Gives("Űr", &dative, "Űrnek"),
          "a capital kept where the stem changes; a compound's last word of three letters or "
          "more; a capital vowel read as a vowel");
    CHECK(Gives("számítógép", &plural, "számítógépek") && Gives("étterem", &plural, "éttermek") &&
              Gives("könyvtár", &plural, "könyvtárak") && Gives("ellenfél", &plural, "ellenfelek"),
          "a common noun's compound by its last word");
    mlocInflection name = Case(mloc_caseAccusative);
    CHECK(Gives("Péter", &name, "Pétert") && Gives("Bonn", &name, "Bonnt") &&
              Gives("Marx", &name, "Marxot") && Gives("Kovács", &name, "Kovácsot"),
          "a name read as a whole: Péter is not tér; t after a long consonant, not after x");
}

static void TestAssimilation(void)
{
    mlocInflection with = Case(mloc_caseInstrumental);
    mlocInflection into = Case(mloc_caseTranslative);
    CHECK(Gives("Ádám", &with, "Ádámmal") && Gives("Kodály", &with, "Kodállyal") &&
              Gives("Szabolcs", &with, "Szabolccsal") && Gives("Kossuth", &with, "Kossuthtal") &&
              Gives("Madách", &with, "Madáchcsal") && Gives("Marx", &with, "Marxszal") &&
              Gives("Kölcsey", &with, "Kölcseyvel") && Gives("Papp", &with, "Papp-pal") &&
              Gives("Kiss", &into, "Kiss-sé"),
          "the v of -val and -vá assimilated as AkH 160 writes names");
    mlocInflection commonWith = Common(mloc_caseInstrumental);
    mlocInflection onto = Common(mloc_caseSublative);
    CHECK(Gives("toll", &commonWith, "tollal") && Gives("kulcs", &commonWith, "kulccsal") &&
              Gives("szív", &commonWith, "szívvel") && Gives("orr", &onto, "orra"),
          "three same letters written two in a common noun (AkH 93)");
    mlocInflection plainName = Case(mloc_caseDative);
    CHECK(Gives("Mann", &plainName, "Mann-nak"),
          "a name's double letter keeps a hyphen before a suffix that begins with it");
    // \xC3\x80 (À): a letter whose last byte is 0x80, read whole.
    CHECK(Gives("Voil\xC3\x80", &with, "Voil\xC3\x80val"),
          "a last letter of two bytes ending in 0x80 is one vowel");
}

static void TestPossessives(void)
{
    mlocInflection r = Common(mloc_caseNominative);
    static const char* const s_house[2][3] = {{"házam", "házad", "háza"},
                                              {"házunk", "házatok", "házuk"}};
    static const char* const s_book[2][3] = {{"könyvem", "könyved", "könyve"},
                                             {"könyvünk", "könyvetek", "könyvük"}};
    bool good = true;
    for (int plural = 0; plural < 2; plural++)
    {
        for (int person = 1; person <= 3; person++)
        {
            r.possessorPerson = (uint8_t)person;
            r.possessorPlural = plural == 1;
            good = Gives("ház", &r, s_house[plural][person - 1]) && good;
            good = Gives("könyv", &r, s_book[plural][person - 1]) && good;
        }
    }
    CHECK(good, "the possessives of a lowering stem, back and front rounded");
    r.possessorPerson = 3;
    r.possessorPlural = false;
    CHECK(Gives("kert", &r, "kertje") && Gives("nap", &r, "napja") && Gives("toll", &r, "tolla") &&
              Gives("asztal", &r, "asztala") && Gives("alma", &r, "almája") &&
              Gives("kulcs", &r, "kulcsa"),
          "the third person's j by the word's end");
    r.plural = true;
    r.possessorPerson = 1;
    mlocInflection inside = Common(mloc_caseInessive);
    inside.possessorPerson = 3;
    mlocInflection mine = Common(mloc_caseAccusative);
    mine.possessorPerson = 1;
    mlocInflection yours = Common(mloc_caseNominative);
    yours.plural = true;
    yours.possessorPerson = 2;
    yours.possessorPlural = true;
    CHECK(Gives("ház", &r, "házaim") && Gives("alma", &r, "almáim") &&
              Gives("kert", &r, "kertjeim") && Gives("ház", &yours, "házaitok") &&
              Gives("kert", &yours, "kertjeitek") && Gives("könyv", &yours, "könyveitek") &&
              Gives("ház", &inside, "házában") && Gives("ház", &mine, "házamat"),
          "the possessed plural, and the cases after a possessive");
}

static void TestMarked(void)
{
    mlocInflection with = Case(mloc_caseInstrumental);
    mlocInflection accusative = Case(mloc_caseAccusative);
    mlocInflection inside = Case(mloc_caseInessive);
    mlocInflection dative = Case(mloc_caseDative);
    CHECK(Gives("5", &with, "5-tel") && Gives("3", &accusative, "3-at") &&
              Gives("10", &accusative, "10-et") && Gives("2010", &inside, "2010-ben") &&
              Gives("4", &with, "4-gyel") && Gives("1000", &accusative, "1000-et") &&
              Gives("23", &accusative, "23-at") && Gives("3.", &accusative, "3.-at") &&
              Gives("8.", &inside, "8.-ban"),
          "digits and ordinals after a hyphen, the suffix as the number is said (AkH 82, 292)");
    mlocInflection on = Case(mloc_caseSuperessive);
    CHECK(Gives("MTA", &on, "MTA-n") && Gives("ENSZ", &with, "ENSZ-szel") &&
              Gives("DNS", &with, "DNS-sel") && Gives("BKK", &dative, "BKK-nak") &&
              Gives("ELTE", &on, "ELTE-n") && Gives("XYZ", &with, "XYZ-vel"),
          "abbreviations after a hyphen by their letters, not lengthened (AkH 288)");
    mlocInflection hinted = Case(mloc_caseInstrumental);
    hinted.hint = "russzó";
    hinted.hintLength = strlen(hinted.hint);
    mlocInflection goethe = Case(mloc_caseDative);
    goethe.hint = "gőte";
    goethe.hintLength = strlen(goethe.hint);
    mlocInflection balzac = Case(mloc_caseInstrumental);
    balzac.hint = "balzak";
    balzac.hintLength = strlen(balzac.hint);
    CHECK(Gives("Rousseau", &hinted, "Rousseau-val") && Gives("Goethe", &goethe, "Goethének") &&
              Gives("Balzac", &balzac, "Balzackal"),
          "a hint: an end not said takes a hyphen, one said joins (AkH 216, 217)");
}

static void TestRefusals(void)
{
    mlocInflection genitive = Case(mloc_caseGenitive);
    mlocInflection copula = Case(mloc_caseInessive);
    copula.copula = true;
    mlocInflection question = Case(mloc_caseDative);
    question.question = true;
    mlocInflection fourth = Case(mloc_caseNominative);
    fourth.possessorPerson = 4;
    mlocOutput output = {s_text, sizeof(s_text), 0, nullptr, nullptr};
    CHECK(mlocInflectText(&s_hu, "ház", 4, &genitive, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_hu, "ház", 4, &copula, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_hu, "ház", 4, &question, &output) == mloc_errorInvalid &&
              mlocInflectText(&s_hu, "ház", 4, &fourth, &output) == mloc_errorInvalid,
          "no genitive, copula or question suffix, no fourth person");
    mlocInflection letters = Case(mloc_caseDative);
    mlocInflection nominative = Case(mloc_caseNominative);
    CHECK(Gives("Brr", &letters, "Brr-nek") && Gives("Anna", &nominative, "Anna"),
          "a word without a vowel by its letters; the nominative as it is");
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
        fprintf(stderr, "test_grammar_hu: cannot open %s\n", MLOC_SPELLOUT_FILE);
        return 2;
    }
    s_data = (mlocDataSet){sources, 2};
    mlocLocale hu;
    if (mlocParseLocale("hu", 2, &hu).status != mloc_success ||
        mlocInitInflector(&s_hu, &s_data, &hu) != mloc_success)
    {
        fputs("test_grammar_hu: cannot make the inflector\n", stderr);
        return 2;
    }
    TestCases();
    TestStems();
    TestAssimilation();
    TestPossessives();
    TestMarked();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
