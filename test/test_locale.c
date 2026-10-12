// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale ids: the grammar of UTS #35, canonical syntax, the refusals
// with their offsets, the fixed-size limits, and writing a tag back.

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <string.h>

// Parses text and writes it back; returns whether it gives expected.
static bool Gives(const char* text, const char* expected)
{
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale(text, strlen(text), &locale);
    if (result.status != mloc_success || result.offset != strlen(text))
    {
        return false;
    }
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return mlocFormatLocale(&locale, &output) == mloc_success &&
           output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
}

// Parses text and expects a refusal with a status at an offset.
static bool Refuses(const char* text, mlocResult status, size_t offset)
{
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale(text, strlen(text), &locale);
    return result.status == status && result.offset == offset;
}

static void TestLanguageIds(void)
{
    CHECK(Gives("en", "en"), "a language");
    CHECK(Gives("EN-us", "en-US"), "case is canonical");
    CHECK(Gives("zh_hant_tw", "zh-Hant-TW"), "underscores and title-case scripts");
    CHECK(Gives("es-419", "es-419"), "a numeric region");
    CHECK(Gives("root", "und"), "root is und");
    CHECK(Gives("und", "und"), "und");
    CHECK(Gives("abcdefgh", "abcdefgh"), "an eight-letter language");
    CHECK(Gives("sl-rozaj-biske-1994", "sl-1994-biske-rozaj"), "variants in order");
    CHECK(Gives("de-CH-1901", "de-CH-1901"), "a digit-first variant of four");
    CHECK(Gives("en-abcdef-abcde", "en-abcde-abcdef"), "a variant before one it begins");
}

static void TestFields(void)
{
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale("sr-latn-rs-u-ca-buddhist", 24, &locale);
    CHECK(result.status == mloc_success, "a full tag");
    CHECK(memcmp(locale.language, "sr\0\0\0\0\0\0", 8) == 0, "the language, NUL-padded");
    CHECK(memcmp(locale.script, "Latn", 4) == 0, "the script");
    CHECK(memcmp(locale.region, "RS\0\0", 4) == 0, "the region");
    CHECK(strcmp(locale.tail, "u-ca-buddhist") == 0, "the tail");
    mlocLocale same;
    result = mlocParseLocale("SR_Latn_RS_U_CA_BUDDHIST", 24, &same);
    CHECK(result.status == mloc_success && memcmp(&locale, &same, sizeof(locale)) == 0,
          "equal locales are equal byte for byte");
}

static void TestExtensions(void)
{
    CHECK(Gives("en-u-nu-thai-ca-buddhist", "en-u-ca-buddhist-nu-thai"), "keys in order");
    CHECK(Gives("en-u-foo-bar-nu-thai", "en-u-bar-foo-nu-thai"), "attributes in order, first");
    CHECK(Gives("en-u-ca-true", "en-u-ca"), "a value true is removed");
    CHECK(Gives("en-u-ca-true-abc", "en-u-ca-true-abc"), "true as one of several values stays");
    CHECK(Gives("en-u-kn", "en-u-kn"), "a key without a value");
    CHECK(Gives("en-u-1a-abc", "en-u-1a-abc"), "a key may begin with a digit");
    CHECK(Gives("en-u-co-phonebk-ca-islamic-civil", "en-u-ca-islamic-civil-co-phonebk"),
          "a key keeps its several values in order");
    CHECK(Gives("en-x-foo-u-ca-gregory", "en-x-foo-u-ca-gregory"),
          "private use takes everything after it");
    CHECK(Gives("en-u-ca-gregory-t-it-a-bc", "en-a-bc-t-it-u-ca-gregory"),
          "extensions by singleton");
    CHECK(Gives("en-z-zz-x-a-b", "en-z-zz-x-a-b"), "private use last, after z");
    CHECK(Gives("en-t-ZH-hant-TW-fonipa-1994-m0-ungegn", "en-t-zh-hant-tw-1994-fonipa-m0-ungegn"),
          "a transformed language in lowercase, its variants in order");
    CHECK(Gives("und-t-m0-names-h0-hybrid", "und-t-h0-hybrid-m0-names"), "fields in order");
    CHECK(Gives("en-t-k0-dvorak", "en-t-k0-dvorak"), "fields without a language");
    CHECK(Gives("root-u-ca-dangi", "und-u-ca-dangi"), "root with extensions");
}

