// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Display names (maul-locale/names.h): each type and form, subdivisions
// included, a code in any case, the fallbacks, the capitalization
// contexts, the output protocol and every refusal. Every code of every
// locale agrees with ICU4C 79's LocaleDisplayNames (conform_names).

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/names.h"

#include <stdio.h>
#include <string.h>

static mlocData s_sources[3];
static mlocDataSet s_data;

// Whether a locale's name of a code is as expected, with the status.
static bool Named(const char* id, mlocNameType type, const char* code, mlocNameForm form,
                  mlocCapitalization capitalization, mlocResult status, const char* expected)
{
    mlocLocale locale;
    mlocNameOptions options = {.form = form, .capitalization = capitalization};
    char buffer[128];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocResult r = mlocParseLocale(id, strlen(id), &locale).status == mloc_success
                       ? mlocGetName(&s_data, &locale, type, code, strlen(code), &options, &output)
                       : mloc_errorInvalid;
    bool same = r == status && output.length == strlen(expected) &&
                memcmp(buffer, expected, output.length) == 0;
    if (!same)
    {
        printf("%s %s: %d \"%.*s\"\n", id, code, (int)r, (int)output.length, buffer);
    }
    return same;
}

static bool Plain(const char* id, mlocNameType type, const char* code, const char* expected)
{
    return Named(id, type, code, mloc_nameFormStandard, mloc_capitalizationNone, mloc_success,
                 expected);
}

static void TestTypes(void)
{
    CHECK(Plain("en", mloc_nameLanguage, "en-GB", "British English") &&
              Plain("en", mloc_nameLanguage, "EN_gb", "British English") &&
              Plain("en", mloc_nameScript, "latn", "Latin") &&
              Plain("en", mloc_nameRegion, "us", "United States") &&
              Plain("en", mloc_nameRegion, "419", "Latin America") &&
              Plain("en", mloc_nameVariant, "VALENCIA", "Valencian") &&
              Plain("en", mloc_nameKey, "CA", "Calendar") &&
              Plain("en", mloc_nameKeyValue, "ca-Gregory", "Gregorian Calendar") &&
              Plain("en", mloc_nameMeasurementSystem, "uksystem", "UK") &&
              Plain("de", mloc_nameRegion, "DE", "Deutschland") &&
              Plain("en", mloc_nameScript, "ZZZZ", "Unknown Script") &&
              Plain("en", mloc_nameRegion, "zz", "Unknown Region") &&
              Plain("en", mloc_nameVariant, "1901", "Traditional German orthography"),
          "each type, codes in any case (Z and z among them), a variant of a digit and three");
    CHECK(Plain("af", mloc_nameLanguage, "tl", "Filippyns") &&
              Plain("az", mloc_nameLanguage, "tl", "taqaloq"),
          "a deprecated code by its alias where the chain lacks its own name");
    CHECK(Named("en", mloc_nameLanguage, "en-GB", mloc_nameFormShort, mloc_capitalizationNone,
                mloc_success, "UK English") &&
              Named("en", mloc_nameScript, "Hans", mloc_nameFormStandAlone, mloc_capitalizationNone,
                    mloc_success, "Simplified Han") &&
              Named("en", mloc_nameKeyValue, "ca-gregory", mloc_nameFormCore,
                    mloc_capitalizationNone, mloc_success, "Gregorian") &&
              Named("en", mloc_nameLanguage, "fr", mloc_nameFormShort, mloc_capitalizationNone,
                    mloc_success, "French"),
          "the forms, a form the chain lacks giving the name");
    CHECK(Plain("en", mloc_nameSubdivision, "GBSCT", "Scotland") &&
              Plain("en-AU", mloc_nameSubdivision, "usca", "California") &&
              Plain("en", mloc_nameSubdivision, "arc", "Buenos Aires") &&
              Plain("fr", mloc_nameSubdivision, "gbsct",
                    "\xC3\x89"
                    "cosse") &&
              Named("fr", mloc_nameSubdivision, "USCA", mloc_nameFormStandard,
                    mloc_capitalizationListOrMenu, mloc_fellBack, "usca") &&
              Named("fr", mloc_nameSubdivision, "usca", mloc_nameFormStandard,
                    mloc_capitalizationSentence, mloc_fellBack, "Usca") &&
              Named("en", mloc_nameSubdivision, "0011", mloc_nameFormStandard,
                    mloc_capitalizationNone, mloc_fellBack, "0011"),
          "subdivisions: English's names inherited, a code of three, French's own, the code where "
          "CLDR's is "
          "provisional, titlecased only at a sentence's start, a region of digits");
}

