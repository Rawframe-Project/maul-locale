// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The currency units' validators and reads (number_data.h,
// currency_region.h, currency_tables.h): the compiled-in units pass, units with one field
// spoiled are refused, lookups find what the units hold; and a currency
// formatter refuses units that pass alone but disagree, in a pack built
// here.

#include "bytes.h"
#include "currency_region.h"
#include "currency_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_tables.h"
#include "number_data.h"
#include "number_tables.h"
#include "test_harness.h"
#include "test_pack.h"

#include "maul-locale/currency.h"

#include <stdlib.h>
#include <string.h>

static DataView s_view;
static DataSpan s_fractions;
static DataSpan s_sets;
static DataSpan s_rootSymbols;
static DataSpan s_regions;

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

static uint16_t Code(const char* code)
{
    return mlocPackCurrency(code, 3);
}

static void TestCodes(void)
{
    char out[3];
    mlocUnpackCurrency(Code("usd"), out);
    CHECK(Code("AAA") == 1 && Code("ZZZ") == currencyMaxCode && memcmp(out, "USD", 3) == 0,
          "codes pack in order, in any case, and unpack");
    CHECK(Code("U$D") == 0 && mlocPackCurrency("US", 2) == 0 && mlocPackCurrency("USDX", 4) == 0,
          "only three letters pack");
}

static void TestFractions(void)
{
    CHECK(mlocValidCurrencyFractions(&s_view, s_fractions), "the compiled-in fractions are valid");
    CurrencyFraction usd = mlocFindCurrencyFraction(s_fractions, Code("USD"));
    CurrencyFraction jpy = mlocFindCurrencyFraction(s_fractions, Code("JPY"));
    CurrencyFraction chf = mlocFindCurrencyFraction(s_fractions, Code("CHF"));
    CurrencyFraction bhd = mlocFindCurrencyFraction(s_fractions, Code("BHD"));
    CHECK(usd.digits == 2 && usd.rounding == 0 && usd.cashDigits == 2, "the default for USD");
    CHECK(jpy.digits == 0 && bhd.digits == 3, "yen and dinar digits");
    CHECK(chf.digits == 2 && chf.cashDigits == 2 && chf.cashRounding == 5, "Swiss cash rounding");
    uint8_t unit[currencyFractionsHeaderSize + 2 * currencyFractionSize] = {2};
    uint8_t* first = unit + currencyFractionsHeaderSize;
    uint8_t* second = first + currencyFractionSize;
    first[fractionDigits] = 2;
    first[fractionCashDigits] = 2;
    Put16(second + fractionCode, Code("JPY"));
    CHECK(Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), "a default and one currency");
    second[fractionDigits] = currencyMaxDigits;
    CHECK(Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), "the most digits");
    second[fractionDigits] = 0;
    CHECK(!Valid(mlocValidCurrencyFractions, unit, sizeof(unit) - 1) &&
              !Valid(mlocValidCurrencyFractions, unit, 4),
          "a size that is not the entries'");
    unit[0] = 0;
    CHECK(!Valid(mlocValidCurrencyFractions, unit, currencyFractionsHeaderSize), "no entry");
    unit[0] = 2;
    unit[4] = 1;
    CHECK(!Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), "a header's nonzero field");
    unit[4] = 0;
    struct
    {
        int offset;
        uint8_t value;
        const char* what;
    } const spoils[] = {
        {fractionDigits, currencyMaxDigits + 1, "too many digits"},
        {fractionCashDigits, currencyMaxDigits + 1, "too many cash digits"},
        {fractionRounding, 3, "a rounding no increment says"},
        {fractionCashRounding, 100, "a cash rounding no increment says"},
        {6, 1, "a nonzero padding byte"},
    };
    for (size_t i = 0; i < sizeof(spoils) / sizeof(spoils[0]); i++)
    {
        uint8_t keep = second[spoils[i].offset];
        second[spoils[i].offset] = spoils[i].value;
        CHECK(!Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), spoils[i].what);
        second[spoils[i].offset] = keep;
    }
    Put16(second + fractionCode, 0);
    CHECK(!Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), "the default twice");
    Put16(second + fractionCode, currencyMaxCode + 1);
    CHECK(!Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), "a code past the last");
    Put16(first + fractionCode, Code("JPY"));
    Put16(second + fractionCode, Code("USD"));
    CHECK(!Valid(mlocValidCurrencyFractions, unit, sizeof(unit)), "no default first");
}

