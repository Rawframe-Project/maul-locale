// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a failed call of maul-locale/locale.h leaves in its outputs: each
// output filled with a pattern, each call made to fail in each way the
// compiled-in data lets it, and every byte of the pattern checked after.
// A failure leaves its outputs as they were (base.h), but for the length
// mloc_errorCapacity gives an mlocOutput.

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <stdint.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static mlocDataSet s_data;
static mlocDataSet s_empty;

static void Fill(void* output, size_t size)
{
    memset(output, pattern, size);
}

static bool Kept(const void* output, size_t size)
{
    const unsigned char* bytes = output;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != pattern)
        {
            return false;
        }
    }
    return true;
}

static mlocLocale Parse(const char* text)
{
    mlocLocale locale = {0};
    CHECK(mlocParseLocale(text, strlen(text), &locale).status == mloc_success,
          "a test locale parses");
    return locale;
}

// A tag whose variants pass MLOC_MAX_LOCALE_TAIL bytes.
static size_t LongTag(char* text, size_t capacity)
{
    size_t length = 0;
    memcpy(text, "en", 2);
    length = 2;
    for (char c = 'a'; c <= 'z' && length + 9 < capacity; c++)
    {
        text[length++] = '-';
        memset(text + length, c, 8);
        length += 8;
    }
    return length;
}

static void TestParseAndFormat(void)
{
    mlocLocale locale;
    char text[256];
    size_t length = LongTag(text, sizeof text);
    Fill(&locale, sizeof locale);
    CHECK(mlocParseLocale("en-", 3, &locale).status == mloc_errorSyntax &&
              mlocParseLocale(text, length, &locale).status == mloc_errorLimit &&
              mlocParseLocale(nullptr, 2, &locale).status == mloc_errorInvalid &&
              Kept(&locale, sizeof locale),
          "a parse refused for its syntax, a limit or a null text leaves the locale");
    char buffer[16];
    Fill(buffer, sizeof buffer);
    mlocOutput output = {.buffer = buffer, .capacity = sizeof buffer, .length = 99};
    CHECK(mlocFormatLocale(nullptr, &output) == mloc_errorInvalid && output.length == 99 &&
              Kept(buffer, sizeof buffer),
          "a format refused leaves the output");
}

static void TestCanonicalize(void)
{
    mlocLocale locale;
    mlocLocaleChanges changes;
    char text[MLOC_MAX_LOCALE_TEXT + 2];
    memset(text, 'a', sizeof text);
    Fill(&locale, sizeof locale);
    Fill(&changes, sizeof changes);
    CHECK(mlocCanonicalizeLocale(&s_data, "en-", 3, &locale, &changes).status == mloc_errorSyntax &&
              mlocCanonicalizeLocale(&s_data, text, sizeof text, &locale, &changes).status ==
                  mloc_errorLimit &&
              mlocCanonicalizeLocale(&s_empty, "en", 2, &locale, &changes).status ==
                  mloc_errorNotFound &&
              mlocCanonicalizeLocale(nullptr, "en", 2, &locale, &changes).status ==
                  mloc_errorInvalid &&
              Kept(&locale, sizeof locale) && Kept(&changes, sizeof changes),
          "a canonicalization refused leaves the locale and the changes");
}

static void TestLikely(void)
{
    mlocLocale unknown = Parse("qaa");
    mlocLocale en = Parse("en");
    mlocLocale out;
    Fill(&out, sizeof out);
    CHECK(mlocMaximizeLocale(&s_data, &unknown, &out) == mloc_errorNotFound &&
              mlocMaximizeLocale(&s_empty, &en, &out) == mloc_errorNotFound &&
              mlocMaximizeLocale(nullptr, &en, &out) == mloc_errorInvalid &&
              mlocMinimizeLocale(&s_data, &unknown, mloc_favorRegion, &out) == mloc_errorNotFound &&
              mlocMinimizeLocale(&s_data, &en, (mlocFavor)2, &out) == mloc_errorInvalid &&
              Kept(&out, sizeof out),
          "a maximize or minimize refused leaves the locale");
    mlocDirection direction;
    Fill(&direction, sizeof direction);
    CHECK(mlocGetLocaleDirection(&s_data, &unknown, &direction) == mloc_errorNotFound &&
              mlocGetLocaleDirection(&s_empty, &en, &direction) == mloc_errorNotFound &&
              mlocGetLocaleDirection(nullptr, &en, &direction) == mloc_errorInvalid &&
              Kept(&direction, sizeof direction),
          "a direction refused leaves the direction");
}

