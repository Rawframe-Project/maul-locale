// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency formatting on the compiled-in data (record mloc-0022): the
// displays and their fallbacks, the spacing of symbols, the currencies'
// digits and cash rounding, accounting, a currency's own pattern and
// separators, ranges, the parts and the refusals. Every expected text is
// ICU4C 79's for the same input (its hidden display aside, which keeps
// the standard pattern without the symbol where we take CLDR's
// noCurrency pattern).

#include "test_harness.h"

#include "maul-locale/currency.h"

#include <string.h>

#define NBSP "\xC2\xA0"

static mlocDataSet s_data;

static mlocResult Init(mlocCurrencyFormatter* formatter, const char* id, const char* code,
                       const mlocCurrencyOptions* options)
{
    mlocLocale locale;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success)
    {
        return (mlocResult)-100;
    }
    return mlocInitCurrencyFormatter(formatter, &s_data, &locale, code, strlen(code), options);
}

// Whether an amount formats as expected in a locale with options.
static bool Formats(const char* id, const char* code, const mlocCurrencyOptions* options,
                    const char* amount, const char* expected)
{
    mlocCurrencyFormatter formatter;
    mlocDecimal decimal;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return Init(&formatter, id, code, options) >= 0 &&
           mlocParseDecimal(amount, strlen(amount), &decimal).status == mloc_success &&
           mlocFormatCurrency(&formatter, &decimal, &output) == mloc_success &&
           output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
}

static bool Shows(const char* id, const char* code, mlocCurrencyDisplay display, const char* amount,
                  const char* expected)
{
    mlocCurrencyOptions options = {.display = display};
    return Formats(id, code, &options, amount, expected);
}

static bool Plain(const char* id, const char* code, const char* amount, const char* expected)
{
    return Formats(id, code, nullptr, amount, expected);
}

static void TestDisplays(void)
{
    CHECK(Plain("en", "USD", "1234.5", "$1,234.50"), "the symbol and the currency's digits");
    CHECK(Shows("en", "USD", mloc_currencyCode, "1234.5", "USD" NBSP "1,234.50"),
          "the code, spaced by the alphaNextToNumber pattern");
    CHECK(Shows("en", "usd", mloc_currencyNarrowSymbol, "1234.5", "$1,234.50"),
          "the narrow symbol; a code in any case");
    CHECK(Shows("en", "USD", mloc_currencyHidden, "-1234.5", "-1,234.50"),
          "hidden: the noCurrency pattern");
    CHECK(Shows("de", "EUR", mloc_currencyHidden, "1234.5", "1.234,50"),
          "hidden, without the space the symbol had");
    CHECK(Shows("zh-Hant", "TWD", mloc_currencyFormalSymbol, "5", "NT$5.00") &&
              Plain("zh-Hant", "TWD", "5", "$5.00"),
          "a formal symbol");
    CHECK(Shows("ar-AE", "AED", mloc_currencyVariantSymbol, "5",
                "\xE2\x80\x8F"
                "5.00" NBSP "\xE2\x83\x83"),
          "a variant symbol");
    CHECK(Shows("en", "USD", mloc_currencyFormalSymbol, "5", "$5.00") &&
              Shows("en", "USD", mloc_currencyVariantSymbol, "5", "$5.00"),
          "a formal or variant symbol the locale lacks is the symbol");
    CHECK(Plain("en", "RUB", "5", "RUB" NBSP "5.00") &&
              Shows("en", "RUB", mloc_currencyNarrowSymbol, "5",
                    "\xE2\x82\xBD"
                    "5.00"),
          "a narrow symbol without a symbol; a symbol the chain lacks is the code");
    CHECK(Plain("en", "ZZZ", "3", "ZZZ" NBSP "3.00"), "a code the data does not know");
}

static void TestSpacing(void)
{
    CHECK(Plain("en", "CHF", "5", "CHF" NBSP "5.00"), "a letter next to the digits");
    CHECK(Shows("en", "CHF", mloc_currencyNarrowSymbol, "5", "Fr." NBSP "5.00"),
          "a full stop next to the digits, spaced as currencySpacing says");
    CHECK(Plain("de", "EUR", "1234.5", "1.234,50" NBSP "\xE2\x82\xAC"), "a suffix symbol");
    CHECK(Plain("fr", "EUR", "-1234.5",
                "-1\xE2\x80\xAF"
                "234,50" NBSP "\xE2\x82\xAC"),
          "French");
    CHECK(Shows("en-IN", "USD", mloc_currencyCode, "1234567.891", "USD" NBSP "12,34,567.89"),
          "the alphaNextToNumber pattern keeps the standard one's grouping");
    CHECK(Shows("en-IN", "USD", mloc_currencyHidden, "1234567.891", "12,34,567.89"),
          "and so does the noCurrency pattern");
}