static void TestFallback(void)
{
    mlocLocale en;
    mlocNameOptions none = {.fallback = mloc_nameFallbackNone};
    char buffer[32];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(Named("en", mloc_nameLanguage, "QQQ", mloc_nameFormStandard, mloc_capitalizationNone,
                mloc_fellBack, "qqq") &&
              Named("en", mloc_nameLanguage, "en-latn-gb", mloc_nameFormStandard,
                    mloc_capitalizationNone, mloc_fellBack, "en-Latn-GB") &&
              Named("en", mloc_nameKeyValue, "ca-zzzz", mloc_nameFormStandard,
                    mloc_capitalizationNone, mloc_fellBack, "zzzz") &&
              Named("en", mloc_nameScript, "qaaa", mloc_nameFormStandard, mloc_capitalizationNone,
                    mloc_fellBack, "Qaaa"),
          "a missing name gives the code in canonical case, a value's for a key and value");
    char longest[200];
    memset(longest, 'a', sizeof(longest));
    memcpy(longest, "en-", 3);
    for (size_t i = 11; i < sizeof(longest); i += 9)
    {
        longest[i] = '-';
    }
    CHECK(Named("en", mloc_nameLanguage, "en-scouse-fonipa", mloc_nameFormStandard,
                mloc_capitalizationNone, mloc_fellBack, "en-fonipa-scouse") &&
              mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocGetName(&s_data, &en, mloc_nameLanguage, longest, sizeof(longest), nullptr,
                          &output) == mloc_errorSyntax,
          "variants in order; a language id longer than the library reads refused");
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocGetName(&s_data, &en, mloc_nameLanguage, "qqq", 3, &none, &output) ==
                  mloc_errorNotFound,
          "no fallback: not found");
    // Variants that fill the locale's tail, all 112 bytes and no NUL.
    char full[3 + MLOC_MAX_LOCALE_TAIL + 1] = "en-1901";
    for (char letter = 'a'; letter <= 'l'; letter++)
    {
        size_t at = strlen(full);
        full[at] = '-';
        memset(full + at + 1, letter, 8);
        full[at + 9] = '\0';
    }
    CHECK(strlen(full) == 3 + MLOC_MAX_LOCALE_TAIL &&
              Named("en", mloc_nameLanguage, full, mloc_nameFormStandard, mloc_capitalizationNone,
                    mloc_fellBack, full),
          "a language id whose variants fill the tail falls back to itself");
}