static void TestParentAndPreferences(void)
{
    mlocLocale root = Parse("root");
    mlocLocale en = Parse("en");
    mlocLocale parent;
    Fill(&parent, sizeof parent);
    CHECK(mlocGetParentLocale(&s_data, mloc_componentMain, &root, &parent) == mloc_errorNotFound &&
              mlocGetParentLocale(&s_empty, mloc_componentMain, &en, &parent) ==
                  mloc_errorNotFound &&
              mlocGetParentLocale(&s_data, (mlocComponent)99, &en, &parent) == mloc_errorInvalid &&
              Kept(&parent, sizeof parent),
          "a parent refused leaves the parent");
    mlocLocalePreferences preferences;
    Fill(&preferences, sizeof preferences);
    CHECK(mlocGetLocalePreferences(&s_empty, &en, &preferences) == mloc_errorNotFound &&
              mlocGetLocalePreferences(nullptr, &en, &preferences) == mloc_errorInvalid &&
              Kept(&preferences, sizeof preferences),
          "preferences refused leave the preferences");
}

static void TestMatch(void)
{
    mlocLocale supported[2] = {Parse("en"), Parse("fr")};
    mlocLocale desired = Parse("fr-CA");
    mlocMatchEntry entries[2];
    Fill(entries, sizeof entries);
    // Past the limit, refused before any locale or entry is read.
    size_t tooMany = (size_t)MLOC_MAX_MATCH_LOCALES + 1;
    CHECK(mlocPrepareMatch(&s_empty, supported, 2, nullptr, entries) == mloc_errorNotFound &&
              mlocPrepareMatch(&s_data, supported, tooMany, nullptr, entries) == mloc_errorLimit &&
              mlocPrepareMatch(&s_data, nullptr, 2, nullptr, entries) == mloc_errorInvalid &&
              Kept(entries, sizeof entries),
          "a preparation refused leaves the entries");
    CHECK(mlocPrepareMatch(&s_data, supported, 2, nullptr, entries) == mloc_success,
          "entries to match with");
    mlocMatch match;
    Fill(&match, sizeof match);
    mlocMatchOptions high = {.threshold = 101};
    CHECK(mlocMatchLocales(&s_empty, supported, entries, 2, &desired, 1, nullptr, &match) ==
                  mloc_errorNotFound &&
              mlocMatchLocales(&s_data, supported, entries, tooMany, &desired, 1, nullptr,
                               &match) == mloc_errorLimit &&
              mlocMatchLocales(&s_data, supported, entries, 2, &desired, 1, &high, &match) ==
                  mloc_errorInvalid &&
              Kept(&match, sizeof match),
          "a match refused leaves the match");
    static const char s_long1[] =
        "de-u-ca-buddhist-co-phonebk-nu-arab-x-aaaaaaaa-bbbbbbbb-cccccccc-d";
    static const char s_long2[] = "de-t-ja-latn-m0-ungegn-h0-hybrid-s0-ascii-k0-extended";
    mlocLocale one = Parse(s_long1);
    mlocLocale two = Parse(s_long2);
    mlocLocale combined;
    Fill(&combined, sizeof combined);
    CHECK(mlocCombineMatch(&one, &two, &combined) == mloc_errorLimit &&
              mlocCombineMatch(nullptr, &two, &combined) == mloc_errorInvalid &&
              Kept(&combined, sizeof combined),
          "a combination refused leaves the locale");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    s_empty = (mlocDataSet){nullptr, 0};
    TestParseAndFormat();
    TestCanonicalize();
    TestLikely();
    TestParentAndPreferences();
    TestMatch();
    return s_failures == 0 ? 0 : 1;
}
