// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number parsing's affix matchers and bounds on crafted data (record
// mloc-0032): CLDR's decimal and percent patterns have no literal the
// ignorables do not cover, so a pack gives the formatter patterns that
// have ("#,##0 pcs", explicit negative subpatterns, parentheses, a
// trailing minus), each case's result as ICU4C 79's DecimalFormat with
// the same pattern and English symbols gives it; then the affix bounds,
// data without the parse sets, and the code point reader.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "number_tables.h"
#include "parse_match.h"
#include "test_harness.h"
#include "test_pack.h"

#include "maul-locale/decimal.h"
#include "maul-locale/number.h"

#include <stdlib.h>
#include <string.h>

static DataView s_view;
static DataSpan s_plurals;
static DataSpan s_ranges;
static DataSpan s_systems;
static DataSpan s_symbols;
static DataSpan s_root;
static DataSpan s_parseSets;

// A pattern's affixes, placeholders as the data has them (\x01 the minus
// sign, \x03 the percent sign), with grouping by three.
typedef struct Pattern
{
    const char* affixes[4];
} Pattern;

// ICU's patterns: "#,##0 pcs", "pre#,##0suf;neg#,##0", "#,##0;#,##0-",
// "#,##0;(#,##0)", "aa#,##0;a#,##0", "#,##0;#,##0", "x#,##0%", "x#,##0".
static const Pattern s_patterns[] = {
    {{"", " pcs", "\x01", " pcs"}},
    {{"pre", "suf", "neg", ""}},
    {{"", "", "", "\x01"}},
    {{"", "", "(", ")"}},
    {{"aa", "", "a", ""}},
    {{"", "", "", ""}},
    {{"x", "\x03", "\x01x", "\x03"}},
    {{"x", "", "\x01x", ""}},
};

typedef struct Case
{
    uint8_t pattern;
    bool strict;
    bool plus;
    const char* text;
    // ICU's value, null for none.
    const char* expected;
    size_t end;
} Case;

