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

#include "maul-locale/number.h"

#include <stdlib.h>
#include <string.h>

static DataView s_view;
static DataSpan s_systems;
static DataSpan s_symbols;
static DataSpan s_patterns;
static DataSpan s_root;

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

alignas(8) static uint8_t s_pack[32768];

static void Put(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// Opens a pack of the compiled-in systems, symbols and patterns and a
// root numbers unit, into a data set of it alone.
static bool OpenPack(const uint8_t* root, uint32_t rootSize, mlocData* source)
{
    const DataSpan units[4] = {s_systems, s_symbols, s_patterns, {root, rootSize}};
    const uint32_t kinds[4] = {kindNumberSystems, kindNumberSymbols, kindNumberPatterns,
                               kindNumbers};
    memset(s_pack, 0, sizeof(s_pack));
    uint32_t at = dataHeaderSize + 4 * dataUnitSize + dataLocaleSize;
    for (int i = 0; i < 4; i++)
    {
        at = (at + 7) / 8 * 8;
        uint8_t* unit = s_pack + dataHeaderSize + i * dataUnitSize;
        Put(unit, kinds[i], 4);
        Put(unit + 4, i == 3 ? 0 : dataNoLocale, 4);
        Put(unit + 8, at, 4);
        Put(unit + 12, units[i].size, 4);
        if (at + units[i].size > sizeof(s_pack) - 16)
        {
            return false;
        }
        memcpy(s_pack + at, units[i].bytes, units[i].size);
        at += units[i].size;
    }
    uint32_t pool = at;
    memcpy(s_pack + at, "root", 4);
    at += 4;
    uint8_t* entry = s_pack + dataHeaderSize + 4 * dataUnitSize;
    Put(entry, 0, 4);
    Put(entry + 4, 4, 4);
    mlocDataVersion version;
    mlocGetDataVersion(&version);
    memcpy(s_pack, MLOC_DATA_MAGIC, 4);
    Put(s_pack + headerFormat, dataFormatVersion, 2);
    Put(s_pack + headerHeaderSize, dataHeaderSize, 2);
    Put(s_pack + headerFileSize, at, 4);
    Put(s_pack + headerCldr,
        mlocPackVersion(version.cldr.major, version.cldr.minor, version.cldr.patch), 4);
    Put(s_pack + headerUnicode,
        mlocPackVersion(version.unicode.major, version.unicode.minor, version.unicode.patch), 4);
    Put(s_pack + headerLibrary,
        mlocPackVersion(version.library.major, version.library.minor, version.library.patch), 4);
    memcpy(s_pack + headerTzdb, version.tzdb, 8);
    Put(s_pack + headerTables, version.tables, 8);
    Put(s_pack + headerUnitCount, 4, 4);
    Put(s_pack + headerLocaleCount, 1, 4);
    Put(s_pack + headerPoolOffset, pool, 4);
    Put(s_pack + headerPoolSize, 4, 4);
    return mlocOpenData(source, s_pack, at) == mloc_success;
}

// The status of a formatter made for und over a pack with this root
// unit.
static mlocResult InitWith(const uint8_t* root, uint32_t size)
{
    mlocData source;
    if (!OpenPack(root, size, &source))
    {
        return (mlocResult)-100;
    }
    mlocDataSet set = {&source, 1};
    mlocLocale locale;
    mlocNumberFormatter formatter;
    if (mlocParseLocale("und", 3, &locale).status != mloc_success)
    {
        return (mlocResult)-101;
    }
    return mlocInitNumberFormatter(&formatter, &set, &locale, nullptr);
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
    const int fields[3] = {entrySymbols, entryDecimal, entryPercent};
    for (int f = 0; f < 3; f++)
    {
        uint32_t value = mlocLoad16(entry + fields[f]);
        uint32_t past = f == 0 ? sets : patterns;
        Put16(entry + fields[f], past);
        CHECK(InitWith(u, size) == mloc_errorNotFound, "an index past its table");
        Put16(entry + fields[f], past - 1);
        CHECK(InitWith(u, size) == mloc_success, "the last of its table");
        Put16(entry + fields[f], value);
    }
    // A unit of one entry, not latn's, leaves none for the default.
    uint8_t one[numberLocaleHeaderSize + numberEntrySize];
    memcpy(one, u, numberLocaleHeaderSize);
    memcpy(one + numberLocaleHeaderSize, entry, numberEntrySize);
    one[localeEntryCount] = 1;
    CHECK(InitWith(one, sizeof(one)) == mloc_success, "latn's entry alone");
    one[numberLocaleHeaderSize + entrySystem] = (uint8_t)(entry[entrySystem] == 0 ? 1 : 0);
    CHECK(InitWith(one, sizeof(one)) == mloc_errorNotFound, "no entry for the system nor for latn");
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
              mlocFindDataUnit(&s_view, kindNumbers, root, &s_root),
          "the compiled-in data has the number units");
    if (s_failures > 0)
    {
        return 1;
    }
    TestSystems();
    TestSymbols();
    TestPatterns();
    TestLocaleUnit();
    TestDisagreeing();
    return s_failures == 0 ? 0 : 1;
}
