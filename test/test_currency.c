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

#include <stdio.h>
#include <string.h>

#define NBSP "\xC2\xA0"

static mlocDataSet s_data;
static mlocDataSet s_withNames;

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
    CHECK(Plain("agq", "USD", "5", "5,00" NBSP "US$"),
          "a suffix symbol's first character is the one next to the digits");
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
    mlocCurrencyOptions engineering = {.number.notation = mloc_notationEngineering};
    CHECK(Formats("en", "USD", &engineering, "12345.678", "$12.35E3"), "engineering notation");
}

static void TestCompact(void)
{
    mlocCurrencyOptions s = {.number.notation = mloc_notationCompactShort};
    CHECK(Formats("en", "USD", &s, "1234", "$1.2K") && Formats("en", "USD", &s, "999", "$999"),
          "short compact, rounded by its own rule");
    CHECK(Formats("de", "EUR", &s, "1234567", "1,2" NBSP "Mio." NBSP "\xE2\x82\xAC") &&
              Formats("de", "EUR", &s, "1234", "1234" NBSP "\xE2\x82\xAC"),
          "German's currency table, which does not compact thousands");
    s.display = mloc_currencyCode;
    CHECK(Formats("en", "USD", &s, "1234", "USD" NBSP "1.2K"),
          "the alphaNextToNumber table for a code");
    s.display = mloc_currencyNarrowSymbol;
    CHECK(Formats("en", "CHF", &s, "1234", "Fr." NBSP "1.2K"), "and for a full stop");
    s.display = mloc_currencySymbol;
    CHECK(Formats("bn", "USD", &s, "1234",
                  "\xE0\xA7\xA7.\xE0\xA7\xA8" NBSP "\xE0\xA6\xB9\xE0\xA6\xBE"
                  "US$"),
          "a currency away from the zeros keeps its pattern (Bengali's 0 হা¤)");
    CHECK(Formats("he", "USD", &s, "1000",
                  "\xE2\x80\x8F"
                  "1" NBSP "\xD7\x90\xD7\xB3\xE2\x80\x8F" NBSP "\xE2\x80\x8F$") &&
              Formats("he", "USD", &s, "2000",
                      "\xE2\x80\x8F"
                      "2K\xE2\x80\x8F" NBSP "\xE2\x80\x8F$"),
          "the plural category picks the pattern (Hebrew's one thousand)");
    mlocCurrencyFormatter formatter;
    CHECK(Init(&formatter, "en-u-nu-roman", "USD", &s) == mloc_fellBack,
          "a system without digits in compact notation");
    CHECK(Formats("ja", "JPY", &s, "12345678",
                  "\xEF\xBF\xA5"
                  "1235\xE4\xB8\x87"),
          "Japanese's man");
    mlocCurrencyOptions l = {.number.notation = mloc_notationCompactLong};
    CHECK(Formats("en", "USD", &l, "1234567", "$1.2M"),
          "long compact takes the short currency table, as CLDR has no long one");
    l.display = mloc_currencyHidden;
    CHECK(Formats("en", "USD", &l, "1234", "1.2K") &&
              Formats("af", "USD", &l, "1234", "1,2" NBSP "k"),
          "hidden, the short decimal table");
}

// Whether an amount formats as expected with the names data.
static bool Named(const char* id, const char* code, const mlocCurrencyOptions* options,
                  const char* amount, const char* expected)
{
    mlocDataSet keep = s_data;
    s_data = s_withNames;
    bool ok = Formats(id, code, options, amount, expected);
    s_data = keep;
    return ok;
}