static void TestDigits(void)
{
    CHECK(Plain("en", "JPY", "1234.5",
                "\xC2\xA5"
                "1,235"),
          "no fraction digits for yen");
    CHECK(Plain("ja", "JPY", "1234.5",
                "\xEF\xBF\xA5"
                "1,235"),
          "Japanese's own yen sign");
    CHECK(Plain("en", "BHD", "1.2345", "BHD" NBSP "1.235"), "three for the Bahraini dinar");
    mlocCurrencyOptions cash = {.usage = mloc_currencyUsageCash};
    CHECK(Formats("en", "CAD", &cash, "1.03", "CA$1.05"), "Canadian cash to 0.05");
    CHECK(Formats("de-CH", "CHF", &cash, "1234.567", "CHF" NBSP "1'234.55"), "Swiss cash to 0.05");
    CHECK(Formats("en", "SEK", &cash, "1.5", "SEK" NBSP "2"), "Swedish cash in whole units");
    mlocCurrencyOptions exact = {
        .number.precision = {.kind = mloc_precisionFraction, .maxFractionDigits = 4}};
    CHECK(Formats("en", "USD", &exact, "1.23456", "$1.2346") &&
              Formats("en", "USD", &exact, "1.5", "$1.5"),
          "a precision of the options replaces the currency's");
    mlocCurrencyOptions halfEven = {.number.precision.mode = mloc_roundHalfEven};
    CHECK(Formats("en", "JPY", &halfEven, "1234.5",
                  "\xC2\xA5"
                  "1,234"),
          "the currency's digits with the options' rounding mode");
}

static void TestPatterns(void)
{
    CHECK(Plain("en-u-cf-account", "USD", "-5", "($5.00)") &&
              Plain("en-u-cf-account", "USD", "5", "$5.00"),
          "accounting from the locale's -u-cf keyword");
    mlocCurrencyOptions always = {.number.signDisplay = mloc_signAlways};
    CHECK(Formats("en-u-cf-account", "USD", &always, "5", "+$5.00") &&
              Formats("en-u-cf-account", "USD", &always, "-5", "($5.00)"),
          "accounting's parentheses are the minus sign");
    CHECK(Plain("de-CH", "CHF", "-1234.567", "CHF-1'234.57"),
          "Swiss German puts the minus after the symbol");
    CHECK(Plain("nl", "EUR", "-5", "\xE2\x82\xAC" NBSP "-5,00"), "Dutch");
    CHECK(Plain("pt-PT", "PTE", "1234.5", "1234$50" NBSP "\xE2\x80\x8B"),
          "the escudo's own separators: the cifrão as its decimal separator");
    CHECK(Plain("ca", "ESP", "1234", "\xE2\x82\xA7" NBSP "1.234"), "the peseta's own pattern");
    CHECK(Plain("de-AT", "EUR", "1234567.5", "\xE2\x82\xAC" NBSP "1.234.567,50"),
          "Austria's currency group separator");
    CHECK(Plain("hi", "INR", "1234567.891",
                "\xE2\x82\xB9"
                "12,34,567.89"),
          "Indian grouping");
    mlocCurrencyOptions scientific = {.number.notation = mloc_notationScientific};
    CHECK(Formats("en", "USD", &scientific, "1234.5", "$1.23E3"), "scientific notation");
}

// Whether a range formats as expected in a locale with options.
static bool Ranges(const char* id, const char* code, mlocCurrencyDisplay display, const char* from,
                   const char* to, const char* expected)
{
    mlocCurrencyFormatter formatter;
    mlocCurrencyOptions options = {.display = display};
    mlocDecimal start;
    mlocDecimal end;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return Init(&formatter, id, code, &options) >= 0 &&
           mlocParseDecimal(from, strlen(from), &start).status == mloc_success &&
           mlocParseDecimal(to, strlen(to), &end).status == mloc_success &&
           mlocFormatCurrencyRange(&formatter, &start, &end, &output) == mloc_success &&
           output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
}

