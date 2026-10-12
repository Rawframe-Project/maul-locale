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
    CHECK(Gives("und-Armn-SU", "und-Armn-AM", mloc_changedRegion), "und with a script");
    CHECK(Gives("hy-Zzzz-SU", "hy-Zzzz-AM", mloc_changedRegion), "Zzzz as no script");
    CHECK(Gives("abcdefg-SU", "abcdefg-RU", mloc_changedRegion),
          "the first for a language too long to look up");
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

static void TestExtensions(void)
{
    mlocLocaleChanges x = mloc_changedExtensions;
    CHECK(Gives("en-u-ms-imperial", "en-u-ms-uksystem", x), "a u value's alias");
    CHECK(Gives("en-u-ca-ethiopic-amete-alem", "en-u-ca-ethioaa", x), "a value of three subtags");
    CHECK(Gives("en-u-ca-islamicc", "en-u-ca-islamic-civil", x), "a deprecated value");
    CHECK(Gives("en-u-ca-islamic-civil", "en-u-ca-islamic-civil", 0),
          "its preferred value is not turned back");
    CHECK(Gives("en-u-kb-yes", "en-u-kb", x), "a value that becomes true is removed");
    CHECK(Gives("en-u-tz-cnckg", "en-u-tz-cnsha", x), "a deprecated zone");
    CHECK(Gives("en-u-tz-cet", "en-u-tz-bebru", x), "a zone's alias");
    CHECK(Gives("en-u-rg-fi01", "en-u-rg-axzzzz", x), "a subdivision that became a region");
    CHECK(Gives("en-u-sd-frbl", "en-u-sd-blzzzz", x), "and for sd");
    CHECK(Gives("en-u-attr-ca-islamicc", "en-u-attr-ca-islamic-civil", x), "attributes kept");
    CHECK(Gives("und-t-iw", "und-t-he", x), "a t extension's language");
    CHECK(Gives("en-t-sh-m0-ies-jes", "en-t-sr-latn-m0-iesjes", x),
          "its language and a field value");
    CHECK(Gives("en-t-d0-name", "en-t-d0-charname", x), "a t value's alias");
    CHECK(Gives("iw-u-ms-imperial-x-foo", "he-u-ms-uksystem-x-foo", mloc_changedLanguage | x),
          "the language and an extension");
    CHECK(Gives("en-u-ca-gregory-a-foo-x-ms-imperial", "en-a-foo-u-ca-gregory-x-ms-imperial", 0),
          "other extensions and private use kept");
    CHECK(Gives("en-t-iw-k0-true", "en-t-he-k0-true", x), "a t value true stays");
    CHECK(Gives("en-a-foo-u-ms-imperial", "en-a-foo-u-ms-uksystem", x),
          "a u extension after another");
    CHECK(Gives("en-a-foo-t-iw", "en-a-foo-t-he", x), "and a t extension");
    CHECK(Gives("en-xyzzy-u-ms-imperial", "en-xyzzy-u-ms-uksystem", x),
          "a variant that begins with x is no private use");
    CHECK(Gives("en-u-ms-imperial-x-u-ms-imperial", "en-u-ms-uksystem-x-u-ms-imperial", x),
          "private use is not read as extensions");
    // A tail of 110 bytes grows past 112 when islamicc becomes
    // islamic-civil.
    char text[256] = "en-u-ca-islamicc-x";
    size_t length = strlen(text);
    while (length < 3 + 110 - 2)
    {
        memcpy(text + length, "-ab", 4);
        length += 3;
    }
    mlocLocale locale;
    mlocTextResult result = mlocCanonicalizeLocale(&s_data, text, length, &locale, nullptr);
    CHECK(result.status == mloc_errorCapacity && result.offset == length,
          "a value that grows past the tail");
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
    // 252 bytes fit, but not with "und-" put before them.
    memcpy(text, "x-", 2);
    for (size_t at = 2; at + 9 <= 252; at += 9)
    {
        text[at + 8] = '-';
    }
    text[251] = 'a';
    mlocTextResult grown = mlocCanonicalizeLocale(&s_data, text, 252, &locale, nullptr);
    CHECK(grown.status == mloc_errorCapacity && grown.offset == 252, "a tag that und- makes 256");
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
    TestExtensions();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