static const Case s_cases[] = {
    {0, false, false, "12 pcs", "12", 6},      {0, true, false, "12 pcs", "12", 6},
    {0, false, true, "12 pcs", "12", 6},       {0, true, true, "12 pcs", nullptr, 2},
    {0, false, false, "12pcs", "12", 5},       {0, true, false, "12pcs", nullptr, 2},
    {0, false, true, "12pcs", "12", 5},        {0, true, true, "12pcs", nullptr, 2},
    {0, false, false, "12 p c s", "12", 8},    {0, true, false, "12 p c s", nullptr, 2},
    {0, false, true, "12 p c s", "12", 8},     {0, true, true, "12 p c s", nullptr, 2},
    {0, false, false, "-12 pcs", "-12", 7},    {0, true, false, "-12 pcs", "-12", 7},
    {0, false, true, "-12 pcs", "-12", 7},     {0, true, true, "-12 pcs", "-12", 7},
    {0, false, false, "12", "12", 2},          {0, true, false, "12", nullptr, 2},
    {0, false, true, "12", "12", 2},           {0, true, true, "12", nullptr, 2},
    {0, false, false, "12 pc", "12", 2},       {0, true, false, "12 pc", nullptr, 2},
    {0, false, true, "12 pc", "12", 2},        {0, true, true, "12 pc", nullptr, 2},
    {0, false, false, "-12", "-12", 3},        {0, true, false, "-12", nullptr, 3},
    {0, false, true, "-12", "-12", 3},         {0, true, true, "-12", nullptr, 3},
    {0, false, false, "pcs12", nullptr, 0},    {0, true, false, "pcs12", nullptr, 0},
    {0, false, true, "pcs12", nullptr, 0},     {0, true, true, "pcs12", nullptr, 0},
    {0, false, false, "12 pcs pcs", "12", 6},  {0, true, false, "12 pcs pcs", "12", 6},
    {0, false, true, "12 pcs pcs", "12", 6},   {0, true, true, "12 pcs pcs", nullptr, 2},
    {0, false, false, "- 12 pcs", "-12", 8},   {0, true, false, "- 12 pcs", nullptr, 1},
    {0, false, true, "- 12 pcs", "-12", 8},    {0, true, true, "- 12 pcs", nullptr, 1},
    {1, false, false, "pre12suf", "12", 8},    {1, true, false, "pre12suf", "12", 8},
    {1, false, false, "neg12", "-12", 5},      {1, true, false, "neg12", "-12", 5},
    {1, false, false, "pre12", "12", 5},       {1, true, false, "pre12", nullptr, 5},
    {1, false, false, "12suf", "12", 5},       {1, true, false, "12suf", nullptr, 2},
    {1, false, false, "neg12suf", "-12", 5},   {1, true, false, "neg12suf", "-12", 5},
    {1, false, false, "pre-12suf", "-12", 9},  {1, true, false, "pre-12suf", nullptr, 3},
    {1, false, false, "12", "12", 2},          {1, true, false, "12", nullptr, 2},
    {1, false, false, "-12", "-12", 3},        {1, true, false, "-12", nullptr, 0},
    {1, false, false, "pre 12 suf", "12", 10}, {1, true, false, "pre 12 suf", nullptr, 3},
    {1, false, false, "PRE12SUF", "12", 8},    {1, true, false, "PRE12SUF", "12", 8},
    {2, false, false, "12-", "-12", 3},        {2, true, false, "12-", "-12", 3},
    {2, false, false, "12", "12", 2},          {2, true, false, "12", "12", 2},
    {2, false, false, "-12", "-12", 3},        {2, true, false, "-12", nullptr, 0},
    {2, false, false, "12 -", "-12", 4},       {2, true, false, "12 -", "12", 2},
    {2, false, false, "12--", "-12", 3},       {2, true, false, "12--", "-12", 3},
    {3, false, false, "(12)", "-12", 4},       {3, true, false, "(12)", "-12", 4},
    {3, false, false, "(12", "-12", 3},        {3, true, false, "(12", nullptr, 3},
    {3, false, false, "12)", "-12", 3},        {3, true, false, "12)", "12", 2},
    {3, false, false, "12", "12", 2},          {3, true, false, "12", "12", 2},
    {3, false, false, "-12", "-12", 3},        {3, true, false, "-12", nullptr, 0},
    {3, false, false, "( 12 )", "-12", 6},     {3, true, false, "( 12 )", nullptr, 1},
    {3, false, false, "(-12)", "-12", 5},      {3, true, false, "(-12)", nullptr, 1},
    {4, false, false, "aa12", "12", 4},        {4, true, false, "aa12", "12", 4},
    {4, false, false, "a12", "-12", 3},        {4, true, false, "a12", "-12", 3},
    {4, false, false, "12", "12", 2},          {4, true, false, "12", nullptr, 2},
    {4, false, false, "aaa12", nullptr, 2},    {4, true, false, "aaa12", nullptr, 2},
    {4, false, false, "a a12", "12", 5},       {4, true, false, "a a12", nullptr, 1},
    {4, false, false, "-12", "-12", 3},        {4, true, false, "-12", nullptr, 0},
    {5, false, false, "12", "12", 2},          {5, true, false, "12", "12", 2},
    {5, false, false, "-12", "-12", 3},        {5, true, false, "-12", nullptr, 0},
    {6, false, false, "x50%", "0.5", 4},       {6, true, false, "x50%", "0.5", 4},
    {6, false, false, "x50", "0.5", 3},        {6, true, false, "x50", nullptr, 3},
    {6, false, false, "50%", "0.5", 3},        {6, true, false, "50%", nullptr, 2},
    {6, false, false, "-x50%", "-0.5", 5},     {6, true, false, "-x50%", "-0.5", 5},
    {6, false, false, "x-50%", "-0.5", 5},     {6, true, false, "x-50%", nullptr, 1},
    {6, false, false, "x 50 %", "0.5", 6},     {6, true, false, "x 50 %", nullptr, 1},
    {6, false, false, "50", "0.5", 2},         {6, true, false, "50", nullptr, 2},
    {7, false, false, "+x12", "12", 4},        {7, true, false, "+x12", nullptr, 0},
    {7, false, true, "+x12", "12", 4},         {7, true, true, "+x12", "12", 4},
    {7, false, false, "x+12", "12", 4},        {7, true, false, "x+12", nullptr, 1},
    {7, false, true, "x+12", nullptr, 0},      {7, true, true, "x+12", nullptr, 0},
    {7, false, false, "x12", "12", 3},         {7, true, false, "x12", "12", 3},
    {7, false, true, "x12", nullptr, 0},       {7, true, true, "x12", nullptr, 0},
    {7, false, false, "-x12", "-12", 4},       {7, true, false, "-x12", "-12", 4},
    {7, false, true, "-x12", "-12", 4},        {7, true, true, "-x12", "-12", 4},
    {7, false, false, "+12", "12", 3},         {7, true, false, "+12", nullptr, 0},
    {7, false, true, "+12", "12", 3},          {7, true, true, "+12", nullptr, 0},
    {7, false, false, "12", "12", 2},          {7, true, false, "12", nullptr, 2},
    {7, false, true, "12", "12", 2},           {7, true, true, "12", nullptr, 2},
};

