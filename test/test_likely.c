// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Likely subtags on the compiled-in data: adding and removing them, the
// lookup order of UTS #35, und, Zzzz and ZZ, the variants and extensions
// kept, and the refusals. CLDR's whole conformance file runs in CI
// (conform_likely).

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <string.h>

static mlocDataSet s_data;

static bool Same(const mlocLocale* locale, const char* expected)
{
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    return mlocFormatLocale(locale, &output) == mloc_success && output.length == strlen(expected) &&
           memcmp(tag, expected, output.length) == 0;
}

static mlocLocale Parse(const char* text)
{
    mlocLocale locale = {0};
    mlocTextResult result = mlocParseLocale(text, strlen(text), &locale);
    CHECK(result.status == mloc_success, "a test locale parses");
    return locale;
}

static bool Maximizes(const char* text, const char* expected)
{
    mlocLocale locale = Parse(text);
    mlocLocale result;
    return mlocMaximizeLocale(&s_data, &locale, &result) == mloc_success && Same(&result, expected);
}

static bool Minimizes(const char* text, mlocFavor favor, const char* expected)
{
    mlocLocale locale = Parse(text);
    mlocLocale result;
    return mlocMinimizeLocale(&s_data, &locale, favor, &result) == mloc_success &&
           Same(&result, expected);
}

static void TestMaximize(void)
{
    CHECK(Maximizes("en", "en-Latn-US"), "a language alone");
    CHECK(Maximizes("zh-TW", "zh-Hant-TW"), "a language and region");
    CHECK(Maximizes("zh-Hant", "zh-Hant-TW"), "a language and script");
    CHECK(Maximizes("sr-ME", "sr-Latn-ME"), "the region decides the script");
    CHECK(Maximizes("und", "en-Latn-US"), "und");
    CHECK(Maximizes("und-TW", "zh-Hant-TW"), "und with a region");
    CHECK(Maximizes("und-Cyrl", "ru-Cyrl-RU"), "und with a script");
    CHECK(Maximizes("de-Latn-AT", "de-Latn-AT"), "a full locale stays");
    CHECK(Maximizes("qaa-Cyrl-CH", "qaa-Cyrl-CH"), "a full locale stays without a lookup");
    CHECK(Maximizes("und-419", "es-Latn-419"), "a numeric region kept");
    CHECK(Maximizes("eo", "eo-Latn-001"), "a numeric region from the table");
    CHECK(Maximizes("tr-Zzzz-ZZ", "tr-Latn-TR"), "Zzzz and ZZ are dropped");
    CHECK(Maximizes("de-1901-u-co-phonebk", "de-Latn-DE-1901-u-co-phonebk"),
          "variants and extensions are kept");
    mlocLocale locale = Parse("ja");
    CHECK(mlocMaximizeLocale(&s_data, &locale, &locale) == mloc_success &&
              Same(&locale, "ja-Jpan-JP"),
          "in place");
}

static void TestMinimize(void)
{
    CHECK(Minimizes("zh-Hant-TW", mloc_favorRegion, "zh-TW"), "favoring the region");
    CHECK(Minimizes("zh-Hant-TW", mloc_favorScript, "zh-Hant"), "favoring the script");
    CHECK(Minimizes("en-Latn-US", mloc_favorRegion, "en"), "to the language alone");
    CHECK(Minimizes("en-US-u-ca-buddhist", mloc_favorRegion, "en-u-ca-buddhist"),
          "extensions are kept");
    CHECK(Minimizes("sr-Latn", mloc_favorScript, "sr-Latn"), "a script that is not likely");
    CHECK(Minimizes("und", mloc_favorRegion, "en"), "und");
}

static void TestRefusals(void)
{
    mlocLocale unknown = Parse("qaa");
    mlocLocale kept = unknown;
    CHECK(mlocMaximizeLocale(&s_data, &unknown, &unknown) == mloc_errorNotFound &&
              memcmp(&kept, &unknown, sizeof(kept)) == 0,
          "a language the data lacks, the locale unchanged");
    mlocLocale longer = Parse("abcde");
    CHECK(mlocMaximizeLocale(&s_data, &longer, &longer) == mloc_errorNotFound,
          "a language too long for the table");
    CHECK(mlocMinimizeLocale(&s_data, &unknown, mloc_favorRegion, &unknown) == mloc_errorNotFound,
          "minimizing what cannot be maximized");
    mlocDataSet empty = {nullptr, 0};
    mlocLocale en = Parse("en");
    mlocLocale result;
    CHECK(mlocMaximizeLocale(&empty, &en, &result) == mloc_errorNotFound,
          "a data set without the table");
    CHECK(mlocMinimizeLocale(&empty, &en, mloc_favorRegion, &result) == mloc_errorNotFound,
          "and to minimize");
    CHECK(mlocMaximizeLocale(nullptr, &en, &result) == mloc_errorInvalid, "a null data set");
    CHECK(mlocMaximizeLocale(&s_data, nullptr, &result) == mloc_errorInvalid, "a null locale");
    CHECK(mlocMaximizeLocale(&s_data, &en, nullptr) == mloc_errorInvalid, "a null result");
    CHECK(mlocMinimizeLocale(&s_data, &en, 2, &result) == mloc_errorInvalid, "another favor");
    CHECK(mlocMinimizeLocale(nullptr, &en, mloc_favorRegion, &result) == mloc_errorInvalid,
          "a null data set to minimize");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestMaximize();
    TestMinimize();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
