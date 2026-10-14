// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The names part's units (names_data.h, names_tables.h) on units built
// here: valid ones pass and their codes are found; copies with one field
// spoiled are refused. Then names found along a chain from the names data
// file: each namespace, each kind of field, a field the chain lacks
// taking the name, a region's own names over its parent's, and the codes
// and fields that find nothing. Last, whole-locale names by patterns and
// names built here.

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_context.h"
#include "names_data.h"
#include "names_tables.h"
#include "spellout_data.h"
#include "test_harness.h"
#include "test_pack.h"

#include "maul-locale/data.h"
#include "maul-locale/names.h"

#include <stdio.h>
#include <string.h>

static uint8_t s_unit[131072];
static mlocData s_sources[2];
static mlocDataSet s_data;

static void Put(uint8_t* p, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Builds a codes unit; its size.
static uint32_t BuildIds(const char* const* codes, uint32_t count)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint32_t text = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        text += (uint32_t)strlen(codes[i]);
    }
    uint8_t* offsets = s_unit + nameIdsHeaderSize;
    uint8_t* textAt = offsets + 4 * (count + 1);
    Put(s_unit, count, 2);
    Put(s_unit + 4, text, 4);
    uint32_t at = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        Put(offsets + 4 * i, at, 4);
        memcpy(textAt + at, codes[i], strlen(codes[i]));
        at += (uint32_t)strlen(codes[i]);
    }
    Put(offsets + 4 * count, at, 4);
    return nameIdsHeaderSize + 4 * (count + 1) + text;
}

static bool ValidIds(uint32_t size)
{
    return mlocValidNameIds(nullptr, (DataSpan){s_unit, size});
}

// Whether a codes unit of one code is valid.
static bool OneValid(const char* code)
{
    return ValidIds(BuildIds(&code, 1));
}

static void TestIds(void)
{
    static const char* const s_codes[] = {"l:en", "l:en-GB", "r:IO%chagos", "t:ca-gregory"};
    uint32_t size = BuildIds(s_codes, 4);
    CHECK(ValidIds(size), "a valid codes unit");
    DataSpan ids = {s_unit, size};
    uint16_t index = 9;
    CHECK(mlocFindNameId(ids, "l:en-GB", 7, &index) && index == 1 &&
              mlocFindNameId(ids, "t:ca-gregory", 12, &index) && index == 3 &&
              !mlocFindNameId(ids, "l:e", 3, &index) && !mlocFindNameId(ids, "l:en-G", 6, &index) &&
              !mlocFindNameId(ids, "z:z", 3, &index),
          "codes found, a prefix or a code past the last not");
    static const char* const s_unordered[] = {"l:en-GB", "l:en"};
    static const char* const s_twice[] = {"l:en", "l:en"};
    static const char* const s_emptyFirst[] = {"", "l:en"};
    static const char* const s_lastLow[] = {"a:a", "b:b", "a:c"};
    char longest[nameMaxId + 2];
    memset(longest, 'a', sizeof(longest));
    longest[1] = ':';
    longest[nameMaxId] = '\0';
    bool fits = OneValid(longest);
    longest[nameMaxId] = 'a';
    longest[nameMaxId + 1] = '\0';
    CHECK(fits && !OneValid(longest) && !OneValid("l:e n") && !OneValid("l:\xC3\xA9") &&
              !ValidIds(BuildIds(s_unordered, 2)) && !ValidIds(BuildIds(s_twice, 2)) &&
              !ValidIds(BuildIds(s_emptyFirst, 2)) && !ValidIds(BuildIds(s_lastLow, 3)),
          "a code too long, empty, of other bytes, out of order (the last below the one before it) "
          "or twice");
    size = BuildIds(s_codes, 4);
    CHECK(!ValidIds(size - 1) && !ValidIds(3), "a size not the counts'");
    Put(s_unit + 2, 1, 2);
    bool padded = ValidIds(size);
    size = BuildIds(s_codes, 4);
    Put(s_unit + nameIdsHeaderSize + 4, 12, 4);
    bool backwards = ValidIds(size);
    size = BuildIds(s_codes, 4);
    Put(s_unit + nameIdsHeaderSize, 1, 4);
    CHECK(!padded && !backwards && !ValidIds(size),
          "a pad not 0, offsets out of order or not from 0");
    // More codes than keys of 16 bits hold.
    memset(s_unit, 0, sizeof(s_unit));
    Put(s_unit, nameMaxCodes + 1, 2);
    CHECK(!ValidIds(nameIdsHeaderSize + 4 * (nameMaxCodes + 2)), "too many codes");
}

