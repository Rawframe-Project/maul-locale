// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Likely subtags on the compiled-in data: adding and removing them, the
// lookup order of UTS #35, und, Zzzz and ZZ, the variants and extensions
// kept, and the refusals; a locale's direction from its script or its
// likely script. CLDR's whole conformance file runs in CI
// (conform_likely), and every locale's character order
// (conform_direction).

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
}

// The output the input itself, or a locale's bytes sharing part of the
// input's from either side: refused, every byte left as it was.
static void TestAliasing(void)
{
    enum
    {
        size = sizeof(mlocLocale),
    };
    // The output the input itself, overlapping it by most to one byte,
    // or right beside it.
    static const size_t s_overlapping[] = {0, size / 4, size / 2, 3 * size / 4, size - 1};
    static unsigned char s_bytes[2 * size];
    mlocLocale ja = Parse("ja");
    bool refused = true;
    bool beside = true;
    for (size_t i = 0; i <= sizeof s_overlapping / sizeof s_overlapping[0]; i++)
    {
        size_t shift = i < sizeof s_overlapping / sizeof s_overlapping[0] ? s_overlapping[i] : size;
        for (int side = 0; side < 2; side++)
        {
            // A locale is bytes alone, so one at any offset is aligned.
            unsigned char* in = s_bytes + (side == 0 ? 0 : shift);
            unsigned char* out = s_bytes + (side == 0 ? shift : 0);
            memset(s_bytes, 0xA5, sizeof s_bytes);
            memcpy(in, &ja, sizeof ja);
            unsigned char before[sizeof s_bytes];
            memcpy(before, s_bytes, sizeof s_bytes);
            mlocResult maximized =
                mlocMaximizeLocale(&s_data, (const mlocLocale*)in, (mlocLocale*)out);
            if (shift < size)
            {
                refused = refused && maximized == mloc_errorInvalid &&
                          mlocMinimizeLocale(&s_data, (const mlocLocale*)in, mloc_favorRegion,
                                             (mlocLocale*)out) == mloc_errorInvalid &&
                          memcmp(before, s_bytes, sizeof s_bytes) == 0;
                continue;
            }
            beside = beside && maximized == mloc_success && Same((mlocLocale*)out, "ja-Jpan-JP") &&
                     mlocMinimizeLocale(&s_data, (const mlocLocale*)in, mloc_favorRegion,
                                        (mlocLocale*)out) == mloc_success &&
                     Same((mlocLocale*)out, "ja") && memcmp(in, &ja, sizeof ja) == 0;
        }
    }
    CHECK(refused, "an output that is or overlaps the input refused, nothing written");
    CHECK(beside, "an output right beside the input, on either side, written");
}

static void TestMinimize(void)
{
    CHECK(Minimizes("zh-Hant-TW", mloc_favorRegion, "zh-TW"), "favoring the region");
    CHECK(Minimizes("zh-Hant-TW", mloc_favorScript, "zh-Hant"), "favoring the script");
    CHECK(Minimizes("en-Latn-US", mloc_favorRegion, "en"), "to the language alone");
    CHECK(Minimizes("en-Latn-GB", mloc_favorRegion, "en-GB"), "a region that is not likely");
    CHECK(Minimizes("en-US-u-ca-buddhist", mloc_favorRegion, "en-u-ca-buddhist"),
          "extensions are kept");
    CHECK(Minimizes("sr-Latn", mloc_favorScript, "sr-Latn"), "a script that is not likely");
    CHECK(Minimizes("und", mloc_favorRegion, "en"), "und");
}

static void TestRefusals(void)
{
    mlocLocale unknown = Parse("qaa");
    mlocLocale out = Parse("fr");
    mlocLocale kept = out;
    CHECK(mlocMaximizeLocale(&s_data, &unknown, &out) == mloc_errorNotFound &&
              memcmp(&kept, &out, sizeof(kept)) == 0,
          "a language the data lacks, the output unchanged");
    mlocLocale longer = Parse("abcde");
    CHECK(mlocMaximizeLocale(&s_data, &longer, &out) == mloc_errorNotFound,
          "a language too long for the table");
    CHECK(mlocMinimizeLocale(&s_data, &unknown, mloc_favorRegion, &out) == mloc_errorNotFound &&
              memcmp(&kept, &out, sizeof(kept)) == 0,
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

static bool Direction(const char* text, mlocDirection expected)
{
    mlocLocale locale = Parse(text);
    mlocDirection direction = mloc_directionUnknown;
    return mlocGetLocaleDirection(&s_data, &locale, &direction) == mloc_success &&
           direction == expected;
}

static void TestDirection(void)
{
    const mlocDirection ltr = mloc_directionLeftToRight;
    const mlocDirection rtl = mloc_directionRightToLeft;
    CHECK(Direction("ar", rtl) && Direction("he", rtl) && Direction("fa-IR", rtl) &&
              Direction("dv", rtl) && Direction("nqo", rtl) && Direction("ff-Adlm", rtl),
          "Arabic, Hebrew, Thaana, N'Ko and Adlam are right to left");
    CHECK(Direction("en", ltr) && Direction("und", ltr) && Direction("ja", ltr) &&
              Direction("mn-Mong", ltr),
          "Latin, Japanese and Mongolian are left to right");
    CHECK(Direction("ar-Latn", ltr) && Direction("ks-Deva", ltr) && Direction("en-Arab", rtl) &&
              Direction("pa-Arab", rtl) && Direction("ar-Zzzz", rtl),
          "a script named decides; Zzzz is none");
    CHECK(Direction("qaa-Hebr", rtl) && Direction("qaa-Adlm-US", rtl) &&
              Direction("en-Avst", rtl) && Direction("en-Yezi", rtl) && Direction("en-Adlm", rtl) &&
              Direction("en-Zyyy", ltr),
          "the first and last scripts of the list, and a script without likely data");
    mlocLocale unknown = Parse("qaa");
    mlocDirection direction = mloc_directionUnknown;
    CHECK(mlocGetLocaleDirection(&s_data, &unknown, &direction) == mloc_errorNotFound &&
              direction == mloc_directionUnknown,
          "a language without a likely script has no direction, the result untouched");
    mlocDataSet empty = {nullptr, 0};
    mlocLocale en = Parse("en");
    mlocLocale enLatn = Parse("en-Latn");
    CHECK(mlocGetLocaleDirection(&empty, &en, &direction) == mloc_errorNotFound &&
              mlocGetLocaleDirection(&empty, &enLatn, &direction) == mloc_success &&
              direction == mloc_directionLeftToRight,
          "without data only a named script gives a direction");
    CHECK(mlocGetLocaleDirection(nullptr, &en, &direction) == mloc_errorInvalid &&
              mlocGetLocaleDirection(&s_data, nullptr, &direction) == mloc_errorInvalid &&
              mlocGetLocaleDirection(&s_data, &en, nullptr) == mloc_errorInvalid,
          "null pointers are refused");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestMaximize();
    TestMinimize();
    TestRefusals();
    TestAliasing();
    TestDirection();
    return s_failures == 0 ? 0 : 1;
}
