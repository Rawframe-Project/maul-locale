// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a failed call of the number headers (decimal.h, plural.h,
// number.h, currency.h, unit.h, spellout.h) leaves in its outputs: each
// output filled with a pattern, each call made to fail in the ways the
// compiled-in data lets it, and every byte of the pattern checked after.
// A failure leaves its outputs as they were (base.h), but for the length
// mloc_errorCapacity gives an mlocOutput, the count it gives a list and
// the offset where a parse stopped.

#include "test_harness.h"

#include "maul-locale/currency.h"
#include "maul-locale/decimal.h"
#include "maul-locale/number.h"
#include "maul-locale/plural.h"
#include "maul-locale/spellout.h"
#include "maul-locale/unit.h"

#include <stdint.h>
#include <string.h>

enum
{
    pattern = 0xA5,
};

static mlocDataSet s_data;
static mlocDataSet s_empty;
static mlocLocale s_en;

static void Fill(void* output, size_t size)
{
    memset(output, pattern, size);
}

static bool Kept(const void* output, size_t size)
{
    const unsigned char* bytes = output;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != pattern)
        {
            return false;
        }
    }
    return true;
}

static mlocDecimal Decimal(const char* text)
{
    mlocDecimal decimal = {0};
    CHECK(mlocParseDecimal(text, strlen(text), &decimal).status == mloc_success,
          "a test decimal parses");
    return decimal;
}

// An output over a patterned buffer, its length a pattern too.
typedef struct Text
{
    char buffer[32];
    mlocOutput output;
} Text;

static void FillText(Text* text)
{
    Fill(text->buffer, sizeof(text->buffer));
    text->output = (mlocOutput){.buffer = text->buffer, .capacity = sizeof(text->buffer)};
    Fill(&text->output.length, sizeof(text->output.length));
}

static bool KeptText(const Text* text)
{
    return Kept(text->buffer, sizeof(text->buffer)) &&
           Kept(&text->output.length, sizeof(text->output.length));
}

static void TestDecimal(void)
{
    mlocDecimal decimal;
    char digits[MLOC_MAX_DECIMAL_DIGITS + 1];
    memset(digits, '9', sizeof(digits));
    Fill(&decimal, sizeof(decimal));
    CHECK(mlocParseDecimal("1x", 2, &decimal).status == mloc_errorSyntax &&
              mlocParseDecimal(digits, sizeof(digits), &decimal).status == mloc_errorLimit &&
              mlocParseDecimal(nullptr, 1, &decimal).status == mloc_errorInvalid &&
              Kept(&decimal, sizeof(decimal)),
          "a parse refused leaves the decimal");
    mlocDecimal bad = Decimal("1");
    bad.digits[0] = 10;
    Text text;
    FillText(&text);
    CHECK(mlocFormatDecimal(&bad, &text.output) == mloc_errorInvalid && KeptText(&text),
          "a format refused leaves the output");
    mlocPluralOperands operands;
    mlocDecimal nan = {.kind = mloc_decimalNaN};
    Fill(&operands, sizeof(operands));
    CHECK(mlocGetPluralOperands(&nan, &operands) == mloc_errorInvalid &&
              mlocGetPluralOperands(&bad, &operands) == mloc_errorInvalid &&
              Kept(&operands, sizeof(operands)),
          "operands refused leave the operands");
}

