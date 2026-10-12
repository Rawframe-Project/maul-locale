// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Fallback chains on the compiled-in data (record mloc-0016): keys made
// with likely subtags, CLDR's explicit parents, the non-likely script
// rule in the main component only, variants and extensions, and the
// refusals. conform_parents checks every CLDR locale in CI.

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <string.h>

static mlocDataSet s_data;

// Whether the chain of text in a component is the tags given, ending at
// und.
static bool Chain(mlocComponent component, const char* text, const char* const* expected)
{
    mlocLocale locale;
    if (mlocParseLocale(text, strlen(text), &locale).status != mloc_success)
    {
        return false;
    }
    for (int i = 0; expected[i] != nullptr; i++)
    {
        mlocLocale parent;
        char tag[256];
        mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
        if (mlocGetParentLocale(&s_data, component, &locale, &parent) != mloc_success ||
            mlocFormatLocale(&parent, &output) != mloc_success ||
            output.length != strlen(expected[i]) || memcmp(tag, expected[i], output.length) != 0)
        {
            return false;
        }
        locale = parent;
    }
    mlocLocale none;
    return mlocGetParentLocale(&s_data, component, &locale, &none) == mloc_errorNotFound;
}

#define CHAIN(component, text, ...)                                                                \
    Chain(component, text, (const char* const[]){__VA_ARGS__, nullptr})

static void TestMain(void)
{
    mlocComponent m = mloc_componentMain;
    CHECK(CHAIN(m, "de-AT", "de", "und"), "truncation");
    CHECK(CHAIN(m, "en-AU", "en-001", "en", "und"), "an explicit parent");
    CHECK(CHAIN(m, "es-MX", "es-419", "es", "und"), "Latin American Spanish");
    CHECK(CHAIN(m, "pt-MO", "pt-PT", "pt", "und"), "European Portuguese");
    CHECK(CHAIN(m, "zh-TW", "zh-Hant", "und"), "zh-TW never reaches zh");
    CHECK(CHAIN(m, "zh-Hant-HK", "zh-Hant", "und"), "nor does zh-Hant-HK");
    CHECK(CHAIN(m, "zh-Hans-CN", "zh", "und"), "the likely script is left out");
    CHECK(CHAIN(m, "sr-Cyrl-BA", "sr", "und"), "sr-Cyrl is sr");
    CHECK(CHAIN(m, "sr-Latn-ME", "sr-Latn", "und"), "a non-likely script goes to root");
    CHECK(CHAIN(m, "ru-Latn", "und"), "even one CLDR does not list");
    CHECK(CHAIN(m, "hi-Latn", "en-IN", "en-001", "en", "und"), "a parent of another language");
    CHECK(CHAIN(m, "de-CH-1996-fonipa", "de-CH-1996", "de-CH", "de", "und"),
          "variants dropped from the end");
    CHECK(CHAIN(m, "qaa-Cyrl-CH", "qaa-Cyrl", "und"), "a language without likely subtags");
    CHECK(CHAIN(m, "und-Cyrl", "und"), "und with a script is not root yet");
}

static void TestComponents(void)
{
    mlocComponent c = mloc_componentCollation;
    CHECK(CHAIN(c, "zh-Hant-TW", "zh-Hant", "zh", "und"), "collation keeps zh");
    CHECK(CHAIN(c, "yue", "zh-Hant", "zh", "und"), "collation's own parents");
    CHECK(CHAIN(c, "en-AU", "en", "und"), "the main parents do not apply");
    CHECK(CHAIN(mloc_componentPlurals, "zh-Hant", "zh", "und"), "plurals truncate");
    CHECK(CHAIN(mloc_componentGrammar, "en-AU", "en", "und"), "grammar too");
}

static void TestExtensions(void)
{
    mlocLocale locale;
    CHECK(mlocParseLocale("en-AU-u-ca-buddhist-x-foo", 25, &locale).status == mloc_success,
          "a tag with extensions");
    mlocLocale parent;
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    CHECK(mlocGetParentLocale(&s_data, mloc_componentMain, &locale, &parent) == mloc_success &&
              mlocFormatLocale(&parent, &output) == mloc_success &&
              output.length == strlen("en-001-u-ca-buddhist-x-foo") &&
              memcmp(tag, "en-001-u-ca-buddhist-x-foo", output.length) == 0,
          "the extensions are kept");
    CHECK(mlocParseLocale("de-1996-fonipa-u-co-phonebk", 27, &locale).status == mloc_success &&
              mlocGetParentLocale(&s_data, mloc_componentMain, &locale, &parent) == mloc_success,
          "variants and extensions");
    output.capacity = sizeof(tag);
    CHECK(mlocFormatLocale(&parent, &output) == mloc_success &&
              output.length == strlen("de-1996-u-co-phonebk") &&
              memcmp(tag, "de-1996-u-co-phonebk", output.length) == 0,
          "after the parent's variants");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocLocale parent;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocGetParentLocale(&s_data, 5, &locale, &parent) == mloc_errorInvalid,
          "another component");
    CHECK(mlocGetParentLocale(nullptr, 0, &locale, &parent) == mloc_errorInvalid, "null data");
    CHECK(mlocGetParentLocale(&s_data, 0, nullptr, &parent) == mloc_errorInvalid, "a null locale");
    CHECK(mlocGetParentLocale(&s_data, 0, &locale, nullptr) == mloc_errorInvalid, "a null parent");
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocGetParentLocale(&empty, 0, &locale, &parent) == mloc_errorNotFound,
          "data without parents");
    CHECK(mlocParseLocale("root", 4, &locale).status == mloc_success &&
              mlocGetParentLocale(&s_data, 0, &locale, &parent) == mloc_errorNotFound,
          "root has no parent");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestMain();
    TestComponents();
    TestExtensions();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