static void Put32(uint8_t* p, uint32_t value)
{
    Put16(p, value & 0xFFFF);
    Put16(p + 2, value >> 16);
}

enum
{
    regionsSize = currencyRegionsHeaderSize + 2 * currencyRegionSize + 3 * currencyRegionEntrySize,
};

// Two regions, the first with two entries, the second with one.
static void BuildRegions(uint8_t* u)
{
    memset(u, 0, regionsSize);
    Put16(u, 2);
    Put16(u + 2, 3);
    uint8_t* regions = u + currencyRegionsHeaderSize;
    Put16(regions, mlocPackRegion("DE", 2));
    Put16(regions + currencyRegionSize, mlocPackRegion("US", 2));
    Put16(regions + currencyRegionSize + 2, 2);
    uint8_t* entries = regions + 2 * currencyRegionSize;
    for (int i = 0; i < 3; i++)
    {
        uint8_t* e = entries + (size_t)i * currencyRegionEntrySize;
        Put16(e + regionEntryCode, Code(i == 0 ? "EUR" : i == 1 ? "DEM" : "USD"));
        e[regionEntryTender] = 1;
        Put32(e + regionEntryFrom, (uint32_t)INT32_MIN);
        Put32(e + regionEntryTo, (uint32_t)INT32_MAX);
    }
}

static void TestRegions(void)
{
    CHECK(mlocValidCurrencyRegions(&s_view, s_regions), "the compiled-in regions are valid");
    uint8_t u[regionsSize];
    BuildRegions(u);
    CHECK(Valid(mlocValidCurrencyRegions, u, sizeof(u)), "two regions, three entries");
    uint8_t* regions = u + currencyRegionsHeaderSize;
    uint8_t* entries = regions + 2 * currencyRegionSize;
    uint8_t* last = entries + 2 * currencyRegionEntrySize;
    Put32(last + regionEntryFrom, 100);
    Put32(last + regionEntryTo, 100);
    last[regionEntryTender] = 0;
    CHECK(Valid(mlocValidCurrencyRegions, u, sizeof(u)), "a one-day span, no tender");
    Put32(last + regionEntryTo, 99);
    CHECK(!Valid(mlocValidCurrencyRegions, u, sizeof(u)), "a span that ends before it starts");
    Put32(last + regionEntryFrom, (uint32_t)-5);
    Put32(last + regionEntryTo, (uint32_t)-6);
    CHECK(!Valid(mlocValidCurrencyRegions, u, sizeof(u)), "a span before 1970 that ends first");
    Put32(last + regionEntryTo, (uint32_t)-5);
    CHECK(Valid(mlocValidCurrencyRegions, u, sizeof(u)), "a day before 1970");
    CHECK(!Valid(mlocValidCurrencyRegions, u, sizeof(u) - 1) &&
              !Valid(mlocValidCurrencyRegions, u, sizeof(u) - currencyRegionEntrySize) &&
              !Valid(mlocValidCurrencyRegions, u, 3),
          "a size that is not the header's");
    struct
    {
        uint8_t* at;
        uint32_t value;
        int bytes;
        const char* what;
    } const spoils[] = {
        {last + regionEntryCode, 0, 2, "a code of 0"},
        {last + regionEntryCode, currencyMaxCode + 1, 2, "a code past the last"},
        {last + regionEntryTender, 2, 1, "a tender flag that is no flag"},
        {last + 3, 1, 1, "a nonzero padding byte"},
        {regions + 2, 1, 2, "a first region not starting at 0"},
        {regions + currencyRegionSize + 2, 0, 2, "a region without entries"},
        {regions + currencyRegionSize + 2, 3, 2, "a region past the entries"},
        {regions + currencyRegionSize, mlocPackRegion("DE", 2), 2, "a region twice"},
        {regions + currencyRegionSize, mlocPackRegion("AD", 2), 2, "regions out of order"},
        {regions, 31, 2, "a packed region that is none"},
    };
    for (size_t i = 0; i < sizeof(spoils) / sizeof(spoils[0]); i++)
    {
        uint8_t keep[2] = {spoils[i].at[0], spoils[i].at[1]};
        if (spoils[i].bytes == 2)
        {
            Put16(spoils[i].at, spoils[i].value);
        }
        else
        {
            spoils[i].at[0] = (uint8_t)spoils[i].value;
        }
        CHECK(!Valid(mlocValidCurrencyRegions, u, sizeof(u)), spoils[i].what);
        memcpy(spoils[i].at, keep, 2);
    }
    CHECK(Valid(mlocValidCurrencyRegions, u, sizeof(u)), "each spoil undone");
    uint8_t empty[currencyRegionsHeaderSize] = {0};
    CHECK(Valid(mlocValidCurrencyRegions, empty, sizeof(empty)), "no regions");
}