// Builds a names unit of entries (keys and texts); its size.
static uint32_t BuildNames(const uint16_t* keys, const char* const* texts, uint32_t count)
{
    memset(s_unit, 0, sizeof(s_unit));
    uint8_t* offsets = s_unit + nameUnitHeaderSize + 2 * count;
    uint8_t* textAt = offsets + 2 * (count + 1);
    uint32_t at = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        Put(s_unit + nameUnitHeaderSize + 2 * i, keys[i], 2);
        Put(offsets + 2 * i, at, 2);
        memcpy(textAt + at, texts[i], strlen(texts[i]));
        at += (uint32_t)strlen(texts[i]);
    }
    Put(offsets + 2 * count, at, 2);
    Put(s_unit, count, 2);
    Put(s_unit + 2, at, 2);
    return nameUnitHeaderSize + 2 * count + 2 * (count + 1) + at;
}

static bool ValidNames(uint32_t size)
{
    return mlocValidNames(nullptr, (DataSpan){s_unit, size});
}

static void TestNames(void)
{
    static const uint16_t s_keys[] = {0, 1, 8 * 5 + 7};
    static const char* const s_texts[] = {"English", "Eng.", "\xE4\xB8\xAD"};
    uint32_t size = BuildNames(s_keys, s_texts, 3);
    CHECK(ValidNames(size) && ValidNames(BuildNames(s_keys, s_texts, 0)), "valid names units");
    static const uint16_t s_same[] = {1, 1};
    static const uint16_t s_down[] = {2, 1};
    static const char* const s_two[] = {"a", "b"};
    static const char* const s_empty[] = {"a", ""};
    static const char* const s_control[] = {"a", "b\n"};
    static const char* const s_broken[] = {"a", "\xC3"};
    CHECK(!ValidNames(BuildNames(s_same, s_two, 2)) && !ValidNames(BuildNames(s_down, s_two, 2)),
          "keys twice or out of order");
    CHECK(!ValidNames(BuildNames(s_keys, s_empty, 2)) &&
              !ValidNames(BuildNames(s_keys, s_control, 2)) &&
              !ValidNames(BuildNames(s_keys, s_broken, 2)),
          "an empty text, a control byte, malformed UTF-8");
    size = BuildNames(s_keys, s_texts, 3);
    CHECK(!ValidNames(size - 1) && !ValidNames(3), "a size not the counts'");
    Put(s_unit + nameUnitHeaderSize + 6, 1, 2);
    bool first = ValidNames(size);
    size = BuildNames(s_keys, s_texts, 3);
    Put(s_unit + nameUnitHeaderSize + 6 + 6, 2, 2);
    CHECK(!first && !ValidNames(size), "text offsets not from 0, not to the text's size");
    CHECK(mlocNameKind("l:en", 4) == kindNamesLanguages &&
              mlocNameKind("s:Latn", 6) == kindNamesScripts &&
              mlocNameKind("r:US", 4) == kindNamesRegions &&
              mlocNameKind("v:valencia", 10) == kindNamesVariants &&
              mlocNameKind("k:ca", 4) == kindNamesKeys &&
              mlocNameKind("t:ca-roc", 8) == kindNamesKeys &&
              mlocNameKind("p:localePattern", 15) == kindNamePatterns &&
              mlocNameKind("c:script", 8) == kindNamePatterns &&
              mlocNameKind("m:metric", 8) == kindNamePatterns &&
              mlocNameKind("b:yes", 5) == kindNamePatterns && mlocNameKind("z:z", 3) == 0 &&
              mlocNameKind("l", 1) == 0 && mlocNameKind("lx", 2) == 0,
          "each namespace's kind, none for another");
}

// Whether a locale's code field is found as expected.
static bool Named(const char* id, const char* code, NameField field, const char* expected)
{
    mlocLocale locale;
    NameText name;
    bool same = mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
                mlocFindName(&s_data, &locale, code, strlen(code), field, &name) &&
                name.length == strlen(expected) && memcmp(name.text, expected, name.length) == 0;
    if (!same)
    {
        printf("%s %s %d\n", id, code, (int)field);
    }
    return same;
}

