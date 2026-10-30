// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Language matching on the compiled-in data: CLDR's distances and region
// groups, the tie-breaks, demotion, the options, private use and pseudo
// locales, the combined locale, and the refusals. conform_match runs ICU's
// own cases in CI.

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
    CHECK(Is(MATCH("zh-Hant-TW, zh-Hans-TW", "zh-Hani-TW"), 1, 0),
          "in a tie the likely script wins over the one tried first");
    CHECK(Is(MatchWith("ja, abcde, de-AT", "fr", 0, 100, 0), 1, 0), "a tie of two languages");
    CHECK(Is(MatchWith("es-AR, es-419", "es-MX", 0, 0, -1), 1, 0), "a paradigm locale tried first");
    CHECK(Is(MatchWith("fr, de, en", "fr", 0, 0, -1), 0, 0), "the others kept behind it");
    CHECK(Is(MatchWith("ja, de-AT, abcde", "fr", 0, 100, 0), 1, 0),
          "goes to the first tried, a long language not being a paradigm one");
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
    CHECK(Is(MatchWith("fr, en-GB", "en-AU", mloc_matchBothWays, 0, 0), 1, 0),
          "and keeps the others");
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
    CHECK(Combines("fr-a-bcd-z-efg", "fr-CA-u-ca-buddhist", "fr-CA-a-bcd-u-ca-buddhist-z-efg"),
          "the first and last singletons");
    mlocLocale d;
    mlocLocale s;
    static const char s_long1[] =
        "de-u-ca-buddhist-co-phonebk-nu-arab-x-aaaaaaaa-bbbbbbbb-cccccccc-d";
    static const char s_long2[] = "de-t-ja-latn-m0-ungegn-h0-hybrid-s0-ascii-k0-extended";
    CHECK(mlocParseLocale(s_long1, strlen(s_long1), &d).status == mloc_success &&
              mlocParseLocale(s_long2, strlen(s_long2), &s).status == mloc_success,
          "two long tails");
    mlocLocale out = d;
    CHECK(mlocCombineMatch(&d, &s, &out) == mloc_errorLimit && memcmp(&out, &d, sizeof(out)) == 0,
          "too long together, the output unchanged");
    // The output either input itself, or overlapping either: refused,
    // every byte left as it was. A locale is bytes alone, so one at any
    // offset is aligned.
    static unsigned char s_bytes[3 * sizeof(mlocLocale)];
    mlocLocale de;
    mlocLocale en;
    mlocLocale apart;
    bool all = mlocParseLocale("de-u-co-phonebk", 15, &de).status == mloc_success &&
               mlocParseLocale("en-GB", 5, &en).status == mloc_success &&
               mlocCombineMatch(&de, &en, &apart) == mloc_success;
    for (size_t shift = 0; shift < sizeof(mlocLocale); shift += sizeof(mlocLocale) / 4)
    {
        for (int which = 0; which < 2; which++)
        {
            for (int side = 0; side < 2; side++)
            {
                memset(s_bytes, 0xA5, sizeof s_bytes);
                // The input the output may meet, the other input apart.
                unsigned char* met = s_bytes + sizeof(mlocLocale) + (side == 0 ? 0 : shift);
                unsigned char* target = s_bytes + sizeof(mlocLocale) + (side == 0 ? shift : 0);
                memcpy(met, which == 0 ? &de : &en, sizeof(mlocLocale));
                mlocLocale other = which == 0 ? en : de;
                unsigned char before[sizeof s_bytes];
                memcpy(before, s_bytes, sizeof s_bytes);
                const mlocLocale* desired = which == 0 ? (const mlocLocale*)met : &other;
                const mlocLocale* supported = which == 0 ? &other : (const mlocLocale*)met;
                all = all &&
                      mlocCombineMatch(desired, supported, (mlocLocale*)target) ==
                          mloc_errorInvalid &&
                      memcmp(before, s_bytes, sizeof s_bytes) == 0;
            }
        }
    }
    CHECK(all, "an output that is or overlaps either input refused, nothing written");
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
    // Refused before a locale is read: the lists here are short.
    size_t past = (size_t)MLOC_MAX_MATCH_LOCALES + 1;
    CHECK(mlocPrepareMatch(&s_data, s.locales, past, nullptr, entries) == mloc_errorLimit &&
              mlocMatchLocales(&s_data, s.locales, entries, past, d.locales, 1, nullptr, &match) ==
                  mloc_errorLimit &&
              mlocMatchLocales(&s_data, s.locales, entries, 2, d.locales, past, nullptr, &match) ==
                  mloc_errorLimit,
          "a count past MLOC_MAX_MATCH_LOCALES");
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
