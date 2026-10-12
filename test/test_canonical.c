// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Canonicalization on the compiled-in data: legacy tags, extlangs, tags
// that begin with "x" or a script, each kind of alias, a region chosen
// among several by likely subtags, what is reported as changed, and the
// refusals with offsets into the text given. CLDR's whole conformance
// file runs in CI (conform_canonical).

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <string.h>

static mlocDataSet s_data;

// Canonicalizes text; returns whether it gives expected with exactly
// the changes given.
static bool Gives(const char* text, const char* expected, mlocLocaleChanges changes)
{
    mlocLocale locale;
    mlocLocaleChanges got = 0xFFFF;
    mlocTextResult result = mlocCanonicalizeLocale(&s_data, text, strlen(text), &locale, &got);
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    return result.status == mloc_success && result.offset == strlen(text) && got == changes &&
           mlocFormatLocale(&locale, &output) == mloc_success &&
           output.length == strlen(expected) && memcmp(tag, expected, output.length) == 0;
}

static bool Refuses(const char* text, mlocResult status, size_t offset)
{
    mlocLocale locale;
    mlocTextResult result = mlocCanonicalizeLocale(&s_data, text, strlen(text), &locale, nullptr);
    return result.status == status && result.offset == offset;
}

static void TestUnchanged(void)
{
    CHECK(Gives("en-US", "en-US", 0), "a canonical tag");
    CHECK(Gives("EN_us", "en-US", 0), "syntax alone is not reported");
    CHECK(Gives("de-u-co-phonebk", "de-u-co-phonebk", 0), "extensions are kept");
    CHECK(Gives("abcdefgh-Latn", "abcdefgh-Latn", 0), "a long language matches no alias");
}

static void TestLanguageAliases(void)
{
    CHECK(Gives("iw", "he", mloc_changedLanguage), "a deprecated language");
    CHECK(Gives("heb", "he", mloc_changedLanguage), "an overlong language");
    CHECK(Gives("sh", "sr-Latn", mloc_changedLanguage | mloc_changedScript),
          "a language that brings a script");
    CHECK(Gives("sh-Cyrl", "sr-Cyrl", mloc_changedLanguage), "a script given is kept");
    CHECK(Gives("art-lojban", "jbo", mloc_changedLanguage | mloc_changedVariants),
          "a language with a variant");
    CHECK(Gives("no-bokmal-nynorsk", "nb", mloc_changedLanguage | mloc_changedVariants),
          "rules applied until none matches");
    CHECK(Gives("iw-IL-u-ca-hebrew", "he-IL-u-ca-hebrew", mloc_changedLanguage),
          "a region and extensions kept");
}

static void TestOtherAliases(void)
{
    CHECK(Gives("en-Qaai", "en-Zinh", mloc_changedScript), "a script alias");
    CHECK(Gives("de-DD", "de-DE", mloc_changedRegion), "a region alias");
    CHECK(Gives("en-840", "en-US", mloc_changedRegion), "a numeric region");
    CHECK(Gives("el-polytoni", "el-polyton", mloc_changedVariants), "a variant alias");
    CHECK(Gives("ja-Latn-hepburn-heploc", "ja-Latn-alalc97", mloc_changedVariants),
          "two variants to one");
    CHECK(Gives("en-aaland", "en-AX", mloc_changedRegion | mloc_changedVariants),
          "a variant that brings a region");
    CHECK(Gives("ja-alalc97-hepburn-heploc", "ja-alalc97", mloc_changedVariants),
          "a replacement variant already there");
    CHECK(Gives("en-heploc-scouse", "en-alalc97-scouse", mloc_changedVariants),
          "replaced variants in order");
}

static void TestRegionChoice(void)
{
    CHECK(Gives("ru-SU", "ru-RU", mloc_changedRegion), "the language's likely region");
    CHECK(Gives("hy-SU", "hy-AM", mloc_changedRegion), "another language, another region");
    CHECK(Gives("en-SU", "en-RU", mloc_changedRegion), "the first region otherwise");
    CHECK(Gives("und-SU", "und-RU", mloc_changedRegion), "und");
    CHECK(Gives("sr-CS", "sr-RS", mloc_changedRegion), "Serbia and Montenegro");
    CHECK(Gives("az-Arab-SU", "az-Arab-RU", mloc_changedRegion),
          "the script counts: az-Arab is likely in IR, not one of them");
}

static void TestBcp47(void)
{
    CHECK(Gives("i-klingon", "tlh", mloc_changedLegacy), "a legacy tag");
    CHECK(Gives("I_KLINGON", "tlh", mloc_changedLegacy), "in any case and separator");
    CHECK(Gives("zh-min-nan", "nan", mloc_changedLegacy), "a legacy tag with three");
    CHECK(Gives("zh-min", "nan-x-zh-min", mloc_changedLegacy), "a legacy tag into private use");
    CHECK(Gives("zh-yue-HK", "yue-HK", mloc_changedExtlang), "an extlang");
    CHECK(Gives("sgn-ase", "ase", mloc_changedExtlang), "an extlang after three letters");
    CHECK(Gives("x-private", "und-x-private", 0), "private use alone");
    CHECK(Gives("Latn-US", "und-Latn-US", 0), "a tag that begins with a script");
}

static void TestRefusals(void)
{
    CHECK(Refuses("en-", mloc_errorSyntax, 3), "a syntax error at its offset");
    CHECK(Refuses("zh-yue-", mloc_errorSyntax, 7), "an offset past a removed extlang");
    CHECK(Refuses("Latn-", mloc_errorSyntax, 5), "an offset past an added und");
    CHECK(Refuses("i-klingonx", mloc_errorSyntax, 0), "no legacy tag");
    char text[300];
    memset(text, 'a', sizeof(text));
    mlocLocale locale;
    CHECK(mlocCanonicalizeLocale(&s_data, text, 256, &locale, nullptr).status == mloc_errorCapacity,
          "256 bytes are too many");
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocCanonicalizeLocale(&empty, "en", 2, &locale, nullptr).status == mloc_errorNotFound,
          "data without the aliases");
    CHECK(mlocCanonicalizeLocale(nullptr, "en", 2, &locale, nullptr).status == mloc_errorInvalid,
          "a null data set");
    CHECK(mlocCanonicalizeLocale(&s_data, nullptr, 2, &locale, nullptr).status == mloc_errorInvalid,
          "null text");
    CHECK(mlocCanonicalizeLocale(&s_data, "en", 2, nullptr, nullptr).status == mloc_errorInvalid,
          "a null locale");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestUnchanged();
    TestLanguageAliases();
    TestOtherAliases();
    TestRegionChoice();
    TestBcp47();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
