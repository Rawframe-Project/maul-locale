// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Language matching on the compiled-in data (record mloc-0017): CLDR's
// distances and region groups, the tie-breaks, demotion, the options,
// private use and pseudo locales, the combined locale, and the
// refusals. conform_match runs ICU's own cases in CI.

#include "test_harness.h"

#include "maul-locale/locale.h"

#include <string.h>

enum
{
    maxList = 16,
};

static mlocDataSet s_data;

typedef struct List
{
    size_t count;
    mlocLocale locales[maxList];
} List;

static List ListOf(const char* text)
{
    List list = {0};
    for (const char* at = text; *at != '\0' && list.count < maxList;)
    {
        size_t length = strcspn(at, ",");
        const char* begin = at;
        while (*begin == ' ')
        {
            begin++;
            length--;
        }
        if (mlocCanonicalizeLocale(&s_data, begin, length, &list.locales[list.count], nullptr)
                .status == mloc_success)
        {
            list.count++;
        }
        at += strcspn(at, ",");
        at += *at == ',';
    }
    return list;
}

// The match of desired among supported, the first supported locale the
// default; -2 on a failed call.
static mlocMatch MatchWith(const char* supported, const char* desired, mlocMatchFlags flags,
                           uint32_t threshold, int defaultIndex)
{
    List s = ListOf(supported);
    List d = ListOf(desired);
    mlocMatchEntry entries[maxList];
    mlocMatchOptions options = {.flags = flags, .threshold = threshold};
    mlocMatch match = {-2, -2};
    const mlocLocale* def = defaultIndex >= 0 ? &s.locales[defaultIndex] : nullptr;
    if (mlocPrepareMatch(&s_data, s.locales, s.count, def, entries) != mloc_success ||
        mlocMatchLocales(&s_data, s.locales, entries, s.count, d.locales, d.count, &options,
                         &match) != mloc_success)
    {
        return (mlocMatch){-2, -2};
    }
    return match;
}

static bool Is(mlocMatch match, int supported, int desired)
{
    return match.supported == supported && match.desired == desired;
}

#define MATCH(supported, desired) MatchWith(supported, desired, 0, 0, 0)

static void TestDistances(void)
{
    CHECK(Is(MATCH("fr, en-GB, en", "en-AU"), 1, 0), "en-AU is nearer en-GB than en-US");
    CHECK(Is(MATCH("fr, en, en-GB", "en-CA"), 1, 0), "en-CA is nearer en-US");
    CHECK(Is(MATCH("es, es-419", "es-MX"), 1, 0), "es-MX is nearer es-419 than es");
    CHECK(Is(MATCH("zh, zh-Hant", "zh-TW"), 1, 0), "zh-TW matches Traditional Chinese");
    CHECK(Is(MATCH("en, zh-Hant", "zh"), -1, -1), "zh never matches Traditional Chinese");
    CHECK(Is(MATCH("en, nb", "no"), 1, 0), "Norwegian matches Bokmål");
    CHECK(Is(MATCH("fr, de", "ja"), -1, -1), "nothing near enough");
    CHECK(Is(MATCH("fr, en", "af"), 1, 0), "a one-way rule: Afrikaans reads English");
    CHECK(Is(MATCH("fr, af", "en"), -1, -1), "but not the other way");
    CHECK(Is(MATCH("de-AT, de-DE, de-CH", "de"), 1, 0), "the likely region first");
    CHECK(Is(MATCH("", "en"), -1, -1), "no supported locales");
    CHECK(Is(MATCH("en", ""), -1, -1), "no desired locales");
}

static void TestOrder(void)
{
    CHECK(Is(MATCH("en, fr", "fr, en"), 1, 0), "the first desired locale wins");
    CHECK(Is(MATCH("es-419, fr", "es-MX, fr"), 0, 0), "a near match is not demoted away");
    CHECK(Is(MatchWith("es-419, fr", "es-MX, fr", mloc_matchNoDemotion, 0, 0), 1, 1),
          "without demotion the exact later one wins");
    CHECK(Is(MatchWith("en-IN, en-AU", "en-ZA", 0, 0, 0), 0, 0), "a tie goes to the default");
    CHECK(Is(MatchWith("en-IN, en-AU", "en-ZA", 0, 0, 1), 1, 0), "whichever it is");
    CHECK(Is(MatchWith("en-IN, en-AU", "en-ZA", 0, 0, -1), 0, 0), "else to the first given");
    CHECK(Is(MATCH("en-IN, en-GB", "en-ZA"), 1, 0), "a paradigm locale is nearer");
}

