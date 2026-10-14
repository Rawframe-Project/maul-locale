// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Display names (maul-locale/names.h): each type and form, a code in any
// case, the fallbacks, the capitalization contexts, the output protocol
// and every refusal. Every code of every locale agrees with ICU4C 79's
// LocaleDisplayNames (conform_names, record mloc-0037).

#include "test_harness.h"

#include "maul-locale/data.h"
#include "maul-locale/names.h"

#include <stdio.h>
#include <string.h>

static mlocData s_sources[2];
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
        {"0", "e"},           {"0", "en-u-ca-roc"}, {"0", ""},    {"1", "Lat"},
        {"1", "La1n"},        {"2", "USA"},         {"2", "4x9"}, {"3", "abcd"},
        {"3", "abcdefghi"},   {"4", "cal"},         {"5", "ca"},  {"5", "ca-ab"},
        {"5", "caa-gregory"}, {"5", "ca-gregory-"}, {"6", "ab"},  {"6", "uk-system"},
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
              mlocGetName(&s_data, &en, (mlocNameType)7, "US", 2, nullptr, &output) ==
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

// The compiled-in data, then the names data file at the path the build
// gives.
static bool OpenData(void)
{
    alignas(8) static uint8_t s_names[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    FILE* file = fopen(MLOC_LOCALE_NAMES_FILE, "rb");
    size_t read = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 2};
    return read > 0 && read < sizeof(s_names) &&
           mlocOpenData(&s_sources[1], s_names, read) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_names: cannot open " MLOC_LOCALE_NAMES_FILE "\n", stderr);
        return 2;
    }
    TestTypes();
    TestFallback();
    TestCapitalization();
    TestOutput();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
