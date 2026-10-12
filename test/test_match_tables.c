// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The matching table and LSRs from inside (record mloc-0017): the LSRs
// ICU's lookup makes, the paradigm locales the generator wrote, a macro
// region's likely region, and the table's validator, with one field of
// a copy spoiled for each rule.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_distance.h"
#include "locale_likely.h"
#include "locale_lsr.h"
#include "locale_tables.h"
#include "test_harness.h"

#include <stdlib.h>
#include <string.h>

static mlocDataSet s_data;
static Likely s_likely;
static MatchTable s_table;
static DataSpan s_span;

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static bool LsrOf(const char* text, const uint8_t* regionGroups, Lsr* lsrOut)
{
    mlocLocale locale;
    return mlocParseLocale(text, strlen(text), &locale).status == mloc_success &&
           mlocMakeLsr(&s_likely, regionGroups, &locale, 0, lsrOut);
}

// Whether a locale's LSR is the language, script and region given (the
// language as text, the others packed from text, "" for none), with the
// flags and kind.
static bool Makes(const char* text, const char* language, const char* script, const char* region,
                  int flags, int kind)
{
    Lsr lsr;
    char bytes[8] = {0};
    memcpy(bytes, language, strlen(language));
    uint64_t packed = 0;
    memcpy(&packed, bytes, 8);
    return LsrOf(text, s_table.regionGroups, &lsr) && lsr.language == packed &&
           lsr.script == mlocPackScript(script, (uint32_t)strlen(script)) &&
           lsr.region == mlocPackRegion(region, (uint32_t)strlen(region)) && lsr.flags == flags &&
           lsr.kind == kind;
}

static void TestLsrs(void)
{
    CHECK(Makes("en", "en", "Latn", "US", lsrExplicitLanguage, lsrNormal), "a language alone");
    CHECK(Makes("zh-TW", "zh", "Hant", "TW", lsrExplicitLanguage | lsrExplicitRegion, lsrNormal),
          "a language and region");
    CHECK(Makes("und-TW", "zh", "Hant", "TW", lsrExplicitRegion, lsrNormal), "und and a region");
    CHECK(Makes("sr-Latn", "sr", "Latn", "RS", lsrExplicitLanguage | lsrExplicitScript, lsrNormal),
          "a language and script");
    CHECK(Makes("und-Cyrl", "ru", "Cyrl", "RU", lsrExplicitScript, lsrNormal), "und and a script");
    CHECK(
        Makes("und-Hant-TW", "zh", "Hant", "TW", lsrExplicitScript | lsrExplicitRegion, lsrNormal),
        "und's script entry made from its region one");
    CHECK(
        Makes("und-Latn-AD", "ca", "Latn", "AD", lsrExplicitScript | lsrExplicitRegion, lsrNormal),
        "an und script and region entry ICU makes from und-AD");
    CHECK(Makes("und-Latn", "en", "Latn", "US", lsrExplicitScript, lsrNormal),
          "und-Latn, which has region entries only");
    CHECK(
        Makes("und-Latn-EU", "en", "Latn", "EU", lsrExplicitScript | lsrExplicitRegion, lsrNormal),
        "and a region und has no entry for");
    CHECK(
        Makes("qaa-Zinh", "qaa", "Zinh", "US", lsrExplicitLanguage | lsrExplicitScript, lsrNormal),
        "a script und has no entry for");
    CHECK(Makes("en-psaccent-valencia", "en", "Latn", "US", lsrExplicitLanguage, lsrNormal),
          "a pseudo variant with a later one");
    CHECK(Makes("qaa", "qaa", "Latn", "US", lsrExplicitLanguage, lsrNormal),
          "a language without likely subtags takes und's");
    CHECK(
        Makes("qaa-Cyrl", "qaa", "Cyrl", "RU", lsrExplicitLanguage | lsrExplicitScript, lsrNormal),
        "with its script's");
    CHECK(Makes("qaa-Zzzz-ZZ", "qaa", "Latn", "US", lsrExplicitLanguage, lsrNormal),
          "Zzzz and ZZ are none");
    CHECK(Makes("de-Latn-CH", "de", "Latn", "CH", lsrExplicit, lsrNormal), "already maximized");
    CHECK(Makes("und", "und", "", "", lsrExplicit, lsrNormal), "an exact und stays");
    CHECK(Makes("und-fonipa", "en", "Latn", "US", 0, lsrNormal), "und with a variant does not");
    CHECK(Makes("und-x-bork", "und", "", "", lsrExplicit, lsrPrivate), "a private use tag");
    CHECK(Makes("en-XA", "en", "", "XA", lsrExplicit, lsrAccents), "a pseudo region");
    CHECK(Makes("ar-XB", "ar", "", "XB", lsrExplicit, lsrBidi), "the bidi one");
    CHECK(Makes("en-PSCRACK", "en", "", "XC", lsrExplicitLanguage | lsrExplicitScript, lsrCracked),
          "a pseudo variant");
    CHECK(Makes("pt-PT-PSACCENT", "pt", "", "PT", lsrExplicit, lsrAccents), "with a region");
    CHECK(Makes("en-fonipa-psaccent", "en", "Latn", "US", lsrExplicitLanguage, lsrNormal),
          "not with another variant");
    Lsr lsr;
    CHECK(LsrOf("und-419", s_table.regionGroups, &lsr) && lsr.flags == lsrExplicitRegion,
          "a grouping region found is kept");
    uint8_t groups[matchRegionSlots];
    memcpy(groups, s_table.regionGroups, sizeof(groups));
    groups[mlocPackRegion("419", 3)] |= matchMacroRegion;
    CHECK(LsrOf("und-419", groups, &lsr) && lsr.flags == 0 &&
              lsr.region == mlocPackRegion("419", 3),
          "a macro region found takes the likely region");
}