static void TestOptions(void)
{
    CHECK(Is(MATCH("ru, fr", "zh, pl"), -1, -1), "languages too far apart");
    CHECK(Is(MatchWith("ru, fr", "zh, pl", mloc_matchFavorScript, 0, 0), 1, 1),
          "favoring the script matches pl with fr");
    CHECK(Is(MatchWith("da, ru, hr", "sr", mloc_matchFavorScript, 0, 0), 1, 0),
          "and sr with ru, not hr");
    CHECK(Is(MatchWith("fr, en", "af", mloc_matchBothWays, 0, 0), -1, -1),
          "both ways leaves out the one-way rules");
    // af-ZA to en-US: 20 for the languages, 4 for the regions.
    CHECK(Is(MatchWith("fr, en", "af", 0, 24, 0), -1, -1), "a threshold the distance meets");
    CHECK(Is(MatchWith("fr, en", "af", 0, 25, 0), 1, 0), "and one it stays below");
    CHECK(Is(MatchWith("en, ja", "fr", 0, 100, 0), 0, 0), "the largest lets languages match");
}

static void TestSpecial(void)
{
    CHECK(Is(MATCH("fr, x-bork", "x-bork"), 1, 0), "a private use tag matches itself");
    CHECK(Is(MATCH("fr, x-bork", "x-piglatin"), -1, -1), "and no other");
    CHECK(Is(MATCH("it, und", "und"), 1, 0), "und matches und");
    CHECK(Is(MATCH("it, und", "en"), -1, -1), "but not English");
    CHECK(Is(MATCH("it, en", "und"), -1, -1), "nor English und");
    CHECK(Is(MATCH("en-XA, en", "en"), 1, 0), "a pseudo locale is not English");
    CHECK(Is(MATCH("en, en-XA", "en-XA"), 1, 0), "it matches itself");
    CHECK(Is(MATCH("en, en-PSACCENT", "en-XA"), 1, 0), "and its variant form");
}

static bool Combines(const char* desired, const char* supported, const char* expected)
{
    mlocLocale d;
    mlocLocale s;
    mlocLocale got;
    mlocLocale want;
    return mlocParseLocale(desired, strlen(desired), &d).status == mloc_success &&
           mlocParseLocale(supported, strlen(supported), &s).status == mloc_success &&
           mlocParseLocale(expected, strlen(expected), &want).status == mloc_success &&
           mlocCombineMatch(&d, &s, &got) == mloc_success && memcmp(&got, &want, sizeof(got)) == 0;
}

static void TestCombine(void)
{
    CHECK(Combines("ar-SA-u-nu-latn", "ar-EG", "ar-SA-u-nu-latn"), "region and extensions");
    CHECK(Combines("nn-BE-fonipa", "no", "no-BE-fonipa"), "region and variants");
    CHECK(Combines("fr", "fr-CA", "fr-CA"), "a supported region stays");
    CHECK(Combines("sco-fonupa", "sco-ulster-fonipa", "sco-fonupa"), "variants replaced whole");
    CHECK(Combines("th-u-nu-thai", "th-u-ca-buddhist-nu-latn", "th-u-nu-thai"),
          "an extension replaced whole");
    CHECK(Combines("en-fonipa-u-nu-arab-ca-buddhist-t-m0-iso-i0-pinyin", "en-GB-u-sd-gbsct",
                   "en-GB-fonipa-t-m0-iso-i0-pinyin-u-ca-buddhist-nu-arab"),
          "ICU's own case");
    CHECK(Combines("de-x-a-b", "de-CH-u-co-phonebk", "de-CH-u-co-phonebk-x-a-b"),
          "private use kept whole");
    mlocLocale d;
    mlocLocale s;
    static const char s_long1[] =
        "de-u-ca-buddhist-co-phonebk-nu-arab-x-aaaaaaaa-bbbbbbbb-cccccccc-d";
    static const char s_long2[] = "de-t-ja-latn-m0-ungegn-h0-hybrid-s0-ascii-k0-extended";
    CHECK(mlocParseLocale(s_long1, strlen(s_long1), &d).status == mloc_success &&
              mlocParseLocale(s_long2, strlen(s_long2), &s).status == mloc_success,
          "two long tails");
    mlocLocale out = d;
    CHECK(mlocCombineMatch(&d, &s, &out) == mloc_errorCapacity &&
              memcmp(&out, &d, sizeof(out)) == 0,
          "too long together, the output unchanged");
    CHECK(mlocCombineMatch(&d, &d, &d) == mloc_success, "in place");
    CHECK(mlocCombineMatch(nullptr, &s, &out) == mloc_errorInvalid &&
              mlocCombineMatch(&d, nullptr, &out) == mloc_errorInvalid &&
              mlocCombineMatch(&d, &s, nullptr) == mloc_errorInvalid,
          "null pointers");
    mlocLocale bad = d;
    memcpy(bad.tail, "fonipa-%-x", 11);
    CHECK(mlocCombineMatch(&bad, &s, &out) == mloc_errorInvalid, "a tail no parse made");
}