static void TestNames(void)
{
    mlocCurrencyOptions name = {.display = mloc_currencyName};
    CHECK(Named("en", "USD", &name, "1", "1.00 US dollars") &&
              Named("en", "JPY", &name, "1", "1 Japanese yen"),
          "the category of the digits shown picks the name");
    CHECK(Named("ru", "JPY", &name, "1",
                "1 \xD1\x8F\xD0\xBF\xD0\xBE\xD0\xBD\xD1\x81\xD0\xBA\xD0\xB0\xD1\x8F "
                "\xD0\xB8\xD0\xB5\xD0\xBD\xD0\xB0") &&
              Named("ru", "JPY", &name, "5",
                    "5 \xD1\x8F\xD0\xBF\xD0\xBE\xD0\xBD\xD1\x81\xD0\xBA\xD0\xB8\xD1\x85 "
                    "\xD0\xB8\xD0\xB5\xD0\xBD"),
          "Russian's one and many");
    CHECK(Named("fr", "EUR", &name, "1234.5",
                "1\xE2\x80\xAF"
                "234,50 euros"),
          "the decimal pattern, not the currency one");
    CHECK(Named("en", "ZZZ", &name, "5", "5.00 ZZZ"), "a currency without a name: its code");
    mlocCurrencyFormatter formatter;
    CHECK(Init(&formatter, "en", "USD", &name) == mloc_errorNotFound,
          "names come from the names data file alone");
    mlocCurrencyOptions longCompact = {.display = mloc_currencyName,
                                       .number.notation = mloc_notationCompactLong};
    CHECK(Named("en", "USD", &longCompact, "1234567", "1.2 million US dollars"),
          "compact long with a name: the decimal compact table");
    mlocCurrencyOptions append = {.display = mloc_currencyNarrowSymbol, .appendCode = 1};
    CHECK(Formats("en", "USD", &append, "1432", "$1,432.00" NBSP "USD"), "the ISO append");
    mlocCurrencyOptions badAppend = {.display = mloc_currencyCode, .appendCode = 1};
    mlocCurrencyOptions twoAppend = {.appendCode = 2};
    CHECK(Init(&formatter, "en", "USD", &badAppend) == mloc_errorInvalid &&
              Init(&formatter, "en", "USD", &twoAppend) == mloc_errorInvalid,
          "the append with a code, and a value not 0 or 1");
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

// Whether a range formats as expected with the names data.
static bool NamedRanges(const char* id, const char* code, mlocCurrencyDisplay display,
                        const char* from, const char* to, const char* expected)
{
    mlocDataSet keep = s_data;
    s_data = s_withNames;
    bool ok = Ranges(id, code, display, from, to, expected);
    s_data = keep;
    return ok;
}

static void TestNameRanges(void)
{
    CHECK(NamedRanges("en", "USD", mloc_currencyName, "1", "5",
                      "1.00\xE2\x80\x93"
                      "5.00 US dollars"),
          "a name is written once");
    CHECK(NamedRanges("en", "USD", mloc_currencyName, "1", "1", "~1.00 US dollars"),
          "the approximately sign within the name");
    CHECK(NamedRanges("ru", "JPY", mloc_currencyName, "1", "5",
                      "1\xE2\x80\x93"
                      "5 \xD1\x8F\xD0\xBF\xD0\xBE\xD0\xBD\xD1\x81\xD0\xBA\xD0\xB8\xD1\x85 "
                      "\xD0\xB8\xD0\xB5\xD0\xBD"),
          "in the category the plural ranges give (Russian one to many: many)");
}

static void TestLookups(void)
{
    mlocLocale en;
    mlocLocale ja;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocParseLocale("ja", 2, &ja).status == mloc_success,
          "locales");
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocGetCurrencySymbol(&s_data, &en, "USD", 3, mloc_currencySymbol, &output) ==
                  mloc_success &&
              output.length == 1 && buffer[0] == '$',
          "a symbol");
    CHECK(mlocGetCurrencySymbol(&s_data, &en, "RUB", 3, mloc_currencyNarrowSymbol, &output) ==
                  mloc_success &&
              output.length == 3 && memcmp(buffer, "\xE2\x82\xBD", 3) == 0,
          "a narrow symbol");
    CHECK(mlocGetCurrencySymbol(&s_data, &en, "ZZZ", 3, mloc_currencySymbol, &output) ==
                  mloc_fellBack &&
              output.length == 3 && memcmp(buffer, "ZZZ", 3) == 0,
          "no symbol: the code, falling back");
    CHECK(mlocGetCurrencySymbol(&s_data, &en, "usd", 3, mloc_currencyCode, &output) ==
                  mloc_success &&
              memcmp(buffer, "USD", 3) == 0,
          "the code as asked");
    CHECK(mlocGetCurrencySymbol(&s_data, &en, "USD", 3, mloc_currencyName, &output) ==
              mloc_errorInvalid,
          "a name is no symbol");
    CHECK(mlocGetCurrencyName(&s_withNames, &en, "USD", 3, &output) == mloc_success &&
              output.length == 9 && memcmp(buffer, "US Dollar", 9) == 0,
          "a name");
    CHECK(mlocGetCurrencyName(&s_withNames, &ja, "ZZZ", 3, &output) == mloc_fellBack &&
              memcmp(buffer, "ZZZ", 3) == 0,
          "no name: the code, falling back");
    CHECK(mlocGetCurrencyName(&s_data, &en, "USD", 3, &output) == mloc_errorNotFound,
          "no names data file");
    mlocOutput small = {.buffer = buffer, .capacity = 4};
    CHECK(mlocGetCurrencyName(&s_withNames, &en, "USD", 3, &small) == mloc_errorCapacity &&
              small.length == 9,
          "a name past the room, and the room it needs");
    uint8_t digits = 9;
    uint16_t increment = 0;
    CHECK(mlocGetCurrencyDigits(&s_data, "CHF", 3, mloc_currencyUsageCash, &digits, &increment) ==
                  mloc_success &&
              digits == 2 && increment == 5,
          "Swiss cash");
    CHECK(mlocGetCurrencyDigits(&s_data, "JPY", 3, mloc_currencyUsageStandard, &digits,
                                &increment) == mloc_success &&
              digits == 0 && increment == 1,
          "yen");
    CHECK(mlocGetCurrencyDigits(&s_data, "US", 2, mloc_currencyUsageStandard, &digits,
                                &increment) == mloc_errorInvalid &&
              mlocGetCurrencyDigits(&s_data, "USD", 3, (mlocCurrencyUsage)2, &digits, &increment) ==
                  mloc_errorInvalid &&
              mlocGetCurrencyName(&s_withNames, &en, "U$D", 3, &output) == mloc_errorInvalid,
          "refusals");
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
    mlocCurrencyOptions compactPercent = {
        .number = {.notation = mloc_notationCompactShort, .style = mloc_numberPercent}};
    CHECK(Init(&formatter, "en", "USD", &compactPercent) == mloc_errorInvalid,
          "compact notation with a percent style");
    mlocCurrencyOptions display = {.display = (mlocCurrencyDisplay)7};
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
    memset(&mixed, 0, sizeof(mixed));
    memcpy(&mixed, &number, sizeof(number));
    CHECK(mlocFormatCurrency(&mixed, &decimal, &output) == mloc_errorInvalid,
          "a number formatter as a currency formatter");
    CHECK(mlocFormatCurrency(nullptr, &decimal, &output) == mloc_errorInvalid, "a null formatter");
}

// The compiled-in data, and with it the currency names data file, at
// the path the build gives.
static bool OpenData(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_names[4 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 1};
    FILE* file = fopen(MLOC_NAMES_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_withNames = (mlocDataSet){s_sources, 2};
    return size > 0 && size < sizeof(s_names) &&
           mlocOpenData(&s_sources[1], s_names, size) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_currency: cannot open " MLOC_NAMES_FILE "\n", stderr);
        return 2;
    }
    TestDisplays();
    TestSpacing();
    TestDigits();
    TestPatterns();
    TestCompact();
    TestNames();
    TestRanges();
    TestNameRanges();
    TestLookups();
    TestParts();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