// A likely table of three entries: und, und-XX giving qaa-Zinh-YY, and
// en-US; so a script und has no entry of its own, and a region one whose
// answer has another region.
static void TestCraftedLikely(void)
{
    uint32_t und = mlocPackLanguage("und", 3);
    uint64_t keys[3] = {mlocPackTag(und, 0, 0), mlocPackTag(und, 0, mlocPackRegion("XX", 2))};
    uint64_t values[3] = {
        mlocPackTag(mlocPackLanguage("en", 2), mlocPackScript("Latn", 4), mlocPackRegion("US", 2)),
        mlocPackTag(mlocPackLanguage("qaa", 3), mlocPackScript("Zinh", 4),
                    mlocPackRegion("YY", 2))};
    uint8_t keyBytes[16];
    uint8_t valueBytes[16];
    for (int i = 0; i < 2; i++)
    {
        Put(keyBytes + 8 * i, keys[i], 8);
        Put(valueBytes + 8 * i, values[i], 8);
    }
    Likely crafted = {.generalKeys = keyBytes, .generalValues = valueBytes, .generalCount = 2};
    mlocLocale locale;
    Lsr lsr;
    CHECK(mlocParseLocale("und-Zinh-YY", 11, &locale).status == mloc_success &&
              mlocMakeLsr(&crafted, s_table.regionGroups, &locale, 0, &lsr) &&
              lsr.packedLanguage == mlocPackLanguage("qaa", 3) &&
              lsr.flags == (lsrExplicitScript | lsrExplicitRegion),
          "an und script entry from a region entry with another region");
    CHECK(mlocParseLocale("und-Zinh", 8, &locale).status == mloc_success &&
              mlocMakeLsr(&crafted, s_table.regionGroups, &locale, 0, &lsr) &&
              lsr.packedLanguage == mlocPackLanguage("en", 2) && lsr.flags == lsrExplicitScript,
          "its script found, with no entry of its own");
    Likely empty = {0};
    CHECK(!mlocMakeLsr(&empty, s_table.regionGroups, &locale, 0, &lsr), "a table without und");
}