static void TestSets(void)
{
    CHECK(mlocValidCurrencyPatterns(&s_view, s_sets), "the compiled-in sets are valid");
    uint8_t* copy = malloc(s_sets.size);
    if (copy == nullptr)
    {
        CHECK(false, "room for the sets");
        return;
    }
    memcpy(copy, s_sets.bytes, s_sets.size);
    NumberTable t = mlocReadNumberTable((DataSpan){copy, s_sets.size}, currencySetSize);
    uint8_t* last = (uint8_t*)t.records + (size_t)(t.recordCount - 1) * currencySetSize;
    for (int at = setUnitPatterns; at <= setAppend; at += 2)
    {
        uint16_t keep = mlocLoad16(last + at);
        Put16(last + at, t.stringCount);
        CHECK(!Valid(mlocValidCurrencyPatterns, copy, s_sets.size),
              "a unit pattern past the strings");
        Put16(last + at, keep);
    }
    CHECK(Valid(mlocValidCurrencyPatterns, copy, s_sets.size), "the copy restored");
    free(copy);
    // One set whose texts are all the one string, "\x01 \x02".
    uint8_t unit[numberTableHeaderSize + 4 + currencySetSize + 3] = {1, 0, 1, 0, 3};
    unit[numberTableHeaderSize + 2] = 3;
    memcpy(unit + sizeof(unit) - 3, "\x01 \x02", 3);
    CHECK(Valid(mlocValidCurrencyPatterns, unit, sizeof(unit)), "a set and its one text");
    CHECK(!Valid(mlocValidCurrencyPatterns, unit, sizeof(unit) - 1), "a short set");
    unit[sizeof(unit) - 1] = currencyTextOther + 1;
    CHECK(!Valid(mlocValidCurrencyPatterns, unit, sizeof(unit)),
          "a text with a byte past its places");
    unit[sizeof(unit) - 1] = currencyTextNumber;
    CHECK(!Valid(mlocValidCurrencyPatterns, unit, sizeof(unit)), "a text with two number places");
    unit[sizeof(unit) - 3] = ' ';
    unit[sizeof(unit) - 1] = ' ';
    CHECK(!Valid(mlocValidCurrencyPatterns, unit, sizeof(unit)), "a text without its places");
    uint8_t none[numberTableHeaderSize + 2 + currencySetSize] = {0, 0, 1};
    CHECK(!Valid(mlocValidCurrencyPatterns, none, sizeof(none)), "a set without its texts");
}

enum
{
    // The test unit: strings "$" and "US$", entries EUR (symbol "$",
    // pattern 9) and USD (symbol "US$", narrow "$").
    testStrings = 2,
    testEntries = 2,
    testText = 4,
    testRows = 5 + 5,
    testOffsets = sparseHeaderSize,
    testCodes = testOffsets + 2 * (testStrings + 1),
    testRowOffsets = testCodes + 2 * testEntries,
    testRowBytes = testRowOffsets + 2 * (testEntries + 1),
    testTextAt = testRowBytes + testRows,
    testFlagsAt = testTextAt + testText,
    testSize = testFlagsAt + testStrings,
};