static uint8_t s_patternUnit[4096];
static uint8_t s_rootUnit[1024];

// A patterns unit of the table shape holding one pattern of affixes, the
// grouping by three.
static DataSpan PatternUnit(const char* const affixes[4])
{
    uint32_t text = 0;
    for (int a = 0; a < 4; a++)
    {
        text += (uint32_t)strlen(affixes[a]);
    }
    uint32_t offsets = numberTableHeaderSize;
    uint32_t records = offsets + 2 * 5;
    uint32_t textAt = records + numberPatternSize;
    if (textAt + text > sizeof(s_patternUnit))
    {
        return (DataSpan){nullptr, 0};
    }
    memset(s_patternUnit, 0, sizeof(s_patternUnit));
    PackPut(s_patternUnit, 4, 2);
    PackPut(s_patternUnit + 2, 1, 2);
    PackPut(s_patternUnit + 4, text, 4);
    uint32_t at = 0;
    for (int a = 0; a < 4; a++)
    {
        size_t n = strlen(affixes[a]);
        PackPut(s_patternUnit + offsets + 2 * a, at, 2);
        memcpy(s_patternUnit + textAt + at, affixes[a], n);
        PackPut(s_patternUnit + records + 2 * a, (uint64_t)a, 2);
        at += (uint32_t)n;
    }
    PackPut(s_patternUnit + offsets + 8, at, 2);
    uint8_t* r = s_patternUnit + records;
    r[patternMinInteger] = 1;
    r[patternPrimary] = 3;
    r[patternSecondary] = 3;
    return (DataSpan){s_patternUnit, textAt + text};
}

// Root's numbers unit with every entry's decimal and percent pattern the
// pattern unit's one.
static DataSpan RootUnit(void)
{
    if (s_root.size > sizeof(s_rootUnit))
    {
        return (DataSpan){nullptr, 0};
    }
    memcpy(s_rootUnit, s_root.bytes, s_root.size);
    for (uint32_t i = 0; i < s_rootUnit[localeEntryCount]; i++)
    {
        uint8_t* e = s_rootUnit + numberLocaleHeaderSize + (size_t)i * numberEntrySize;
        PackPut(e + entryDecimal, 0, 2);
        PackPut(e + entryPercent, 0, 2);
    }
    return (DataSpan){s_rootUnit, s_root.size};
}

