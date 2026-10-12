// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number units' validators (number_data.h): the compiled-in units
// pass, a copy with one field spoiled is refused; and a formatter refuses
// units that pass alone but disagree, in a pack built here.

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "number_data.h"
#include "number_tables.h"
#include "test_harness.h"
#include "test_pack.h"

#include "maul-locale/number.h"

#include <stdlib.h>
#include <string.h>

static DataView s_view;
static DataSpan s_systems;
static DataSpan s_symbols;
static DataSpan s_patterns;
static DataSpan s_root;
static DataSpan s_compactPatterns;
static DataSpan s_compact;
static DataSpan s_plurals;
static DataSpan s_ranges;

// A copy of a unit with 64 bytes of room, filled with valid text so that
// a validator reading past the unit reads on until the sanitizer builds
// catch it.
static uint8_t* Copy(DataSpan unit)
{
    uint8_t* copy = malloc(unit.size + 64);
    if (copy != nullptr)
    {
        memcpy(copy, unit.bytes, unit.size);
        memset(copy + unit.size, 'a', 64);
    }
    return copy;
}

typedef bool (*Validator)(const DataView* view, DataSpan payload);

static bool Valid(Validator validate, const uint8_t* bytes, size_t size)
{
    return validate(&s_view, (DataSpan){bytes, (uint32_t)size});
}

