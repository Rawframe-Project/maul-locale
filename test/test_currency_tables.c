// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The currency units' validators and reads (number_data.h,
// currency_tables.h): the compiled-in units pass, units with one field
// spoiled are refused, lookups find what the units hold; and a currency
// formatter refuses units that pass alone but disagree, in a pack built
// here.

#include "bytes.h"
#include "currency_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "number_data.h"
#include "number_tables.h"
#include "test_harness.h"
#include "test_pack.h"

#include "maul-locale/currency.h"

#include <string.h>

static DataView s_view;
static DataSpan s_fractions;
static DataSpan s_sets;
static DataSpan s_rootSymbols;

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

static void TestSets(void)
{
    CHECK(mlocValidCurrencyPatterns(&s_view, s_sets), "the compiled-in sets are valid");
    uint8_t unit[numberTableHeaderSize + 2 + currencySetSize] = {0, 0, 1};
    CHECK(Valid(mlocValidCurrencyPatterns, unit, sizeof(unit)), "a set of no strings");
    CHECK(!Valid(mlocValidCurrencyPatterns, unit, sizeof(unit) - 1), "a short set");
    uint8_t strings[numberTableHeaderSize + 4 + currencySetSize + 1] = {1, 0, 1, 0, 1};
    strings[numberTableHeaderSize + 2] = 1;
    strings[sizeof(strings) - 1] = 'a';
    CHECK(!Valid(mlocValidCurrencyPatterns, strings, sizeof(strings)), "a set with a string");
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
}

// A formatter of EUR for und over a pack of the compiled-in number units,
// root's numbers and this root currency symbols unit.
static mlocResult InitOver(const uint8_t* symbols, uint32_t size)
{
    DataSpan systems;
    DataSpan numberSymbols;
    DataSpan patterns;
    DataSpan numbers;
    uint32_t root = 0;
    if (!mlocFindDataUnit(&s_view, kindNumberSystems, dataNoLocale, &systems) ||
        !mlocFindDataUnit(&s_view, kindNumberSymbols, dataNoLocale, &numberSymbols) ||
        !mlocFindDataUnit(&s_view, kindNumberPatterns, dataNoLocale, &patterns) ||
        !mlocFindDataLocale(&s_view, "root", 4, &root) ||
        !mlocFindDataUnit(&s_view, kindNumbers, root, &numbers))
    {
        return (mlocResult)-100;
    }
    // In the order of their kinds.
    const PackUnit units[] = {
        {kindNumberSystems, systems, false},          {kindNumberSymbols, numberSymbols, false},
        {kindNumberPatterns, patterns, false},        {kindNumbers, numbers, true},
        {kindCurrencyFractions, s_fractions, false},  {kindCurrencyPatterns, s_sets, false},
        {kindCurrencySymbols, {symbols, size}, true},
    };
    mlocData source;
    mlocLocale locale;
    mlocCurrencyFormatter formatter;
    if (!OpenRootPack(units, sizeof(units) / sizeof(units[0]), &source) ||
        mlocParseLocale("und", 3, &locale).status != mloc_success)
    {
        return (mlocResult)-101;
    }
    mlocDataSet set = {&source, 1};
    return mlocInitCurrencyFormatter(&formatter, &set, &locale, "EUR", 3, nullptr);
}

static void TestDisagreeing(void)
{
    DataSpan patterns;
    CHECK(mlocFindDataUnit(&s_view, kindNumberPatterns, dataNoLocale, &patterns),
          "the patterns unit");
    uint32_t count = mlocLoad16(patterns.bytes + 2);
    uint8_t u[testSize];
    Build(u);
    Put16(u + testRowBytes + 3, count - 1);
    CHECK(InitOver(u, testSize) == mloc_success, "a currency's own pattern, the last");
    Put16(u + testRowBytes + 3, count);
    CHECK(InitOver(u, testSize) == mloc_errorNotFound, "a currency's own pattern past the unit");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_view = mlocViewOf(&builtin);
    uint32_t root = 0;
    CHECK(mlocFindDataUnit(&s_view, kindCurrencyFractions, dataNoLocale, &s_fractions) &&
              mlocFindDataUnit(&s_view, kindCurrencyPatterns, dataNoLocale, &s_sets) &&
              mlocFindDataLocale(&s_view, "root", 4, &root) &&
              mlocFindDataUnit(&s_view, kindCurrencySymbols, root, &s_rootSymbols),
          "the compiled-in data has the currency units");
    if (s_failures > 0)
    {
        return 1;
    }
    TestCodes();
    TestFractions();
    TestSets();
    TestSymbols();
    TestDisagreeing();
    return s_failures == 0 ? 0 : 1;
}