static void TestPlural(void)
{
    mlocPluralRules rules;
    Fill(&rules, sizeof(rules));
    CHECK(mlocInitPluralRules(&s_empty, &s_en, mloc_pluralCardinal, &rules) == mloc_errorNotFound &&
              mlocInitPluralRules(&s_data, &s_en, (mlocPluralType)2, &rules) == mloc_errorInvalid &&
              Kept(&rules, sizeof(rules)),
          "rules refused leave the rules");
    mlocPluralRules ordinal;
    mlocPluralRules forged;
    memset(&forged, 0, sizeof(forged));
    mlocDecimal one = Decimal("1");
    mlocDecimal bad = one;
    bad.digits[0] = 10;
    mlocPluralOperands operands = {.i = 1, .v = 0};
    mlocPluralCategory category;
    uint32_t categories;
    Fill(&category, sizeof(category));
    Fill(&categories, sizeof(categories));
    CHECK(mlocInitPluralRules(&s_data, &s_en, mloc_pluralCardinal, &rules) == mloc_success &&
              mlocInitPluralRules(&s_data, &s_en, mloc_pluralOrdinal, &ordinal) == mloc_success &&
              mlocPluralRules_Select(&forged, &one, &category) == mloc_errorInvalid &&
              mlocPluralRules_Select(&rules, &bad, &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectInteger(&forged, 1, &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectOperands(&forged, &operands, &category) == mloc_errorInvalid &&
              mlocPluralRules_SelectRange(&ordinal, mloc_pluralOne, mloc_pluralOther, &category) ==
                  mloc_errorInvalid &&
              mlocPluralRules_SelectRange(&rules, mloc_pluralOne, (mlocPluralCategory)6,
                                          &category) == mloc_errorInvalid &&
              mlocPluralRules_GetCategories(&forged, &categories) == mloc_errorInvalid &&
              Kept(&category, sizeof(category)) && Kept(&categories, sizeof(categories)),
          "a selection refused leaves the category");
}

// Rounding to one significant digit carries 9.9 * 10^999999 past the
// decimal's exponent.
static const mlocPrecision s_oneDigit = {
    .kind = mloc_precisionSignificant,
    .minSignificantDigits = 1,
    .maxSignificantDigits = 1,
};

static void TestNumber(void)
{
    mlocNumberFormatter formatter;
    mlocNumberOptions bad = {.notation = (mlocNotation)9};
    Fill(&formatter, sizeof(formatter));
    CHECK(mlocInitNumberFormatter(&s_empty, &s_en, nullptr, &formatter) == mloc_errorNotFound &&
              mlocInitNumberFormatter(&s_data, &s_en, &bad, &formatter) == mloc_errorInvalid &&
              Kept(&formatter, sizeof(formatter)),
          "a formatter refused leaves the formatter");
    mlocNumberFormatter forged;
    memset(&forged, 0, sizeof(forged));
    mlocNumberOptions options = {.precision = s_oneDigit};
    mlocDecimal huge = Decimal("9.9e999999");
    mlocDecimal one = Decimal("1");
    mlocDecimal nan = {.kind = mloc_decimalNaN};
    Text text;
    FillText(&text);
    CHECK(mlocInitNumberFormatter(&s_data, &s_en, &options, &formatter) == mloc_success &&
              mlocNumberFormatter_Format(&formatter, &huge, &text.output) == mloc_errorLimit &&
              mlocNumberFormatter_Format(&forged, &one, &text.output) == mloc_errorInvalid &&
              mlocNumberFormatter_FormatRange(&formatter, &one, &huge, &text.output) ==
                  mloc_errorLimit &&
              mlocNumberFormatter_FormatRange(&formatter, &one, &nan, &text.output) ==
                  mloc_errorInvalid &&
              KeptText(&text),
          "a format refused leaves the output");
    mlocDecimal decimal;
    size_t end;
    Fill(&decimal, sizeof(decimal));
    Fill(&end, sizeof(end));
    CHECK(mlocNumberFormatter_Parse(&forged, "1", 1, mloc_parseLenient, &decimal, &end) ==
                  mloc_errorInvalid &&
              Kept(&decimal, sizeof(decimal)) && Kept(&end, sizeof(end)),
          "a parse refused leaves its outputs");
    CHECK(mlocNumberFormatter_Parse(&formatter, "x", 1, mloc_parseLenient, &decimal, &end) ==
                  mloc_errorSyntax &&
              Kept(&decimal, sizeof(decimal)) && end == 0,
          "a parse that reads nothing gives only where it stopped");
}

static void TestCurrencyFormatter(void)
{
    mlocCurrencyFormatter formatter;
    Fill(&formatter, sizeof(formatter));
    CHECK(mlocInitCurrencyFormatter(&s_empty, &s_en, "USD", 3, nullptr, &formatter) ==
                  mloc_errorNotFound &&
              mlocInitCurrencyFormatter(&s_data, &s_en, "US", 2, nullptr, &formatter) ==
                  mloc_errorInvalid &&
              Kept(&formatter, sizeof(formatter)),
          "a formatter refused leaves the formatter");
    mlocCurrencyFormatter forged;
    memset(&forged, 0, sizeof(forged));
    mlocCurrencyOptions options = {.number.precision = s_oneDigit};
    mlocDecimal huge = Decimal("9.9e999999");
    mlocDecimal one = Decimal("1");
    mlocDecimal nan = {.kind = mloc_decimalNaN};
    Text text;
    FillText(&text);
    CHECK(mlocInitCurrencyFormatter(&s_data, &s_en, "USD", 3, &options, &formatter) ==
                  mloc_success &&
              mlocCurrencyFormatter_Format(&formatter, &huge, &text.output) == mloc_errorLimit &&
              mlocCurrencyFormatter_Format(&forged, &one, &text.output) == mloc_errorInvalid &&
              mlocCurrencyFormatter_FormatRange(&formatter, &one, &huge, &text.output) ==
                  mloc_errorLimit &&
              mlocCurrencyFormatter_FormatRange(&formatter, &one, &nan, &text.output) ==
                  mloc_errorInvalid &&
              KeptText(&text),
          "a format refused leaves the output");
    mlocDecimal decimal;
    char code[3];
    size_t end;
    Fill(&decimal, sizeof(decimal));
    Fill(code, sizeof(code));
    Fill(&end, sizeof(end));
    CHECK(mlocCurrencyFormatter_Parse(&forged, "$1", 2, mloc_parseLenient, 0, &decimal, code,
                                      &end) == mloc_errorInvalid &&
              Kept(&decimal, sizeof(decimal)) && Kept(code, sizeof(code)) &&
              Kept(&end, sizeof(end)),
          "a parse refused leaves its outputs");
    CHECK(mlocCurrencyFormatter_Parse(&formatter, "x", 1, mloc_parseLenient, 0, &decimal, code,
                                      &end) == mloc_errorSyntax &&
              Kept(&decimal, sizeof(decimal)) && Kept(code, sizeof(code)) && end == 0,
          "a parse that reads nothing gives only where it stopped");
}

static void TestCurrencyData(void)
{
    Text text;
    FillText(&text);
    CHECK(mlocGetCurrencySymbol(&s_empty, &s_en, "USD", 3, mloc_currencySymbol, &text.output) ==
                  mloc_errorNotFound &&
              mlocGetCurrencySymbol(&s_data, &s_en, "USD", 3, mloc_currencyName, &text.output) ==
                  mloc_errorInvalid &&
              mlocGetCurrencyName(&s_empty, &s_en, "USD", 3, &text.output) == mloc_errorNotFound &&
              mlocGetCurrencyName(&s_data, &s_en, "US", 2, &text.output) == mloc_errorInvalid &&
              KeptText(&text),
          "a symbol or name refused leaves the output");
    uint8_t digits;
    uint16_t increment;
    char code[4];
    Fill(&digits, sizeof(digits));
    Fill(&increment, sizeof(increment));
    Fill(code, sizeof(code));
    CHECK(mlocGetCurrencyDigits(&s_empty, "USD", 3, mloc_currencyUsageStandard, &digits,
                                &increment) == mloc_errorNotFound &&
              mlocGetCurrencyDigits(&s_data, "USD", 3, (mlocCurrencyUsage)2, &digits, &increment) ==
                  mloc_errorInvalid &&
              mlocFindCurrency(&s_empty, &s_en, 0, code) == mloc_errorNotFound &&
              Kept(&digits, sizeof(digits)) && Kept(&increment, sizeof(increment)) &&
              Kept(code, sizeof(code)),
          "digits or a currency refused leave theirs");
    mlocRegionCurrency currencies[2];
    size_t count;
    Fill(currencies, sizeof(currencies));
    Fill(&count, sizeof(count));
    CHECK(mlocGetRegionCurrencies(&s_empty, "US", 2, currencies, 2, &count) == mloc_errorNotFound &&
              mlocGetRegionCurrencies(&s_data, "U", 1, currencies, 2, &count) ==
                  mloc_errorInvalid &&
              Kept(&count, sizeof(count)) && Kept(currencies, sizeof(currencies)),
          "currencies refused leave the list and the count");
    CHECK(mlocGetRegionCurrencies(&s_data, "DE", 2, currencies, 1, &count) == mloc_errorCapacity &&
              count > 1 && Kept(currencies, sizeof(currencies)),
          "a list too small gives only the count needed");
}

static void TestUnit(void)
{
    mlocDecimal huge = Decimal("1e999999");
    mlocDecimal one = Decimal("1");
    mlocDecimal value;
    Fill(&value, sizeof(value));
    CHECK(mlocConvertUnit(&s_data, "kilometer", 9, "meter", 5, nullptr, &huge, &value) ==
                  mloc_errorLimit &&
              mlocConvertUnit(&s_data, "meter", 5, "second", 6, nullptr, &one, &value) ==
                  mloc_errorInvalid &&
              Kept(&value, sizeof(value)),
          "a conversion refused leaves the value");
    Text text;
    FillText(&text);
    mlocUnitAmounts amounts;
    Fill(&amounts, sizeof(amounts));
    CHECK(mlocGetUnitPreference(&s_data, &s_en, "meter", 5, "Road", 4, &one, nullptr, &text.output,
                                &amounts) == mloc_errorSyntax &&
              mlocGetUnitPreference(&s_empty, &s_en, "meter", 5, nullptr, 0, &one, nullptr,
                                    &text.output, &amounts) == mloc_errorNotFound &&
              mlocGetUnitPreference(&s_data, &s_en, "foot-and-inch", 13, nullptr, 0, &one, nullptr,
                                    &text.output, &amounts) == mloc_errorInvalid &&
              KeptText(&text) && Kept(&amounts, sizeof(amounts)),
          "a preference refused leaves the unit and the amounts");
    mlocOutput small = {.buffer = text.buffer, .capacity = 1};
    CHECK(mlocGetUnitPreference(&s_data, &s_en, "meter", 5, nullptr, 0, &one, nullptr, &small,
                                &amounts) == mloc_errorCapacity &&
              small.length > 1 && Kept(&amounts, sizeof(amounts)),
          "a unit too long gives only its length");
    mlocUnitFormatter formatter;
    Fill(&formatter, sizeof(formatter));
    CHECK(mlocInitUnitFormatter(&s_empty, &s_en, "meter", 5, nullptr, &formatter) ==
                  mloc_errorNotFound &&
              mlocInitUnitFormatter(&s_data, &s_en, "meter", 5, nullptr, &formatter) ==
                  mloc_errorNotFound &&
              Kept(&formatter, sizeof(formatter)),
          "a formatter refused leaves the formatter");
    mlocUnitFormatter forged;
    memset(&forged, 0, sizeof(forged));
    FillText(&text);
    CHECK(
        mlocUnitFormatter_Format(&forged, &one, &text.output) == mloc_errorInvalid &&
            mlocUnitFormatter_FormatRange(&forged, &one, &one, &text.output) == mloc_errorInvalid &&
            mlocGetUnitDisplayName(&s_data, &s_en, "meter", 5, mloc_unitWidthShort, &text.output) ==
                mloc_errorNotFound &&
            mlocGetUnitDisplayName(&s_data, &s_en, "meter", 5, (mlocUnitWidth)9, &text.output) ==
                mloc_errorInvalid &&
            KeptText(&text),
        "a format or name refused leaves the output");
    mlocGender gender;
    Fill(&gender, sizeof(gender));
    CHECK(mlocGetUnitGender(&s_data, &s_en, "meter", 5, &gender) == mloc_errorNotFound &&
              mlocGetUnitGender(&s_data, &s_en, "meter", 0, &gender) == mloc_errorInvalid &&
              Kept(&gender, sizeof(gender)),
          "a gender refused leaves the gender");
}

static void TestSpellout(void)
{
    mlocSpelloutFormatter formatter;
    Fill(&formatter, sizeof(formatter));
    CHECK(mlocInitSpelloutFormatter(&s_data, &s_en, nullptr, 0, nullptr, &formatter) ==
                  mloc_errorNotFound &&
              mlocInitNumberingSystemFormatter(&s_data, &s_en, "roman", 5, nullptr, &formatter) ==
                  mloc_errorNotFound &&
              Kept(&formatter, sizeof(formatter)),
          "a formatter refused leaves the formatter");
    const char* name;
    size_t length;
    Fill(&name, sizeof(name));
    Fill(&length, sizeof(length));
    CHECK(mlocGetSpelloutRuleSet(&s_data, &s_en, 0, &name, &length) == mloc_errorNotFound &&
              Kept(&name, sizeof(name)) && Kept(&length, sizeof(length)),
          "a rule set refused leaves the name and its length");
    mlocSpelloutFormatter forged;
    memset(&forged, 0, sizeof(forged));
    static mlocSpelloutWorkspace s_workspace;
    mlocDecimal decimal;
    size_t end;
    Fill(&decimal, sizeof(decimal));
    Fill(&end, sizeof(end));
    CHECK(mlocSpelloutFormatter_Parse(&forged, "one", 3, &s_workspace, &decimal, &end) ==
                  mloc_errorInvalid &&
              Kept(&decimal, sizeof(decimal)) && Kept(&end, sizeof(end)),
          "a parse refused leaves its outputs");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    s_empty = (mlocDataSet){nullptr, 0};
    CHECK(mlocParseLocale("en", 2, &s_en).status == mloc_success, "en parses");
    TestDecimal();
    TestPlural();
    TestNumber();
    TestCurrencyFormatter();
    TestCurrencyData();
    TestUnit();
    TestSpellout();
    return s_failures == 0 ? 0 : 1;
}