// A formatter over a pack of root's units with the pattern, and the parse
// sets when withSets is set.
static mlocResult Init(const char* const affixes[4], bool percent, bool plus, bool withSets,
                       mlocData* source, mlocNumberFormatter* formatter)
{
    DataSpan patterns = PatternUnit(affixes);
    DataSpan root = RootUnit();
    if (patterns.bytes == nullptr || root.bytes == nullptr)
    {
        return (mlocResult)-100;
    }
    // In the order of their kinds.
    const PackUnit units[] = {
        {kindPlurals, s_plurals, true},        {kindPluralRanges, s_ranges, false},
        {kindNumberSystems, s_systems, false}, {kindNumberSymbols, s_symbols, false},
        {kindNumberPatterns, patterns, false}, {kindNumbers, root, true},
        {kindParseSets, s_parseSets, false},
    };
    if (!OpenRootPack(units, withSets ? 7 : 6, source))
    {
        return (mlocResult)-101;
    }
    mlocDataSet set = {source, 1};
    mlocLocale locale;
    mlocNumberOptions options = {.style = percent ? mloc_numberPercent : mloc_numberDecimal,
                                 .signDisplay = plus ? mloc_signAlways : mloc_signAuto};
    if (mlocParseLocale("und", 3, &locale).status != mloc_success)
    {
        return (mlocResult)-102;
    }
    return mlocInitNumberFormatter(formatter, &set, &locale, &options);
}

static bool SameValue(const mlocDecimal* a, const mlocDecimal* b)
{
    return a->kind == b->kind && a->negative == b->negative && a->count == b->count &&
           (a->count == 0 || a->exponent == b->exponent) &&
           memcmp(a->digits, b->digits, a->count) == 0;
}

static bool Parses(const Case* c)
{
    mlocData source;
    mlocNumberFormatter formatter;
    // The percent pattern is the only one with the percent sign.
    bool percent = c->pattern == 6;
    if (Init(s_patterns[c->pattern].affixes, percent, c->plus, true, &source, &formatter) !=
        mloc_success)
    {
        return false;
    }
    mlocDecimal decimal;
    size_t end = 0;
    size_t length = strlen(c->text);
    char* copy = malloc(length == 0 ? 1 : length);
    if (copy == nullptr)
    {
        return false;
    }
    memcpy(copy, c->text, length);
    mlocResult r = mlocParseNumber(
        &formatter, copy, length, c->strict ? mloc_parseStrict : mloc_parseLenient, &decimal, &end);
    free(copy);
    if (c->expected == nullptr)
    {
        return r == mloc_errorSyntax && end == c->end;
    }
    mlocDecimal expected;
    return r == mloc_success && end == c->end &&
           mlocParseDecimal(c->expected, strlen(c->expected), &expected).status == mloc_success &&
           SameValue(&decimal, &expected);
}

static void TestAffixes(void)
{
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        if (!Parses(&s_cases[i]))
        {
            CHECK(false, s_cases[i].text);
        }
    }
}

// The status of parsing text strictly with a pattern of a prefix.
static mlocResult ParsePrefixed(const char* prefix, const char* text, bool strict)
{
    const char* affixes[4] = {prefix, "", "\x01", ""};
    mlocData source;
    mlocNumberFormatter formatter;
    if (Init(affixes, false, false, true, &source, &formatter) != mloc_success)
    {
        return (mlocResult)-100;
    }
    mlocDecimal decimal;
    size_t end = 0;
    return mlocParseNumber(&formatter, text, strlen(text),
                           strict ? mloc_parseStrict : mloc_parseLenient, &decimal, &end);
}