static void TestFind(void)
{
    CHECK(Named("en", "l:en-GB", nameFieldName, "British English") &&
              Named("en", "l:en-GB", nameFieldShort, "UK English") &&
              Named("en", "s:Hans", nameFieldName, "Simplified") &&
              Named("en", "s:Hans", nameFieldStandAlone, "Simplified Han") &&
              Named("en", "r:US", nameFieldShort, "US") &&
              Named("en", "v:valencia", nameFieldName, "Valencian") &&
              Named("en", "k:ca", nameFieldName, "Calendar") &&
              Named("en", "t:ca-gregory", nameFieldName, "Gregorian Calendar") &&
              Named("en", "t:ca-gregory", nameFieldCore, "Gregorian") &&
              Named("en", "l:ku", nameFieldExtension, "Kurmanji"),
          "each namespace's names and alternates");
    CHECK(Named("en", "p:localePattern", nameFieldName, "{0} ({1})") &&
              Named("en", "c:region", nameFieldName, "Region: {0}") &&
              Named("en", "m:uksystem", nameFieldName, "UK") &&
              Named("en", "b:yes", nameFieldName, "On") &&
              Named("en", "r:IO%chagos", nameFieldName, "Chagos Archipelago"),
          "patterns, measurement systems, yes and no, an alt of a code of its own");
    CHECK(Named("en", "l:fr", nameFieldShort, "French") &&
              Named("en", "l:ku", nameFieldCore, "Kurdish") &&
              Named("fr", "s:Latn", nameFieldMenu, "latin"),
          "a field the chain lacks takes the name");
    CHECK(Named("en-AU", "l:en-US", nameFieldName, "United States English") &&
              Named("en-AU", "l:en-GB", nameFieldName, "British English") &&
              Named("de-CH", "r:BW", nameFieldName, "Botswana") &&
              Named("de-AT", "r:BW", nameFieldName, "Botsuana") &&
              Named("de-CH", "r:CV", nameFieldName, "Kapverden"),
          "a region's own names, the rest from its chain");
    mlocLocale en;
    NameText name;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              !mlocFindName(&s_data, &en, "l:qqq", 5, nameFieldName, &name) &&
              name.text == nullptr &&
              !mlocFindName(&s_data, &en, "z:en", 4, nameFieldName, &name) &&
              !mlocFindName(&s_data, &en, "l:en", 4, nameFieldCount, &name),
          "a code without a name, of no namespace, a field out of range");
}

// The names' capitalization: the context transforms of each usage, and
// the titlecase of a name's first letter.
static void TestCapitalization(void)
{
    mlocLocale fr;
    mlocLocale en;
    DataSpan titlecase;
    const uint8_t* text = nullptr;
    uint32_t length = 0;
    CHECK(mlocParseLocale("fr", 2, &fr).status == mloc_success &&
              mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocGetLocaleContext(&s_data, &fr, contextLanguages) == contextListOrMenu &&
              mlocGetLocaleContext(&s_data, &fr, contextTerritory) == 0 &&
              mlocGetLocaleContext(&s_data, &en, contextLanguages) == 0,
          "French titlecases language names in lists and menus");
    CHECK(mlocFindDataSetUnit(&s_data, kindNameTitlecase, nullptr, 0, &titlecase) &&
              mlocSpelloutTitlecase(titlecase, 0xE9, &text, &length) && length == 2 &&
              memcmp(text, "\xC3\x89", 2) == 0 &&
              !mlocSpelloutTitlecase(titlecase, 'A', &text, &length),
          "the names' titlecase table");
}

// A set of the compiled-in data, a root pack of a codes unit and one
// names unit, and with the file the names data file.
static bool OpenPackCodes(const char* const* codes, uint32_t codeCount, uint32_t kind,
                          const uint16_t* keys, const char* const* texts, uint32_t count,
                          bool withFile, mlocDataSet* set)
{
    static uint8_t s_ids[256];
    static uint8_t s_names[256];
    uint32_t idsSize = BuildIds(codes, codeCount);
    memcpy(s_ids, s_unit, idsSize);
    uint32_t namesSize = BuildNames(keys, texts, count);
    memcpy(s_names, s_unit, namesSize);
    static mlocData s_three[3];
    s_three[0] = s_sources[0];
    s_three[2] = s_sources[1];
    PackUnit units[] = {{kindNameIds, {s_ids, idsSize}, false}, {kind, {s_names, namesSize}, true}};
    *set = (mlocDataSet){s_three, withFile ? 3 : 2};
    return OpenRootPack(units, 2, &s_three[1]);
}

static bool OpenPack(const char* code, uint32_t kind, const uint16_t* keys,
                     const char* const* texts, uint32_t count, bool withFile, mlocDataSet* set)
{
    return OpenPackCodes(&code, 1, kind, keys, texts, count, withFile, set);
}