static void Build(uint8_t* u)
{
    memset(u, 0, testSize);
    Put16(u, testStrings);
    Put16(u + 2, testEntries);
    u[4] = testText;
    u[8] = testRows;
    Put16(u + testOffsets + 2, 1);
    Put16(u + testOffsets + 4, 4);
    Put16(u + testCodes, Code("EUR"));
    Put16(u + testCodes + 2, Code("USD"));
    Put16(u + testRowOffsets + 2, 5);
    Put16(u + testRowOffsets + 4, 10);
    uint8_t* row = u + testRowBytes;
    row[0] = 1u << currencySymbol | 1u << currencyOwnPattern;
    Put16(row + 1, 0);
    Put16(row + 3, 9);
    row[5] = 1u << currencySymbol | 1u << currencyNarrow;
    Put16(row + 6, 1);
    Put16(row + 8, 0);
    memcpy(u + testTextAt, "$US$", 4);
    u[testFlagsAt + 1] = currencyStartsAlpha;
}

static bool ValidTest(const uint8_t* u)
{
    return Valid(mlocValidCurrencySymbols, u, testSize);
}

static void TestSymbols(void)
{
    CHECK(mlocValidCurrencySymbols(&s_view, s_rootSymbols), "root's compiled-in symbols are valid");
    uint8_t u[testSize];
    Build(u);
    CHECK(ValidTest(u), "the test unit");
    // Text and row sizes each 2^31 larger: the same sum modulo 2^32.
    u[7] = (uint8_t)(u[7] + 0x80);
    u[11] = (uint8_t)(u[11] + 0x80);
    CHECK(!ValidTest(u), "sizes that wrap 32 bits");
    Build(u);
    SparseTable t = mlocReadSparseTable((DataSpan){u, testSize});
    const uint8_t* usd = mlocFindSparseRow(&t, Code("USD"));
    const uint8_t* eur = mlocFindSparseRow(&t, Code("EUR"));
    uint16_t value = 0;
    CHECK(usd != nullptr && mlocSparseField(usd, currencyNarrow, &value) && value == 0 &&
              mlocSparseField(usd, currencySymbol, &value) && value == 1 &&
              !mlocSparseField(usd, currencyOwnPattern, &value),
          "a row's fields by the mask");
    CHECK(eur != nullptr && mlocSparseField(eur, currencyOwnPattern, &value) && value == 9 &&
              mlocFindSparseRow(&t, Code("GBP")) == nullptr &&
              mlocFindSparseRow(&t, Code("AAA")) == nullptr &&
              mlocFindSparseRow(&t, Code("ZZZ")) == nullptr,
          "rows by code");
    CHECK(!Valid(mlocValidCurrencySymbols, u, testSize - 1) &&
              !Valid(mlocValidCurrencySymbols, u, 4),
          "a size that is not the parts'");
    struct
    {
        int offset;
        uint32_t value;
        bool wide;
        const char* what;
    } const spoils[] = {
        {testOffsets, 1, true, "a first string offset not 0"},
        {testOffsets + 2, 0, true, "an empty string"},
        {testOffsets + 2, 5, true, "a string past the text"},
        {testCodes + 2, Code("EUR"), true, "a code twice"},
        {testCodes, 0, true, "code 0"},
        {testCodes + 2, currencyMaxCode + 1, true, "a code past the last"},
        {testRowOffsets, 1, true, "a first row offset not 0"},
        {testRowOffsets + 2, 4, true, "a row shorter than its mask"},
        {testRowOffsets + 2, 11, true, "a row past the rows"},
        {testRowBytes, 0, false, "an empty mask"},
        {testRowBytes, 1u << currencyFieldCount | 1, false, "a field past the last"},
        {testRowBytes + 1, testStrings, true, "a string index past the strings"},
        {testTextAt, 0x1F, false, "a control character"},
        {testTextAt, 0xFF, false, "malformed UTF-8"},
        {testFlagsAt, 4, false, "a flag no flag is"},
    };
    for (size_t i = 0; i < sizeof(spoils) / sizeof(spoils[0]); i++)
    {
        Build(u);
        if (spoils[i].wide)
        {
            Put16(u + spoils[i].offset, spoils[i].value);
        }
        else
        {
            u[spoils[i].offset] = (uint8_t)spoils[i].value;
        }
        CHECK(!ValidTest(u), spoils[i].what);
    }
    Build(u);
    Put16(u + testRowBytes + 3, 0xFFFF);
    CHECK(ValidTest(u), "a pattern index is bounded when a formatter reads it");
    // The names shape: every field a string, no flags.
    Build(u);
    Put16(u + testRowBytes + 3, 1);
    CHECK(Valid(mlocValidCurrencyNames, u, testSize - testStrings), "a names unit");
    CHECK(!Valid(mlocValidCurrencyNames, u, testSize), "a names unit with flags");
    Put16(u + testRowBytes + 3, testStrings);
    CHECK(!Valid(mlocValidCurrencyNames, u, testSize - testStrings),
          "a name past the strings, where symbols hold a pattern index");
}