static void TestBounds(void)
{
    // A literal is a matcher, with an ignorables matcher between two in
    // lenient mode: 16 literals fit, 17 do not.
    CHECK(ParsePrefixed("abcdefghijklmnop", "abcdefghijklmnop12", false) == mloc_success,
          "16 literals in lenient mode");
    CHECK(ParsePrefixed("abcdefghijklmnopq", "abcdefghijklmnopq12", false) == mloc_errorCapacity,
          "17 literals in lenient mode");
    CHECK(ParsePrefixed("abcdefghijklmnopqrstuvwxyzABCDEF", "abcdefghijklmnopqrstuvwxyzABCDEF12",
                        true) == mloc_success,
          "32 literals in strict mode");
    // 21 and 22 three-byte literals: 63 bytes fit, 66 do not.
    char prefix[80] = {0};
    char text[90] = {0};
    for (int count = 21; count <= 22; count++)
    {
        for (int i = 0; i < count; i++)
        {
            memcpy(prefix + 3 * i, "\xE3\x81\x82", 3);
        }
        prefix[3 * count] = '\0';
        memcpy(text, prefix, (size_t)(3 * count));
        memcpy(text + 3 * count, "12", 3);
        CHECK(ParsePrefixed(prefix, text, true) ==
                  (count == 21 ? mloc_success : mloc_errorCapacity),
              count == 21 ? "an affix of 63 bytes" : "an affix of 66 bytes");
    }
    const char* plain[4] = {"", "", "\x01", ""};
    mlocData source;
    mlocNumberFormatter formatter;
    mlocDecimal decimal;
    size_t end = 0;
    CHECK(Init(plain, false, false, false, &source, &formatter) == mloc_success &&
              mlocParseNumber(&formatter, "12", 2, mloc_parseLenient, &decimal, &end) ==
                  mloc_errorNotFound,
          "data without the parse sets formats but does not parse");
}

static void TestCodePoints(void)
{
    static const struct
    {
        const char* bytes;
        size_t length;
        uint32_t cp;
        size_t size;
    } s_reads[] = {
        {"\x7F", 1, 0x7F, 1},
        {"\x80", 1, parseNoCodePoint, 1},
        {"\xC1\xBF", 2, parseNoCodePoint, 1},
        {"\xC2\x80", 2, 0x80, 2},
        {"\xDF\xBF", 2, 0x7FF, 2},
        {"\xE0\x9F\xBF", 3, parseNoCodePoint, 1},
        {"\xE0\xA0\x80", 3, 0x800, 3},
        {"\xED\x9F\xBF", 3, 0xD7FF, 3},
        {"\xED\xA0\x80", 3, parseNoCodePoint, 1},
        {"\xEF\xBF\xBF", 3, 0xFFFF, 3},
        {"\xF0\x8F\xBF\xBF", 4, parseNoCodePoint, 1},
        {"\xF0\x90\x80\x80", 4, 0x10000, 4},
        {"\xF4\x8F\xBF\xBF", 4, 0x10FFFF, 4},
        {"\xF4\x90\x80\x80", 4, parseNoCodePoint, 1},
        {"\xF5\x80\x80\x80", 4, parseNoCodePoint, 1},
        {"\xE0\xA0", 2, parseNoCodePoint, 1},
        {"\xE0\x41\x80", 3, parseNoCodePoint, 1},
    };
    for (size_t i = 0; i < sizeof(s_reads) / sizeof(s_reads[0]); i++)
    {
        size_t size = 0;
        uint32_t cp =
            mlocParseCodePoint((const uint8_t*)s_reads[i].bytes, s_reads[i].length, 0, &size);
        CHECK(cp == s_reads[i].cp && size == s_reads[i].size, "a code point read");
    }
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    uint32_t root = 0;
    CHECK(mlocFindDataUnit(&s_view, kindNumberSystems, dataNoLocale, &s_systems) &&
              mlocFindDataUnit(&s_view, kindNumberSymbols, dataNoLocale, &s_symbols) &&
              mlocFindDataLocale(&s_view, "root", 4, &root) &&
              mlocFindDataUnit(&s_view, kindNumbers, root, &s_root) &&
              mlocFindDataUnit(&s_view, kindPlurals, root, &s_plurals) &&
              mlocFindDataUnit(&s_view, kindPluralRanges, dataNoLocale, &s_ranges) &&
              mlocFindDataUnit(&s_view, kindParseSets, dataNoLocale, &s_parseSets),
          "the compiled-in data has the units");
    if (s_failures > 0)
    {
        return 1;
    }
    TestAffixes();
    TestBounds();
    TestCodePoints();
    return s_failures == 0 ? 0 : 1;
}