static void TestRanges(void)
{
    CHECK(Ranges("en", "USD", mloc_currencySymbol, "3", "5", "$3.00 \xE2\x80\x93 $5.00"),
          "a one-character symbol is not shared");
    CHECK(Ranges("en", "USD", mloc_currencySymbol, "5", "5", "~$5.00"), "ends that write alike");
    CHECK(Ranges("en", "EUR", mloc_currencyCode, "3", "5",
                 "EUR" NBSP "3.00\xE2\x80\x93"
                 "5.00"),
          "a code is shared");
    CHECK(Ranges("de", "EUR", mloc_currencySymbol, "3", "5",
                 "3,00\xE2\x80\x93"
                 "5,00" NBSP "\xE2\x82\xAC"),
          "a symbol with its space is shared");
}

typedef struct Parts
{
    uint32_t kinds[16];
    size_t starts[16];
    size_t lengths[16];
    int count;
} Parts;

static void Collect(void* context, uint32_t kind, size_t start, size_t length)
{
    Parts* p = context;
    if (p->count < 16)
    {
        p->kinds[p->count] = kind;
        p->starts[p->count] = start;
        p->lengths[p->count] = length;
        p->count++;
    }
}

static void TestParts(void)
{
    mlocCurrencyFormatter formatter;
    mlocDecimal decimal;
    char buffer[64];
    Parts parts = {0};
    mlocOutput output = {
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &parts};
    CHECK(Init(&formatter, "en", "USD", nullptr) == mloc_success &&
              mlocParseDecimal("-1.5", 4, &decimal).status == mloc_success &&
              mlocFormatCurrency(&formatter, &decimal, &output) == mloc_success,
          "a formatter of the parts");
    // -$1.50
    CHECK(parts.count == 5 && parts.kinds[0] == mloc_numberPartMinusSign &&
              parts.kinds[1] == mloc_numberPartCurrency && parts.starts[1] == 1 &&
              parts.lengths[1] == 1 && parts.kinds[2] == mloc_numberPartInteger &&
              parts.kinds[3] == mloc_numberPartDecimal && parts.kinds[4] == mloc_numberPartFraction,
          "the currency is its own part");
    mlocCurrencyOptions code = {.display = mloc_currencyCode};
    Parts codeParts = {0};
    output = (mlocOutput){
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &codeParts};
    CHECK(Init(&formatter, "en", "EUR", &code) == mloc_success &&
              mlocFormatCurrency(&formatter, &decimal, &output) == mloc_success &&
              codeParts.kinds[1] == mloc_numberPartCurrency && codeParts.lengths[1] == 3 &&
              memcmp(buffer + codeParts.starts[1], "EUR", 3) == 0,
          "a code as the currency part");
}

static void TestRefusals(void)
{
    mlocCurrencyFormatter formatter;
    mlocLocale locale;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocInitCurrencyFormatter(&formatter, &s_data, &locale, "US", 2, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitCurrencyFormatter(&formatter, &s_data, &locale, "U$D", 3, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitCurrencyFormatter(&formatter, &s_data, &locale, nullptr, 3, nullptr) ==
                  mloc_errorInvalid,
          "a code that is not three letters");
    mlocCurrencyOptions percent = {.number.style = mloc_numberPercent};
    mlocCurrencyOptions display = {.display = (mlocCurrencyDisplay)6};
    mlocCurrencyOptions usage = {.usage = (mlocCurrencyUsage)2};
    CHECK(Init(&formatter, "en", "USD", &percent) == mloc_errorInvalid &&
              Init(&formatter, "en", "USD", &display) == mloc_errorInvalid &&
              Init(&formatter, "en", "USD", &usage) == mloc_errorInvalid,
          "options out of range");
    CHECK(mlocInitCurrencyFormatter(nullptr, &s_data, &locale, "USD", 3, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitCurrencyFormatter(&formatter, nullptr, &locale, "USD", 3, nullptr) ==
                  mloc_errorInvalid,
          "null pointers");
    // A number formatter is not a currency formatter.
    mlocNumberFormatter number;
    mlocCurrencyFormatter mixed;
    mlocDecimal decimal;
    char buffer[32];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocInitNumberFormatter(&number, &s_data, &locale, nullptr) == mloc_success &&
              mlocParseDecimal("1", 1, &decimal).status == mloc_success,
          "a number formatter");
    static_assert(sizeof(number) == sizeof(mixed), "the opaque values agree in size");
    memcpy(&mixed, &number, sizeof(mixed));
    CHECK(mlocFormatCurrency(&mixed, &decimal, &output) == mloc_errorInvalid,
          "a number formatter as a currency formatter");
    CHECK(mlocFormatCurrency(nullptr, &decimal, &output) == mloc_errorInvalid, "a null formatter");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestDisplays();
    TestSpacing();
    TestDigits();
    TestPatterns();
    TestRanges();
    TestParts();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