// What a pack holds beside the compiled-in number units of no locale:
// root's numbers, the fractions and the sets (each left out when its
// size is 0) and root's currency symbols.
typedef struct Pack
{
    DataSpan numbers;
    DataSpan fractions;
    DataSpan sets;
    DataSpan symbols;
} Pack;

static DataSpan s_numbers;
static DataSpan s_numberSystems;
static DataSpan s_numberSymbols;
static DataSpan s_numberPatterns;

// A formatter of EUR for und over a pack.
static mlocResult InitOver(const Pack* p)
{
    // In the order of their kinds.
    PackUnit units[7];
    uint32_t count = 0;
    units[count++] = (PackUnit){kindNumberSystems, s_numberSystems, false};
    units[count++] = (PackUnit){kindNumberSymbols, s_numberSymbols, false};
    units[count++] = (PackUnit){kindNumberPatterns, s_numberPatterns, false};
    units[count++] = (PackUnit){kindNumbers, p->numbers, true};
    if (p->fractions.size > 0)
    {
        units[count++] = (PackUnit){kindCurrencyFractions, p->fractions, false};
    }
    if (p->sets.size > 0)
    {
        units[count++] = (PackUnit){kindCurrencyPatterns, p->sets, false};
    }
    units[count++] = (PackUnit){kindCurrencySymbols, p->symbols, true};
    mlocData source;
    mlocLocale locale;
    mlocCurrencyFormatter formatter;
    if (!OpenRootPack(units, count, &source) ||
        mlocParseLocale("und", 3, &locale).status != mloc_success)
    {
        return (mlocResult)-101;
    }
    mlocDataSet set = {&source, 1};
    return mlocInitCurrencyFormatter(&formatter, &set, &locale, "EUR", 3, nullptr);
}