static void TestRefusals(void)
{
    List s = ListOf("en, fr");
    List d = ListOf("fr");
    mlocMatchEntry entries[2];
    mlocMatch match = {7, 7};
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocPrepareMatch(nullptr, s.locales, 2, nullptr, entries) == mloc_errorInvalid,
          "null data");
    CHECK(mlocPrepareMatch(&s_data, nullptr, 2, nullptr, entries) == mloc_errorInvalid &&
              mlocPrepareMatch(&s_data, s.locales, 2, nullptr, nullptr) == mloc_errorInvalid,
          "null lists with a count");
    CHECK(mlocPrepareMatch(&s_data, nullptr, 0, nullptr, nullptr) == mloc_success,
          "but none without one");
    CHECK(mlocPrepareMatch(&empty, s.locales, 2, nullptr, entries) == mloc_errorNotFound,
          "data without the tables");
    CHECK(mlocPrepareMatch(&s_data, s.locales, 2, nullptr, entries) == mloc_success, "prepared");
    mlocMatchOptions options = {.flags = 8};
    CHECK(mlocMatchLocales(&s_data, s.locales, entries, 2, d.locales, 1, &options, &match) ==
              mloc_errorInvalid,
          "another flag");
    options = (mlocMatchOptions){.threshold = 101};
    CHECK(mlocMatchLocales(&s_data, s.locales, entries, 2, d.locales, 1, &options, &match) ==
              mloc_errorInvalid,
          "a threshold above 100");
    CHECK(mlocMatchLocales(&s_data, s.locales, entries + 1, 1, d.locales, 1, nullptr, &match) ==
              mloc_errorInvalid,
          "entries of another count");
    CHECK(mlocMatchLocales(&s_data, s.locales, entries, 2, nullptr, 1, nullptr, &match) ==
                  mloc_errorInvalid &&
              mlocMatchLocales(&s_data, s.locales, entries, 2, d.locales, 1, nullptr, nullptr) ==
                  mloc_errorInvalid &&
              mlocMatchLocales(&s_data, nullptr, entries, 2, d.locales, 1, nullptr, &match) ==
                  mloc_errorInvalid,
          "null pointers");
    CHECK(mlocMatchLocales(&empty, s.locales, entries, 2, d.locales, 1, nullptr, &match) ==
              mloc_errorNotFound,
          "data without the tables");
    CHECK(match.supported == 7 && match.desired == 7, "the outcome left unchanged");
    mlocMatchEntry spoiled[2];
    memcpy(spoiled, entries, sizeof(spoiled));
    memset(&spoiled[1], 0xFF, sizeof(spoiled[1]));
    CHECK(mlocMatchLocales(&s_data, s.locales, spoiled, 2, d.locales, 1, nullptr, &match) ==
              mloc_errorInvalid,
          "an entry mlocPrepareMatch did not make");
    CHECK(mlocMatchLocales(&s_data, s.locales, entries, 2, d.locales, 1, nullptr, &match) ==
                  mloc_success &&
              Is(match, 1, 0),
          "and the right ones match");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestDistances();
    TestOrder();
    TestOptions();
    TestSpecial();
    TestCombine();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