static void Put16(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void TestSystems(void)
{
    CHECK(mlocValidNumberSystems(&s_view, s_systems), "the compiled-in systems are valid");
    uint8_t* u = Copy(s_systems);
    uint32_t size = s_systems.size;
    uint8_t* first = u + numberSystemsHeaderSize;
    uint8_t* second = first + numberSystemSize;
    CHECK(!Valid(mlocValidNumberSystems, u, size - 1), "a short unit");
    CHECK(!Valid(mlocValidNumberSystems, u, 4), "shorter than its header");
    u[4] = 1;
    CHECK(!Valid(mlocValidNumberSystems, u, size), "the reserved word");
    u[4] = 0;
    uint8_t saved[numberSystemSize];
    memcpy(saved, first, sizeof(saved));
    memcpy(first, second, 8);
    CHECK(!Valid(mlocValidNumberSystems, u, size), "two equal ids");
    memcpy(first, saved, sizeof(saved));
    first[0] = 'A';
    CHECK(!Valid(mlocValidNumberSystems, u, size), "an id that is no system id");
    first[0] = saved[0];
    // latn's digits: the first system with ASCII digits.
    uint8_t* latn = nullptr;
    for (uint32_t i = 0; i < mlocLoad32(u); i++)
    {
        uint8_t* e = u + numberSystemsHeaderSize + (size_t)i * numberSystemSize;
        if (mlocLoad64(e) == numberLatn)
        {
            latn = e;
        }
    }
    CHECK(latn != nullptr && latn[8] == '0' && latn[8 + 9 * numberDigitSize] == '9',
          "latn's digits are 0 to 9");
    if (latn != nullptr)
    {
        uint8_t* digit = latn + 8 + 3 * numberDigitSize;
        static const uint8_t s_bad[][numberDigitSize] = {
            {0xC0, 0x80, 0, 0},       // overlong
            {0xED, 0xA0, 0x80, 0},    // a surrogate
            {0xF5, 0x80, 0x80, 0x80}, // past U+10FFFF
            {0x20, 0, 0, 0},          // a space
            {0, 0, 0, 0},             // empty
            {'3', 'x', 0, 0},         // a byte past the character
            {0xD9, 0xA3, 0, 0},       // a digit of another length
        };
        for (size_t b = 0; b < sizeof(s_bad) / sizeof(s_bad[0]); b++)
        {
            uint8_t keep[numberDigitSize];
            memcpy(keep, digit, sizeof(keep));
            memcpy(digit, s_bad[b], sizeof(keep));
            CHECK(!Valid(mlocValidNumberSystems, u, size), "a digit that is not one character");
            memcpy(digit, keep, sizeof(keep));
        }
        CHECK(Valid(mlocValidNumberSystems, u, size), "restored");
    }
    u[0] = 0;
    u[1] = 0;
    CHECK(!Valid(mlocValidNumberSystems, u, numberSystemsHeaderSize), "no system");
    free(u);
    // A count the u8 indexes cannot name, at its exact size.
    size_t big = numberSystemsHeaderSize + (size_t)numberAlgorithmic * numberSystemSize;
    uint8_t* many = calloc(big, 1);
    if (many != nullptr)
    {
        many[0] = numberAlgorithmic;
        // Two-letter ids, ordered as packed numbers: by the second letter,
        // then the first.
        for (uint32_t i = 0; i < numberAlgorithmic; i++)
        {
            uint8_t* e = many + numberSystemsHeaderSize + (size_t)i * numberSystemSize;
            e[0] = (uint8_t)('a' + i % 26);
            e[1] = (uint8_t)('a' + i / 26);
            for (int d = 0; d < 10; d++)
            {
                e[8 + d * numberDigitSize] = (uint8_t)('0' + d);
            }
        }
        CHECK(!Valid(mlocValidNumberSystems, many, big), "254 systems");
        many[0] = numberAlgorithmic - 1;
        CHECK(Valid(mlocValidNumberSystems, many, big - numberSystemSize), "253 systems");
        free(many);
    }
}

// Checks the table rules both table units share.
static void TestTable(Validator validate, DataSpan unit, uint32_t recordSize, const char* name)
{
    CHECK(validate(&s_view, unit), name);
    uint8_t* u = Copy(unit);
    uint32_t size = unit.size;
    uint32_t strings = mlocLoad16(u);
    uint32_t records = mlocLoad16(u + 2);
    uint8_t* offsets = u + numberTableHeaderSize;
    uint8_t* text = offsets + (size_t)(strings + 1) * 2 + (size_t)records * recordSize;
    CHECK(!Valid(validate, u, size - 1), "a short table");
    CHECK(!Valid(validate, u, 7), "shorter than its header");
    offsets[0] = 1;
    CHECK(!Valid(validate, u, size), "a first offset past 0");
    offsets[0] = 0;
    uint32_t last = mlocLoad16(offsets + (size_t)strings * 2);
    Put16(offsets + (size_t)strings * 2, last - 1);
    CHECK(!Valid(validate, u, size), "a last offset short of the text");
    Put16(offsets + (size_t)strings * 2, last);
    // Two strings' bounds out of order: the second ends before it starts.
    uint32_t second = mlocLoad16(offsets + 4);
    Put16(offsets + 4, mlocLoad16(offsets + 2) - 1);
    CHECK(mlocLoad16(offsets + 2) == 0 || !Valid(validate, u, size), "offsets out of order");
    Put16(offsets + 4, second);
    // A first string ending past the text, the next one's end back
    // within it: refused before the first is read.
    if (strings >= 2)
    {
        uint32_t keepFirst = mlocLoad16(offsets + 2);
        Put16(offsets + 2, 60000);
        CHECK(!Valid(validate, u, size), "a string ending past the text");
        Put16(offsets + 2, keepFirst);
    }
    // Spoil a byte of the first non-empty string.
    for (uint32_t s = 0; s < strings; s++)
    {
        uint32_t start = mlocLoad16(offsets + (size_t)s * 2);
        if (mlocLoad16(offsets + (size_t)s * 2 + 2) > start)
        {
            uint8_t keep = text[start];
            static const uint8_t s_bad[] = {0x00, 0x09, 0x80, 0xFF, 0xC1};
            for (size_t b = 0; b < sizeof(s_bad); b++)
            {
                text[start] = s_bad[b];
                CHECK(!Valid(validate, u, size), "a string byte that is no text");
            }
            text[start] = keep;
            break;
        }
    }
    CHECK(Valid(validate, u, size), "restored");
    free(u);
}

static void TestSymbols(void)
{
    TestTable(mlocValidNumberSymbols, s_symbols, 2 * numberSymbolCount,
              "the compiled-in symbol sets are valid");
    uint8_t* u = Copy(s_symbols);
    uint32_t strings = mlocLoad16(u);
    uint8_t* records = u + numberTableHeaderSize + (size_t)(strings + 1) * 2;
    Put16(records + 2 * (numberSymbolCount - 1), strings);
    CHECK(!Valid(mlocValidNumberSymbols, u, s_symbols.size), "a symbol past the strings");
    Put16(records + 2 * (numberSymbolCount - 1), strings - 1);
    CHECK(Valid(mlocValidNumberSymbols, u, s_symbols.size), "the last string");
    free(u);
}

static void TestPatterns(void)
{
    TestTable(mlocValidNumberPatterns, s_patterns, numberPatternSize,
              "the compiled-in patterns are valid");
    uint8_t* u = Copy(s_patterns);
    uint32_t size = s_patterns.size;
    uint32_t strings = mlocLoad16(u);
    uint8_t* p = u + numberTableHeaderSize + (size_t)(strings + 1) * 2;
    uint8_t keep[numberPatternSize];
    memcpy(keep, p, sizeof(keep));
    for (int a = 0; a < 4; a++)
    {
        Put16(p + 2 * a, strings);
        CHECK(!Valid(mlocValidNumberPatterns, u, size), "an affix past the strings");
        memcpy(p, keep, sizeof(keep));
    }
    typedef struct Case
    {
        int field;
        uint8_t value;
        bool valid;
        const char* name;
    } Case;
    static const Case s_cases[] = {
        {patternMinInteger, 40, true, "40 integer digits"},
        {patternMinInteger, 41, false, "41 integer digits"},
        {patternMaxFraction, 40, true, "40 fraction digits"},
        {patternMaxFraction, 41, false, "41 fraction digits"},
        {patternSecondary, 9, true, "secondary groups of 9"},
        {patternMinFraction, 4, false, "a minimum past the maximum"},
        {patternPrimary, 9, true, "groups of 9"},
        {patternPrimary, 10, false, "groups of 10"},
        {patternSecondary, 10, false, "secondary groups of 10"},
        {patternExponentDigits, 9, true, "9 exponent digits"},
        {patternExponentDigits, 10, false, "10 exponent digits"},
        {patternFlags, 2, false, "an unknown flag"},
        {15, 1, false, "the reserved byte"},
    };
    for (size_t c = 0; c < sizeof(s_cases) / sizeof(s_cases[0]); c++)
    {
        p[s_cases[c].field] = s_cases[c].value;
        if (s_cases[c].field == patternMaxFraction || s_cases[c].field == patternMinFraction)
        {
            p[s_cases[c].field == patternMinFraction ? patternMaxFraction : patternMinFraction] = 3;
        }
        CHECK(Valid(mlocValidNumberPatterns, u, size) == s_cases[c].valid, s_cases[c].name);
        memcpy(p, keep, sizeof(keep));
    }
    p[patternPrimary] = 0;
    p[patternSecondary] = 3;
    CHECK(!Valid(mlocValidNumberPatterns, u, size), "a secondary size without a primary one");
    p[patternPrimary] = 3;
    p[patternSecondary] = 0;
    CHECK(!Valid(mlocValidNumberPatterns, u, size), "a primary size without a secondary one");
    p[patternPrimary] = 0;
    CHECK(Valid(mlocValidNumberPatterns, u, size), "no grouping");
    memcpy(p, keep, sizeof(keep));
    free(u);
    // Placeholders: each up to the narrow currency's, and no other.
    uint8_t table[numberTableHeaderSize + 4 + 2] = {1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 2, 0};
    table[12] = 'a';
    for (uint8_t b = 1; b <= numberAffixCurrencyNarrow; b++)
    {
        table[13] = b;
        CHECK(Valid(mlocValidNumberPatterns, table, sizeof(table)), "a placeholder");
    }
    table[13] = numberAffixCurrencyNarrow + 1;
    CHECK(!Valid(mlocValidNumberPatterns, table, sizeof(table)), "a byte past the placeholders");
    table[13] = 0;
    CHECK(!Valid(mlocValidNumberPatterns, table, sizeof(table)), "a NUL");
    table[13] = numberAffixMinus;
    CHECK(!Valid(mlocValidNumberSymbols, table, sizeof(table)), "a placeholder among symbols");
    table[13] = ' ';
    CHECK(Valid(mlocValidNumberSymbols, table, sizeof(table)), "a space");
    // The last plane's first character, U+100000, and the last, U+10FFFF.
    uint8_t wide[numberTableHeaderSize + 4 + 4] = {1, 0, 0, 0, 4,    0,    0,    0,
                                                   0, 0, 4, 0, 0xF4, 0x80, 0x80, 0x80};
    CHECK(Valid(mlocValidNumberSymbols, wide, sizeof(wide)), "U+100000");
    wide[13] = 0x8F;
    wide[14] = 0xBF;
    wide[15] = 0xBF;
    CHECK(Valid(mlocValidNumberSymbols, wide, sizeof(wide)), "U+10FFFF");
    wide[13] = 0x90;
    CHECK(!Valid(mlocValidNumberSymbols, wide, sizeof(wide)), "past U+10FFFF");
}

static void TestCompactUnits(void)
{
    TestTable(mlocValidNumberCompactPatterns, s_compactPatterns, numberCompactPatternSize,
              "the compiled-in compact patterns are valid");
    uint8_t* u = Copy(s_compactPatterns);
    uint32_t size = s_compactPatterns.size;
    uint32_t strings = mlocLoad16(u);
    uint8_t* p = u + numberTableHeaderSize + (size_t)(strings + 1) * 2;
    uint8_t keep[numberCompactPatternSize];
    memcpy(keep, p, sizeof(keep));
    const int fields[3] = {compactPrefix, compactSuffix, compactNegativePrefix};
    for (int f = 0; f < 3; f++)
    {
        Put16(p + fields[f], strings);
        CHECK(!Valid(mlocValidNumberCompactPatterns, u, size), "a compact affix past the strings");
        memcpy(p, keep, sizeof(keep));
    }
    p[compactFlags] = patternFlagNoNumber;
    CHECK(Valid(mlocValidNumberCompactPatterns, u, size), "a pattern without digits");
    p[compactFlags] = patternFlagExponentPlus;
    CHECK(!Valid(mlocValidNumberCompactPatterns, u, size), "a flag compact patterns have not");
    free(u);

    CHECK(mlocValidNumberCompact(&s_view, s_compact), "the compiled-in compact tables are valid");
    // One table: its first row (10^3) compacts by the pattern given, the
    // others do not compact.
    uint8_t t[numberCompactHeaderSize + 8 + 2 * numberCompactRows + 2] = {1};
    uint8_t* row = t + numberCompactHeaderSize + 8;
    t[numberCompactHeaderSize + 4] = (uint8_t)(sizeof(t) - numberCompactHeaderSize - 8);
    row[0] = 1;
    row[1] = 1u << numberCompactOther;
    CHECK(Valid(mlocValidNumberCompact, t, sizeof(t)), "a table of one compacted row");
    typedef struct RowCase
    {
        uint8_t zeros;
        uint8_t mask;
        bool valid;
        const char* name;
    } RowCase;
    static const RowCase s_rows[] = {
        {4, 1u << numberCompactOther, true, "four zeros at 10^3"},
        {5, 1u << numberCompactOther, false, "five zeros at 10^3"},
        {1, 1u << numberCompactExactOne, false, "a compacted row without the other slot"},
        {0, 1u << numberCompactOther, false, "a slot in a row that does not compact"},
        {1, 0x80 | 1u << numberCompactOther, false, "a slot past the seven"},
        {1, 3u << (numberCompactOther - 1), false, "more slots than the table holds"},
    };
    for (size_t i = 0; i < sizeof(s_rows) / sizeof(s_rows[0]); i++)
    {
        row[0] = s_rows[i].zeros;
        row[1] = s_rows[i].mask;
        CHECK(Valid(mlocValidNumberCompact, t, sizeof(t)) == s_rows[i].valid, s_rows[i].name);
    }
    row[0] = 1;
    row[1] = 1u << numberCompactOther;
    t[4] = 1;
    CHECK(!Valid(mlocValidNumberCompact, t, sizeof(t)), "the reserved word");
    t[4] = 0;
    t[numberCompactHeaderSize] = 1;
    CHECK(!Valid(mlocValidNumberCompact, t, sizeof(t)), "a first offset past 0");
    t[numberCompactHeaderSize] = 0;
    CHECK(!Valid(mlocValidNumberCompact, t, sizeof(t) - 1), "a table ending past its unit");
    t[0] = 200;
    CHECK(!Valid(mlocValidNumberCompact, t, sizeof(t)), "more offsets than the unit holds");
    // Two tables, the first empty: its offsets agree, but it holds no row.
    uint8_t two[numberCompactHeaderSize + 12 + 2 * numberCompactRows + 2] = {2};
    uint32_t tableSize = 2 * numberCompactRows + 2;
    two[numberCompactHeaderSize + 8] = (uint8_t)tableSize;
    two[numberCompactHeaderSize + 12 + 1] = 0;
    two[numberCompactHeaderSize + 12] = 1;
    two[numberCompactHeaderSize + 12 + 1] = 1u << numberCompactOther;
    CHECK(!Valid(mlocValidNumberCompact, two, sizeof(two)), "an empty table");
    two[numberCompactHeaderSize + 4] = (uint8_t)tableSize;
    two[numberCompactHeaderSize + 8] = (uint8_t)(2 * tableSize);
    CHECK(!Valid(mlocValidNumberCompact, two, sizeof(two)), "a second table past the unit");
    t[0] = 1;
    CHECK(!Valid(mlocValidNumberCompact, t, 7), "shorter than its header");
}

static void TestLocaleUnit(void)
{
    CHECK(mlocValidNumbers(&s_view, s_root), "root's numbers are valid");
    uint8_t* u = Copy(s_root);
    uint32_t size = s_root.size;
    CHECK(!Valid(mlocValidNumbers, u, size - 1), "a short unit");
    CHECK(!Valid(mlocValidNumbers, u, numberLocaleHeaderSize - 1), "shorter than its header");
    typedef struct Case
    {
        int field;
        uint8_t value;
        bool valid;
        const char* name;
    } Case;
    static const Case s_cases[] = {
        {6, 1, false, "the first reserved byte"},
        {7, 1, false, "the second"},
        {localeMinGrouping, 0, false, "no minimum grouping"},
        {localeMinGrouping, 4, true, "a minimum grouping of 4"},
        {localeMinGrouping, 5, false, "of 5"},
        {localeDefaultSystem, numberAlgorithmic, false, "an algorithmic default"},
        {localeDefaultSystem, numberNoSystem, false, "no default"},
        {localeNativeSystem, numberNoSystem, false, "no native system"},
        {localeNativeSystem, numberAlgorithmic, true, "an algorithmic native system"},
        {localeTraditionalSystem, numberNoSystem, true, "no traditional system"},
        {localeEntryCount, 0, false, "no entry"},
        {numberLocaleHeaderSize + 1, 1, false, "an entry's reserved byte"},
        {numberLocaleHeaderSize, numberAlgorithmic, false, "an entry of an algorithmic system"},
    };
    for (size_t c = 0; c < sizeof(s_cases) / sizeof(s_cases[0]); c++)
    {
        uint8_t keep = u[s_cases[c].field];
        u[s_cases[c].field] = s_cases[c].value;
        CHECK(Valid(mlocValidNumbers, u, size) == s_cases[c].valid, s_cases[c].name);
        u[s_cases[c].field] = keep;
    }
    free(u);
    // Two entries, in order and not.
    uint8_t two[numberLocaleHeaderSize + 2 * numberEntrySize] = {0, 0, 0xFF, 0xFF, 1, 2};
    two[numberLocaleHeaderSize] = 3;
    two[numberLocaleHeaderSize + numberEntrySize] = 5;
    CHECK(Valid(mlocValidNumbers, two, sizeof(two)), "two entries in order");
    two[numberLocaleHeaderSize + numberEntrySize] = 3;
    CHECK(!Valid(mlocValidNumbers, two, sizeof(two)), "two entries of one system");
}

// Opens a pack of root's plural rules, the compiled-in number units of no
// locale and a root numbers unit, into a data set of it alone.
static bool OpenPack(const uint8_t* root, uint32_t rootSize, mlocData* source)
{
    // In the order of their kinds.
    const PackUnit units[] = {
        {kindPlurals, s_plurals, true},
        {kindPluralRanges, s_ranges, false},
        {kindNumberSystems, s_systems, false},
        {kindNumberSymbols, s_symbols, false},
        {kindNumberPatterns, s_patterns, false},
        {kindNumbers, {root, rootSize}, true},
        {kindNumberCompactPatterns, s_compactPatterns, false},
        {kindNumberCompact, s_compact, false},
    };
    return OpenRootPack(units, sizeof(units) / sizeof(units[0]), source);
}

// The status of a formatter made for und over a pack with this root
// unit, in a notation.
static mlocResult InitStyled(const uint8_t* root, uint32_t size, mlocNotation notation,
                             mlocNumberStyle style)
{
    mlocData source;
    if (!OpenPack(root, size, &source))
    {
        return (mlocResult)-100;
    }
    mlocDataSet set = {&source, 1};
    mlocLocale locale;
    mlocNumberFormatter formatter;
    mlocNumberOptions options = {.notation = notation, .style = style};
    if (mlocParseLocale("und", 3, &locale).status != mloc_success)
    {
        return (mlocResult)-101;
    }
    return mlocInitNumberFormatter(&formatter, &set, &locale, &options);
}

static mlocResult InitAs(const uint8_t* root, uint32_t size, mlocNotation notation)
{
    return InitStyled(root, size, notation, mloc_numberDecimal);
}

static mlocResult InitWith(const uint8_t* root, uint32_t size)
{
    return InitAs(root, size, mloc_notationStandard);
}

static void TestDisagreeing(void)
{
    uint8_t* u = Copy(s_root);
    uint32_t size = s_root.size;
    CHECK(InitWith(u, size) == mloc_success, "a pack of root's numbers");
    uint32_t systems = mlocLoad32(s_systems.bytes);
    uint8_t keep = u[localeTraditionalSystem];
    u[localeTraditionalSystem] = (uint8_t)systems;
    CHECK(InitWith(u, size) == mloc_errorNotFound, "a system past the table");
    u[localeTraditionalSystem] = (uint8_t)(systems - 1);
    CHECK(InitWith(u, size) == mloc_success, "the last system of the table");
    u[localeTraditionalSystem] = keep;
    keep = u[localeFinanceSystem];
    u[localeFinanceSystem] = (uint8_t)systems;
    CHECK(InitWith(u, size) == mloc_errorNotFound, "a finance system past the table");
    u[localeFinanceSystem] = keep;
    // latn's entry, which the default system, latn, reads.
    uint8_t* entry = u + numberLocaleHeaderSize;
    for (uint32_t i = 0; i < u[localeEntryCount]; i++)
    {
        uint8_t* e = u + numberLocaleHeaderSize + (size_t)i * numberEntrySize;
        if (mlocLoad64(s_systems.bytes + numberSystemsHeaderSize +
                       (size_t)e[entrySystem] * numberSystemSize) == numberLatn)
        {
            entry = e;
        }
    }
    uint32_t sets = mlocLoad16(s_symbols.bytes + 2);
    uint32_t patterns = mlocLoad16(s_patterns.bytes + 2);
    // Each index is bounded where a formatter reads it: the percent
    // pattern by a percent formatter alone.
    const int fields[3] = {entrySymbols, entryDecimal, entryPercent};
    for (int f = 0; f < 3; f++)
    {
        uint32_t value = mlocLoad16(entry + fields[f]);
        uint32_t past = f == 0 ? sets : patterns;
        mlocNumberStyle style = f == 2 ? mloc_numberPercent : mloc_numberDecimal;
        Put16(entry + fields[f], past);
        CHECK(InitStyled(u, size, mloc_notationStandard, style) == mloc_errorNotFound,
              "an index past its table");
        CHECK(f != 2 || InitWith(u, size) == mloc_success, "a decimal formatter reads no percent");
        Put16(entry + fields[f], past - 1);
        CHECK(InitStyled(u, size, mloc_notationStandard, style) == mloc_success,
              "the last of its table");
        Put16(entry + fields[f], value);
    }
    uint32_t tables = mlocLoad32(s_compact.bytes);
    for (int length = 0; length < 2; length++)
    {
        int field = length == 0 ? entryCompactShort : entryCompactLong;
        mlocNotation notation = length == 0 ? mloc_notationCompactShort : mloc_notationCompactLong;
        uint32_t value = mlocLoad16(entry + field);
        CHECK(InitAs(u, size, notation) == mloc_success, "a compact formatter of the pack");
        Put16(entry + field, tables);
        CHECK(InitAs(u, size, notation) == mloc_errorNotFound && InitWith(u, size) == mloc_success,
              "a compact table past the unit, which only compact notation reads");
        Put16(entry + field, value);
    }
    // A unit of one entry, not latn's, leaves none for the default.
    uint8_t one[numberLocaleHeaderSize + numberEntrySize];
    memcpy(one, u, numberLocaleHeaderSize);
    memcpy(one + numberLocaleHeaderSize, entry, numberEntrySize);
    one[localeEntryCount] = 1;
    CHECK(InitWith(one, sizeof(one)) == mloc_success, "latn's entry alone");
    one[numberLocaleHeaderSize + entrySystem] = (uint8_t)(entry[entrySystem] == 0 ? 1 : 0);
    CHECK(InitWith(one, sizeof(one)) == mloc_errorNotFound, "no entry for the system nor for latn");
    // A compact table naming a pattern past the unit, or one without
    // digits as its other one, is refused when a formatter is made.
    uint32_t count = mlocLoad16(s_compactPatterns.bytes + 2);
    uint32_t strings = mlocLoad16(s_compactPatterns.bytes);
    const uint8_t* records =
        s_compactPatterns.bytes + numberTableHeaderSize + (size_t)(strings + 1) * 2;
    uint32_t wordless = count;
    uint32_t digits = count;
    for (uint32_t i = 0; i < count; i++)
    {
        bool none = (records[(size_t)i * numberCompactPatternSize + compactFlags] &
                     patternFlagNoNumber) != 0;
        wordless = none ? i : wordless;
        digits = none ? digits : i;
    }
    CHECK(wordless < count && digits < count, "compact patterns with and without digits");
    uint8_t table[numberCompactHeaderSize + 8 + 2 * numberCompactRows + 2] = {1};
    table[numberCompactHeaderSize + 4] = (uint8_t)(sizeof(table) - numberCompactHeaderSize - 8);
    uint8_t* row = table + numberCompactHeaderSize + 8;
    row[0] = 1;
    row[1] = 1u << numberCompactOther;
    DataSpan saved = s_compact;
    s_compact = (DataSpan){table, sizeof(table)};
    memcpy(one + numberLocaleHeaderSize, entry, numberEntrySize);
    Put16(one + numberLocaleHeaderSize + entryCompactShort, 0);
    const uint32_t indexes[3] = {digits, count, wordless};
    const mlocResult expected[3] = {mloc_success, mloc_errorNotFound, mloc_errorNotFound};
    for (int i = 0; i < 3; i++)
    {
        Put16(row + 2, indexes[i]);
        CHECK(InitAs(one, sizeof(one), mloc_notationCompactShort) == expected[i],
              "a compact table's patterns checked at init");
    }
    s_compact = saved;

    free(u);
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    uint32_t root = 0;
    CHECK(mlocFindDataUnit(&s_view, kindNumberSystems, dataNoLocale, &s_systems) &&
              mlocFindDataUnit(&s_view, kindNumberSymbols, dataNoLocale, &s_symbols) &&
              mlocFindDataUnit(&s_view, kindNumberPatterns, dataNoLocale, &s_patterns) &&
              mlocFindDataLocale(&s_view, "root", 4, &root) &&
              mlocFindDataUnit(&s_view, kindNumbers, root, &s_root) &&
              mlocFindDataUnit(&s_view, kindNumberCompactPatterns, dataNoLocale,
                               &s_compactPatterns) &&
              mlocFindDataUnit(&s_view, kindNumberCompact, dataNoLocale, &s_compact) &&
              mlocFindDataUnit(&s_view, kindPlurals, root, &s_plurals) &&
              mlocFindDataUnit(&s_view, kindPluralRanges, dataNoLocale, &s_ranges),
          "the compiled-in data has the number units");
    if (s_failures > 0)
    {
        return 1;
    }
    TestSystems();
    TestSymbols();
    TestPatterns();
    TestCompactUnits();
    TestLocaleUnit();
    TestDisagreeing();
    return s_failures == 0 ? 0 : 1;
}