static void TestDisagreeing(void)
{
    uint32_t count = mlocLoad16(s_numberPatterns.bytes + 2);
    uint8_t u[testSize];
    Build(u);
    Pack p = {s_numbers, s_fractions, s_sets, {u, testSize}};
    Put16(u + testRowBytes + 3, count - 1);
    CHECK(InitOver(&p) == mloc_success, "a currency's own pattern, the last");
    Put16(u + testRowBytes + 3, count);
    CHECK(InitOver(&p) == mloc_errorNotFound, "a currency's own pattern past the unit");
    Build(u);
    p.fractions = (DataSpan){0};
    CHECK(InitOver(&p) == mloc_errorNotFound, "no fractions");
    p.fractions = s_fractions;
    p.sets = (DataSpan){0};
    CHECK(InitOver(&p) == mloc_errorNotFound, "no pattern sets");
    p.sets = s_sets;
    // Every entry of root's numbers names a set past the sets, or a set
    // whose patterns pass the patterns.
    uint8_t numbers[1024];
    uint32_t sets = mlocLoad16(s_sets.bytes + 2);
    if (s_numbers.size > sizeof(numbers))
    {
        CHECK(false, "room for root's numbers");
        return;
    }
    memcpy(numbers, s_numbers.bytes, s_numbers.size);
    for (uint32_t i = 0; i < numbers[localeEntryCount]; i++)
    {
        Put16(numbers + numberLocaleHeaderSize + i * numberEntrySize + entryCurrency, sets);
    }
    p.numbers = (DataSpan){numbers, s_numbers.size};
    CHECK(InitOver(&p) == mloc_errorNotFound, "a set past the sets");
    for (uint32_t i = 0; i < numbers[localeEntryCount]; i++)
    {
        Put16(numbers + numberLocaleHeaderSize + i * numberEntrySize + entryCurrency, sets - 1);
    }
    CHECK(InitOver(&p) == mloc_success, "the last set");
    static uint8_t spoiled[65536];
    if (s_sets.size > sizeof(spoiled))
    {
        CHECK(false, "room for the sets");
        return;
    }
    for (int field = 0; field < currencyPatternCount; field++)
    {
        memcpy(spoiled, s_sets.bytes, s_sets.size);
        NumberTable t = mlocReadNumberTable((DataSpan){spoiled, s_sets.size}, currencySetSize);
        uint8_t* last = (uint8_t*)t.records + (size_t)(sets - 1) * currencySetSize;
        Put16(last + 2 * field, count);
        p.sets = (DataSpan){spoiled, s_sets.size};
        CHECK(InitOver(&p) == mloc_errorNotFound, "a set's pattern past the patterns");
    }
    p.sets = s_sets;
    p.numbers = s_numbers;
    // A symbol longer than a core holds.
    enum
    {
        longSize = sparseHeaderSize + 4 + 2 + 4 + 3 + 256 + 1,
    };
    uint8_t wide[longSize] = {1, 0, 1, 0, 0, 1, 0, 0, 3};
    uint8_t* at = wide + sparseHeaderSize;
    Put16(at + 2, 256);
    Put16(at + 4, Code("EUR"));
    Put16(at + 8, 3);
    at[10] = 1u << currencySymbol;
    memset(at + 13, 'a', 256);
    p.symbols = (DataSpan){wide, longSize};
    CHECK(Valid(mlocValidCurrencySymbols, wide, longSize), "a long symbol is valid alone");
    CHECK(InitOver(&p) == mloc_errorNotFound, "a symbol longer than a core holds");
    Put16(wide + 4, 255);
    Put16(at + 2, 255);
    at[13 + 255] = 0;
    p.symbols = (DataSpan){wide, longSize - 1};
    CHECK(Valid(mlocValidCurrencySymbols, wide, longSize - 1), "a symbol of 255 bytes is valid");
    CHECK(InitOver(&p) == mloc_success, "a symbol of 255 bytes");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    uint32_t root = 0;
    CHECK(mlocFindDataUnit(&s_view, kindCurrencyFractions, dataNoLocale, &s_fractions) &&
              mlocFindDataUnit(&s_view, kindCurrencyPatterns, dataNoLocale, &s_sets) &&
              mlocFindDataUnit(&s_view, kindCurrencyRegions, dataNoLocale, &s_regions) &&
              mlocFindDataLocale(&s_view, "root", 4, &root) &&
              mlocFindDataUnit(&s_view, kindCurrencySymbols, root, &s_rootSymbols) &&
              mlocFindDataUnit(&s_view, kindNumbers, root, &s_numbers) &&
              mlocFindDataUnit(&s_view, kindNumberSystems, dataNoLocale, &s_numberSystems) &&
              mlocFindDataUnit(&s_view, kindNumberSymbols, dataNoLocale, &s_numberSymbols) &&
              mlocFindDataUnit(&s_view, kindNumberPatterns, dataNoLocale, &s_numberPatterns),
          "the compiled-in data has the currency units");
    if (s_failures > 0)
    {
        return 1;
    }
    TestCodes();
    TestFractions();
    TestRegions();
    TestSets();
    TestSymbols();
    TestDisagreeing();
    return s_failures == 0 ? 0 : 1;
}