static void TestCapitalization(void)
{
    CHECK(Named("fr", mloc_nameLanguage, "fr", mloc_nameFormStandard, mloc_capitalizationSentence,
                mloc_success, "Français") &&
              Named("fr", mloc_nameLanguage, "fr", mloc_nameFormStandard,
                    mloc_capitalizationListOrMenu, mloc_success, "Français") &&
              Named("fr", mloc_nameLanguage, "fr", mloc_nameFormStandard,
                    mloc_capitalizationStandAlone, mloc_success, "français") &&
              Named("fr", mloc_nameScript, "Latn", mloc_nameFormStandard,
                    mloc_capitalizationListOrMenu, mloc_success, "latin") &&
              Named("fr", mloc_nameLanguage, "qqq", mloc_nameFormStandard,
                    mloc_capitalizationSentence, mloc_fellBack, "Qqq"),
          "titlecased at a sentence's start always, in a menu as French's data says, a code too");
    CHECK(Named("fr", mloc_nameLanguage, "de", mloc_nameFormStandard, mloc_capitalizationSentence,
                mloc_success, "Allemand") &&
              Named("pt", mloc_nameLanguage, "en", mloc_nameFormStandard,
                    mloc_capitalizationStandAlone, mloc_success, "Ingl\xC3\xAAs") &&
              Named("en", mloc_nameLanguage, "en", mloc_nameFormStandard,
                    mloc_capitalizationSentence, mloc_success, "English") &&
              Named("en", mloc_nameKey, "q", mloc_nameFormStandard, mloc_capitalizationSentence,
                    mloc_fellBack, "Q"),
          "a first a, Portuguese alone, a capital kept, a code of one letter");
    CHECK(Named("az", mloc_nameLanguage, "en", mloc_nameFormStandard, mloc_capitalizationSentence,
                mloc_success, "\xC4\xB0ngilis") &&
              Named("az", mloc_nameLanguage, "de", mloc_nameFormStandard,
                    mloc_capitalizationSentence, mloc_success, "Alman") &&
              Named("en", mloc_nameMeasurementSystem, "metric", mloc_nameFormStandard,
                    mloc_capitalizationListOrMenu, mloc_success, "Metric"),
          "Azerbaijani's dotted capital I; a measurement system by the sentence rule alone");
    CHECK(
        Named("ru", mloc_nameLanguage, "en", mloc_nameFormStandard, mloc_capitalizationSentence,
              mloc_success,
              "\xD0\x90\xD0\xBD\xD0\xB3\xD0\xBB\xD0\xB8\xD0\xB9\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9") &&
            Named("zh", mloc_nameLanguage, "en", mloc_nameFormStandard, mloc_capitalizationSentence,
                  mloc_success, "\xE8\x8B\xB1\xE8\xAF\xAD"),
        "a letter beyond ASCII titlecased by the table, a character without a titlecase kept");
}

static void TestOutput(void)
{
    mlocLocale en;
    char small[4];
    mlocOutput tight = {.buffer = small, .capacity = sizeof(small)};
    mlocOutput measure = {0};
    char exact[13];
    mlocOutput fits = {.buffer = exact, .capacity = sizeof(exact)};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, nullptr, &tight) ==
                  mloc_errorCapacity &&
              tight.length == 13 &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, nullptr, &measure) ==
                  mloc_errorCapacity &&
              measure.length == 13 &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, nullptr, &fits) == mloc_success &&
              fits.length == 13 && memcmp(exact, "United States", 13) == 0,
          "a buffer too small or none gives the size needed; one of that size takes it");
}

static void TestRefusals(void)
{
    mlocLocale en;
    char buffer[32];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    static const char* const s_bad[][2] = {
        {"0", "e"},           {"0", "en-u-ca-roc"}, {"0", ""},      {"1", "Lat"},
        {"1", "La1n"},        {"2", "USA"},         {"2", "4x9"},   {"3", "abcd"},
        {"3", "abcdefghi"},   {"4", "cal"},         {"5", "ca"},    {"5", "ca-ab"},
        {"5", "caa-gregory"}, {"5", "ca-gregory-"}, {"6", "ab"},    {"6", "uk-system"},
        {"7", "gb"},          {"7", "gbsctxxx"},    {"7", "g1sct"}, {"7", "123"},
        {"7", "gb-ct"},
    };
    bool refused = mlocParseLocale("en", 2, &en).status == mloc_success;
    for (size_t i = 0; refused && i < sizeof(s_bad) / sizeof(s_bad[0]); i++)
    {
        refused = mlocGetName(&s_data, &en, (mlocNameType)(s_bad[i][0][0] - '0'), s_bad[i][1],
                              strlen(s_bad[i][1]), nullptr, &output) == mloc_errorSyntax;
        if (!refused)
        {
            printf("not refused: %s\n", s_bad[i][1]);
        }
    }
    CHECK(refused, "codes not of their type's syntax");
    mlocNameOptions badForm = {.form = (mlocNameForm)8};
    mlocNameOptions badFallback = {.fallback = (mlocNameFallback)2};
    mlocNameOptions badCase = {.capitalization = (mlocCapitalization)4};
    mlocOutput parts = {.buffer = buffer, .capacity = sizeof(buffer), .part = (mlocPartFunction)1};
    mlocOutput noBuffer = {.capacity = 4};
    mlocDataSet builtinAlone = {s_sources, 1};
    CHECK(mlocGetName(nullptr, &en, mloc_nameRegion, "US", 2, nullptr, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, nullptr, mloc_nameRegion, "US", 2, nullptr, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, nullptr, 2, nullptr, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, nullptr, &parts) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, nullptr, &noBuffer) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, (mlocNameType)8, "US", 2, nullptr, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, &badForm, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, &badFallback, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&s_data, &en, mloc_nameRegion, "US", 2, &badCase, &output) ==
                  mloc_errorInvalid &&
              mlocGetName(&builtinAlone, &en, mloc_nameRegion, "US", 2, nullptr, &output) ==
                  mloc_errorNotFound,
          "null pointers, a part function, values out of range, data without the names");
}