static void TestParadigms(void)
{
    static const char* const s_paradigms[] = {"en", "en-GB", "es", "es-419", "pt-BR", "pt-PT"};
    CHECK(s_table.paradigmCount == 6, "CLDR's six paradigm locales");
    for (uint32_t i = 0; i < 6 && i < s_table.paradigmCount; i++)
    {
        Lsr lsr;
        CHECK(LsrOf(s_paradigms[i], s_table.regionGroups, &lsr) &&
                  mlocLoad64(s_table.paradigms + (size_t)i * 8) ==
                      mlocPackTag(lsr.packedLanguage, lsr.script, lsr.region),
              "a paradigm locale maximized as matching maximizes it");
    }
    CHECK(s_table.languageDistance == 80 && s_table.scriptDistance == 50 &&
              s_table.regionDistance == 4 && s_table.minRegionDistance == 3,
          "CLDR's default distances");
}

// Whether the table is valid with value written at offset of a copy.
static bool ValidWith(size_t offset, uint64_t value, int bytes)
{
    uint8_t* copy = malloc(s_span.size);
    if (copy == nullptr)
    {
        return true;
    }
    memcpy(copy, s_span.bytes, s_span.size);
    Put(copy + offset, value, bytes);
    bool valid = mlocValidMatch(nullptr, (DataSpan){copy, s_span.size});
    free(copy);
    return valid;
}

static size_t At(const uint8_t* p)
{
    return (size_t)(p - s_span.bytes);
}

// The first language pair whose script table is skipped, else the
// first with scripts.
static const uint8_t* LanguageWith(bool skip)
{
    for (uint32_t i = 0; i < s_table.languageCount; i++)
    {
        const uint8_t* e = s_table.languages + (size_t)i * matchLanguageSize;
        if ((e[9] == matchSkipScript) == skip && mlocLoad16(e + 6) > 1)
        {
            return e;
        }
    }
    return nullptr;
}

static void TestValidator(void)
{
    CHECK(mlocValidMatch(nullptr, s_span), "the compiled-in table is valid");
    CHECK(!ValidWith(0, 101, 1), "a distance above 100");
    CHECK(!ValidWith(4, matchMaxParadigms + 1, 2), "too many paradigm locales");
    CHECK(!ValidWith(6, s_table.languageCount + 1, 2), "a count the size does not hold");
    CHECK(!ValidWith(12, 0, 2), "no groups");
    CHECK(!ValidWith(At(s_table.paradigms), 0x7FFFull << 31, 8), "a paradigm no tag is");
    const uint8_t* first = s_table.languages;
    CHECK(!ValidWith(At(first), mlocLoad32(first + matchLanguageSize), 4),
          "language pairs out of order");
    CHECK(!ValidWith(At(first), 1u << 30, 4), "a key past its bits");
    CHECK(!ValidWith(At(first) + 8, 101, 1), "a pair's distance");
    CHECK(!ValidWith(At(first) + 9, 2, 1), "another flag");
    CHECK(!ValidWith(At(first) + 10, 1, 2), "reserved bytes");
    const uint8_t* skip = LanguageWith(true);
    const uint8_t* scripted = LanguageWith(false);
    CHECK(skip != nullptr && scripted != nullptr, "both kinds of language pair");
    if (skip != nullptr && scripted != nullptr)
    {
        CHECK(!ValidWith(At(skip) + 6, 0, 2), "a skipped script table without regions");
        CHECK(!ValidWith(At(skip) + 4, s_table.regionCount, 2), "regions past the end");
        CHECK(!ValidWith(At(skip) + 4, mlocLoad16(skip + 4) + 1, 2), "a region run without *");
        CHECK(!ValidWith(At(scripted) + 4, mlocLoad16(scripted + 4) + 1, 2),
              "a script run without *");
        CHECK(!ValidWith(At(scripted) + 4, s_table.scriptCount, 2), "children past the scripts");
        const uint8_t* script =
            s_table.scripts + (size_t)mlocLoad16(scripted + 4) * matchScriptSize;
        const uint8_t* second = script + matchScriptSize;
        CHECK(!ValidWith(At(second), mlocLoad64(second + matchScriptSize), 8),
              "script pairs out of order");
        CHECK(!ValidWith(At(second), 1ull << 40, 8), "a script key past its bits");
        CHECK(!ValidWith(At(second) + 12, 101, 1), "a script pair's distance");
        CHECK(!ValidWith(At(second) + 13, 1, 1), "a script pair's reserved bytes");
    }
    const uint8_t* region = s_table.regions + matchRegionSize;
    CHECK(!ValidWith(At(region), 0x7F, 1), "a partition no character is");
    CHECK(ValidWith(At(region), 0x21, 1), "the lowest character is one");
    CHECK(ValidWith(2, 100, 1), "a distance of 100");
    CHECK(ValidWith(At(s_table.regionGroups) + 1, s_table.groupCount - 1, 1),
          "the last group, read as no group entry");
    CHECK(!ValidWith(At(region) + 2, 101, 1), "a region pair's distance");
    CHECK(!ValidWith(At(region) + 3, 1, 1), "a region pair's reserved byte");
    CHECK(!ValidWith(At(s_table.groups) + 2, s_table.textSize + 1, 2), "a group past the text");
    CHECK(!ValidWith(At(s_table.regionGroups) + 33, s_table.groupCount, 1),
          "a group past the groups");
    CHECK(!ValidWith(At(s_table.text), ' ', 1), "a partition of a space");
    CHECK(!mlocValidMatch(nullptr, (DataSpan){s_span.bytes, s_span.size - 1}), "a short table");
    CHECK(!mlocValidMatch(nullptr, (DataSpan){s_span.bytes, 8}), "a header cut off");
}