static void TestRefusals(void)
{
    CHECK(Refuses("", mloc_errorSyntax, 0), "empty");
    CHECK(Refuses("en-", mloc_errorSyntax, 3), "a trailing separator");
    CHECK(Refuses("en--US", mloc_errorSyntax, 3), "an empty subtag");
    CHECK(Refuses("e", mloc_errorSyntax, 0), "a language of one letter");
    CHECK(Refuses("abcd", mloc_errorSyntax, 0), "a tag that begins with a script");
    CHECK(Refuses("e1", mloc_errorSyntax, 0), "a language with a digit");
    CHECK(Refuses("en-US-abcdefghi", mloc_errorSyntax, 6), "a subtag of nine");
    CHECK(Refuses("en-U\xC3\xA9", mloc_errorSyntax, 4), "a byte no subtag has");
    CHECK(Refuses("en US", mloc_errorSyntax, 2), "a space");
    CHECK(Refuses("en-fonipa-US", mloc_errorSyntax, 10), "a region after a variant");
    CHECK(Refuses("de-1996-fonipa-1996", mloc_errorSyntax, 15), "a duplicate variant");
    CHECK(Refuses("en-US-ab", mloc_errorSyntax, 6), "a subtag that fits nowhere");
    CHECK(Refuses("en-a1b", mloc_errorSyntax, 3), "a region of three that are not digits");
    CHECK(Refuses("root-Latn", mloc_errorSyntax, 5), "root takes no script");
    CHECK(Refuses("en-u", mloc_errorSyntax, 3), "an empty u extension");
    CHECK(Refuses("en-u-ca-buddhist-ca-islamic", mloc_errorSyntax, 17), "a duplicate key");
    CHECK(Refuses("en-u-foo-foo", mloc_errorSyntax, 9), "a duplicate attribute");
    CHECK(Refuses("en-u-a1", mloc_errorSyntax, 5), "a key whose second character is a digit");
    CHECK(Refuses("en-u-ca-buddhist-u-nu-thai", mloc_errorSyntax, 17), "a singleton twice");
    CHECK(Refuses("en-t", mloc_errorSyntax, 3), "an empty t extension");
    CHECK(Refuses("en-t-k0", mloc_errorSyntax, 5), "a field without a value");
    CHECK(Refuses("en-t-k0-abc-ka-abc", mloc_errorSyntax, 12), "a field key without its digit");
    CHECK(Refuses("en-t-k0-ab", mloc_errorSyntax, 5), "a field value of two");
    CHECK(Refuses("en-t-it-fonipa-fonipa", mloc_errorSyntax, 15),
          "a duplicate variant of the transformed language");
    CHECK(Refuses("en-t-m0-names-m0-ungegn", mloc_errorSyntax, 14), "a duplicate field");
    CHECK(Refuses("en-a", mloc_errorSyntax, 3), "an empty other extension");
    CHECK(Refuses("en-a-b", mloc_errorSyntax, 3), "a singleton straight after a singleton");
    CHECK(Refuses("en-x", mloc_errorSyntax, 3), "empty private use");
}

// Writes start followed by count copies of piece into text, NUL-ended.
static void Repeat(char* text, const char* start, const char* piece, int count)
{
    size_t at = strlen(start);
    memcpy(text, start, at);
    for (int i = 0; i < count; i++)
    {
        memcpy(text + at, piece, strlen(piece));
        at += strlen(piece);
    }
    text[at] = '\0';
}

static void TestLimits(void)
{
    char text[512];
    Repeat(text, "en-x", "-abcdefgh", 13);
    CHECK(Refuses(text, mloc_errorCapacity, strlen(text)),
          "variants and extensions past 112 bytes");
    Repeat(text, "en-x", "-a", 63);
    CHECK(Refuses(text, mloc_errorCapacity, 4 + 62 * 2 + 1), "more than 64 subtags");
    // 112 bytes exactly: "x" and 37 subtags "-ab" make 1 + 37 * 3 = 112.
    Repeat(text, "en-x", "-ab", 37);
    mlocLocale locale;
    CHECK(mlocParseLocale(text, strlen(text), &locale).status == mloc_success &&
              locale.tail[111] == 'b',
          "a tail of exactly 112 bytes");
    Repeat(text, "en-x", "-ab", 38);
    CHECK(Refuses(text, mloc_errorCapacity, strlen(text)), "one subtag more");
}

static void TestArgumentsAndOutput(void)
{
    mlocLocale locale;
    CHECK(mlocParseLocale(nullptr, 2, &locale).status == mloc_errorInvalid, "null text");
    CHECK(mlocParseLocale("en", 2, nullptr).status == mloc_errorInvalid, "a null locale");
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale to write");
    mlocOutput measure = {0};
    CHECK(mlocFormatLocale(&locale, &measure) == mloc_errorCapacity && measure.length == 2,
          "a null buffer measures");
    char small[1];
    mlocOutput shortOutput = {.buffer = small, .capacity = 1};
    CHECK(mlocFormatLocale(&locale, &shortOutput) == mloc_errorCapacity && shortOutput.length == 2,
          "a short buffer gives the length needed");
    mlocOutput bad = {.buffer = nullptr, .capacity = 4};
    CHECK(mlocFormatLocale(&locale, &bad) == mloc_errorInvalid, "a null buffer with capacity");
    CHECK(mlocFormatLocale(nullptr, &measure) == mloc_errorInvalid, "a null locale to write");
    CHECK(mlocFormatLocale(&locale, nullptr) == mloc_errorInvalid, "a null output");
    mlocLocale kept = locale;
    CHECK(mlocParseLocale("en-", 3, &locale).status == mloc_errorSyntax &&
              memcmp(&kept, &locale, sizeof(locale)) == 0,
          "a refusal leaves the locale unchanged");
}

int main(void)
{
    TestLanguageIds();
    TestFields();
    TestExtensions();
    TestRefusals();
    TestLimits();
    TestArgumentsAndOutput();
    return s_failures == 0 ? 0 : 1;
}