// Whether a locale's name in a display locale is as expected.
static bool LocaleNamed(const char* display, const char* id, mlocLanguageDisplay mode,
                        mlocNameForm form, mlocCapitalization capitalization, mlocResult status,
                        const char* expected)
{
    mlocLocale in;
    mlocLocale named;
    mlocLocaleNameOptions options = {mode, form, capitalization};
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocResult r = mlocParseLocale(display, strlen(display), &in).status == mloc_success &&
                           mlocParseLocale(id, strlen(id), &named).status == mloc_success
                       ? mlocGetLocaleName(&s_data, &in, &named, &options, &output)
                       : mloc_errorInvalid;
    bool same = r == status && output.length == strlen(expected) &&
                memcmp(buffer, expected, output.length) == 0;
    if (!same)
    {
        printf("%s %s: %d \"%.*s\"\n", display, id, (int)r, (int)output.length, buffer);
    }
    return same;
}

static bool Dialect(const char* id, const char* expected)
{
    return LocaleNamed("en", id, mloc_languageDisplayDialect, mloc_nameFormStandard,
                       mloc_capitalizationNone, mloc_success, expected);
}

static void TestLocaleName(void)
{
    CHECK(Dialect("en-GB", "British English") && Dialect("nl-BE", "Flemish") &&
              Dialect("es-Cyrl-MX", "Mexican Spanish (Cyrillic)") &&
              LocaleNamed("en", "en-GB", mloc_languageDisplayStandard, mloc_nameFormStandard,
                          mloc_capitalizationNone, mloc_success, "English (United Kingdom)") &&
              Dialect("en-MM", "English (Myanmar [Burma])"),
          "dialect and standard names, nested brackets");
    CHECK(Dialect("en-u-ca-buddhist", "English (Buddhist Calendar)") &&
              Dialect("en-u-cu-eur", "English (Currency: \xE2\x82\xAC)") &&
              Dialect("en-u-kr-digit-deva-latn",
                      "English (Script/Block Reordering: Digits, Devanagari, Latin)") &&
              Dialect("en-u-kn", "English (Sort Digits Numerically)") &&
              Dialect("en-t-h0-hybrid", "English") &&
              Dialect("hi-u-nu-latn-t-en-h0-hybrid", "Hindi (Western Digits, Hybrid: English)") &&
              Dialect("en-t-ja-latn-jp-m0-iso",
                      "English (Transform: Japanese, Latin, Japan, ISO Transliteration)") &&
              Dialect("fr-z-zz-zzz-v-vv-vvv-u-uu-uuu-t-ru-Cyrl-s-ss-sss-a-aa-aaa-x-u-x",
                      "French (uu: uuu, Transform: Russian, Cyrillic, a: aa-aaa, s: ss-sss, v: "
                      "vv-vvv, x: u-x, z: zz-zzz)"),
          "keys and values, currency symbols, reorder codes, a key's true, the t extension");
    CHECK(LocaleNamed("en", "qqq-US", mloc_languageDisplayDialect, mloc_nameFormStandard,
                      mloc_capitalizationNone, mloc_fellBack, "qqq (United States)") &&
              LocaleNamed("en", "en-GB", mloc_languageDisplayDialect, mloc_nameFormShort,
                          mloc_capitalizationNone, mloc_success, "UK English") &&
              LocaleNamed("en", "en-US", mloc_languageDisplayStandard, mloc_nameFormShort,
                          mloc_capitalizationNone, mloc_success, "English (US)") &&
              LocaleNamed("fr", "fr-CA", mloc_languageDisplayStandard, mloc_nameFormStandard,
                          mloc_capitalizationListOrMenu, mloc_success,
                          "Fran\xC3\xA7"
                          "ais (Canada)"),
          "a language without a name, the short form, a menu's capital");
    CHECK(Dialect("en-u-kr-zyyy", "English (Script/Block Reordering: Common)") &&
              LocaleNamed("en-ZA", "en-u-cu-zar", mloc_languageDisplayStandard,
                          mloc_nameFormStandard, mloc_capitalizationNone, mloc_success,
                          "English (Currency: R)") &&
              Dialect("en-u-cu-abcde", "English (Currency: ABCDE)") &&
              Dialect("en-u-cu-usd-eur", "English (Currency: USD-EUR)") &&
              LocaleNamed("fr", "fr-CA", mloc_languageDisplayStandard, mloc_nameFormStandard,
                          mloc_capitalizationStandAlone, mloc_success,
                          "fran\xC3\xA7"
                          "ais (Canada)"),
          "codes beginning with z, a currency without a symbol in capitals, alone");
    CHECK(Dialect("en-u-sd-gbsct", "English (Region Subdivision: Scotland)") &&
              Dialect("en-u-rg-gbeng", "English (Region For Supplemental Data: England)") &&
              Dialect("en-u-rg-uszzzz", "English (Region For Supplemental Data: uszzzz)"),
          "subdivisions by name, a region's rg value as written");
    mlocLocale en;
    mlocLocale gb;
    char small[4];
    mlocOutput tight = {.buffer = small, .capacity = sizeof(small)};
    mlocOutput parts = {.buffer = small, .capacity = sizeof(small), .part = (mlocPartFunction)1};
    mlocLocaleNameOptions badMode = {.languageDisplay = (mlocLanguageDisplay)2};
    mlocLocaleNameOptions badForm = {.form = (mlocNameForm)8};
    mlocLocaleNameOptions badCase = {.capitalization = (mlocCapitalization)4};
    mlocDataSet builtinAlone = {s_sources, 1};
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocParseLocale("en-GB", 5, &gb).status == mloc_success &&
              mlocGetLocaleName(&s_data, &en, &gb, nullptr, &tight) == mloc_errorCapacity &&
              tight.length == 15 &&
              mlocGetLocaleName(nullptr, &en, &gb, nullptr, &tight) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, nullptr, &gb, nullptr, &tight) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, &en, nullptr, nullptr, &tight) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, &en, &gb, nullptr, nullptr) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, &en, &gb, nullptr, &parts) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, &en, &gb, &badMode, &tight) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, &en, &gb, &badForm, &tight) == mloc_errorInvalid &&
              mlocGetLocaleName(&s_data, &en, &gb, &badCase, &tight) == mloc_errorInvalid &&
              mlocGetLocaleName(&builtinAlone, &en, &gb, nullptr, &tight) == mloc_errorNotFound,
          "the size needed, and every refusal");
    // A value the parser did not make: its fields full, with no NUL.
    mlocLocale full;
    memset(&full, 'a', sizeof(full));
    char buffer[512];
    mlocOutput wide = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocResult r = mlocGetLocaleName(&s_data, &en, &full, nullptr, &wide);
    CHECK(r == mloc_success || r == mloc_fellBack, "a locale value without a NUL read in bounds");
}

// Reads a data file into a buffer and opens it.
static bool OpenFile(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t read = file == nullptr ? 0 : fread(buffer, 1, capacity, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    return read > 0 && read < capacity && mlocOpenData(buffer, read, out) == mloc_success;
}

// The compiled-in data, then the names and subdivisions data files at the
// paths the build gives.
static bool OpenData(void)
{
    alignas(8) static uint8_t s_names[8 << 20];
    alignas(8) static uint8_t s_subdivisions[1 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 3};
    return OpenFile(MLOC_LOCALE_NAMES_FILE, s_names, sizeof(s_names), &s_sources[1]) &&
           OpenFile(MLOC_SUBDIVISIONS_FILE, s_subdivisions, sizeof(s_subdivisions), &s_sources[2]);
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_names: cannot open " MLOC_LOCALE_NAMES_FILE " or " MLOC_SUBDIVISIONS_FILE "\n",
              stderr);
        return 2;
    }
    TestTypes();
    TestFallback();
    TestCapitalization();
    TestOutput();
    TestRefusals();
    TestLocaleName();
    return s_failures == 0 ? 0 : 1;
}