// Whether matching refuses entries with one field of the second spoiled.
static bool Refused(void (*spoil)(Lsr*))
{
    mlocLocale supported[2];
    mlocLocale desired;
    mlocMatchEntry entries[2];
    if (mlocParseLocale("fr", 2, &supported[0]).status != mloc_success ||
        mlocParseLocale("en", 2, &supported[1]).status != mloc_success ||
        mlocParseLocale("de", 2, &desired).status != mloc_success ||
        mlocPrepareMatch(&s_data, supported, 2, nullptr, entries) != mloc_success)
    {
        return false;
    }
    Lsr lsr;
    memcpy(&lsr, &entries[1], sizeof(lsr));
    spoil(&lsr);
    memcpy(&entries[1], &lsr, sizeof(lsr));
    mlocMatch match;
    return mlocMatchLocales(&s_data, supported, entries, 2, &desired, 1, nullptr, &match) ==
           mloc_errorInvalid;
}

static void IndexPast(Lsr* lsr)
{
    lsr->index = 2;
}

static void KindPast(Lsr* lsr)
{
    lsr->kind = lsrPrivate + 1;
}

static void FlagsPast(Lsr* lsr)
{
    lsr->flags = lsrExplicit + 1;
}

static void RegionPast(Lsr* lsr)
{
    lsr->region = matchRegionSlots;
}

static void LastRegion(Lsr* lsr)
{
    lsr->region = matchRegionSlots - 1;
}

static void TestEntries(void)
{
    CHECK(Refused(IndexPast), "an entry naming no locale of the list");
    CHECK(Refused(KindPast), "an entry of no kind");
    CHECK(Refused(FlagsPast), "an entry of other flags");
    CHECK(Refused(RegionPast), "an entry's region past the groups");
    CHECK(!Refused(LastRegion), "and the last region");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    CHECK(mlocOpenLikely(&s_data, &s_likely) && mlocOpenMatchTable(&s_data, &s_table) &&
              mlocFindDataSetUnit(&s_data, kindLocaleMatch, nullptr, 0, &s_span),
          "the tables open");
    TestLsrs();
    TestCraftedLikely();
    TestParadigms();
    TestValidator();
    TestEntries();
    return s_failures == 0 ? 0 : 1;
}