// A source whose codes lack the one asked for is passed over; a code's
// entries ending its unit end the search there.
static void TestSources(void)
{
    mlocLocale en;
    mlocDataSet set;
    NameText name;
    static const uint16_t s_one[] = {0};
    static const char* const s_other[] = {"(x)"};
    bool passed = mlocParseLocale("en", 2, &en).status == mloc_success &&
                  OpenPack("p:zzz", kindNamePatterns, s_one, s_other, 1, true, &set) &&
                  mlocFindName(&set, &en, "p:localePattern", 15, nameFieldName, &name) &&
                  name.length == 9 && memcmp(name.text, "{0} ({1})", 9) == 0;
    CHECK(passed, "a source without the code passed over");
    static const uint16_t s_short[] = {nameFieldShort};
    static const char* const s_text[] = {"aa"};
    mlocLocale und;
    CHECK(mlocParseLocale("und", 3, &und).status == mloc_success &&
              OpenPack("l:zz", kindNamesLanguages, s_short, s_text, 1, false, &set) &&
              !mlocFindName(&set, &und, "l:zz", 4, nameFieldName, &name) &&
              mlocFindName(&set, &und, "l:zz", 4, nameFieldShort, &name) && name.length == 2,
          "a code's entries at its unit's end, its name absent");
}

// Whether a locale's name in English, by a set, is as expected.
static bool LocaleNamed(const mlocDataSet* set, mlocLanguageDisplay mode, const char* id,
                        const char* expected)
{
    mlocLocale en;
    mlocLocale named;
    mlocLocaleNameOptions options = {.languageDisplay = mode};
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocResult r = mlocParseLocale("en", 2, &en).status == mloc_success &&
                           mlocParseLocale(id, strlen(id), &named).status == mloc_success
                       ? mlocGetLocaleName(set, &en, &named, &options, &output)
                       : mloc_errorInvalid;
    bool same =
        r >= 0 && output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
    if (!same)
    {
        printf("%s: %d \"%.*s\"\n", id, (int)r, r >= 0 ? (int)output.length : 0, buffer);
    }
    return same;
}

// Whole-locale names by patterns and names no CLDR locale has: a pattern
// with a prefix and a separator with both, a pattern without its
// placeholders (root's taken instead), and a dialect name for a whole id
// with variants, passed over when it holds a hyphen.
static void TestCraftedLocaleNames(void)
{
    mlocDataSet set;
    static const char* const s_patternCodes[] = {"p:localePattern", "p:localeSeparator"};
    static const uint16_t s_patternKeys[] = {0, nameFieldCount};
    static const char* const s_patterns[] = {"<{0}|{1}>", "!{0};{1}/"};
    CHECK(OpenPackCodes(s_patternCodes, 2, kindNamePatterns, s_patternKeys, s_patterns, 2, true,
                        &set) &&
              LocaleNamed(&set, mloc_languageDisplayStandard, "en", "English") &&
              LocaleNamed(&set, mloc_languageDisplayStandard, "en-Latn-GB",
                          "<English|!Latin;United Kingdom/>") &&
              LocaleNamed(&set, mloc_languageDisplayStandard, "en-u-kr-latn-grek",
                          "<English|Script/Block Reordering: !Latin;Greek/>"),
          "a pattern's prefix only with parts, a separator's prefix and suffix");
    static const uint16_t s_name[] = {0};
    static const char* const s_broken[] = {"{0} only"};
    CHECK(OpenPack("p:localePattern", kindNamePatterns, s_name, s_broken, 1, true, &set) &&
              LocaleNamed(&set, mloc_languageDisplayStandard, "en-GB", "English (United Kingdom)"),
          "a pattern without its placeholders: root's");
    static const char* const s_wholeCodes[] = {"l:xx-YY-varxx", "l:xy-YY-varyy"};
    static const uint16_t s_wholeKeys[] = {0, nameFieldCount};
    static const char* const s_wholes[] = {"Foo", "Fo-o"};
    CHECK(
        OpenPackCodes(s_wholeCodes, 2, kindNamesLanguages, s_wholeKeys, s_wholes, 2, true, &set) &&
            LocaleNamed(&set, mloc_languageDisplayDialect, "xx-YY-varxx", "Foo") &&
            LocaleNamed(&set, mloc_languageDisplayDialect, "xy-YY-varyy", "xy (YY, varyy)"),
        "a whole id's dialect name, passed over when it holds a hyphen");
}

// The compiled-in data, then the names data file at the path the build
// gives.
static bool OpenData(void)
{
    alignas(8) static uint8_t s_names[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    FILE* file = fopen(MLOC_LOCALE_NAMES_FILE, "rb");
    size_t read = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 2};
    return read > 0 && read < sizeof(s_names) &&
           mlocOpenData(&s_sources[1], s_names, read) == mloc_success;
}

int main(void)
{
    TestIds();
    TestNames();
    if (!OpenData())
    {
        fputs("test_names_data: cannot open " MLOC_LOCALE_NAMES_FILE "\n", stderr);
        return 2;
    }
    TestFind();
    TestSources();
    TestCraftedLocaleNames();
    TestCapitalization();
    return s_failures == 0 ? 0 : 1;
}
