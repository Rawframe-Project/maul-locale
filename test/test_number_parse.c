// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number parsing on the compiled-in data (record mloc-0032): cases whose
// results ICU4C 79's DecimalFormat::parse gives alike (the oracle in the
// research notes compares over 800,000 more), the round trip of formatted
// values in both modes, the bounds, and the refusals.

#include "test_harness.h"

#include "maul-locale/decimal.h"
#include "maul-locale/number.h"

#include <stdlib.h>
#include <string.h>

static mlocDataSet s_data;

typedef enum Style
{
    styleDecimal,
    stylePercent,
    stylePerMille,
    styleScientific,
    stylePlus,
} Style;

static mlocNumberOptions OptionsOf(Style style)
{
    mlocNumberOptions o = {0};
    o.style = style == stylePercent    ? mloc_numberPercent
              : style == stylePerMille ? mloc_numberPerMille
                                       : mloc_numberDecimal;
    o.notation = style == styleScientific ? mloc_notationScientific : mloc_notationStandard;
    o.signDisplay = style == stylePlus ? mloc_signAlways : mloc_signAuto;
    return o;
}

static bool Formatter(const char* id, Style style, mlocNumberFormatter* formatterOut)
{
    mlocLocale locale;
    mlocNumberOptions o = OptionsOf(style);
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitNumberFormatter(formatterOut, &s_data, &locale, &o) >= 0;
}

typedef struct Case
{
    const char* id;
    Style style;
    mlocParseMode mode;
    const char* text;
    // The decimal read, as mlocFormatDecimal writes it; null for none.
    const char* expected;
    size_t end;
} Case;

// Cases whose value and end ICU gives alike (its value without the
// fraction digits typed, which the library keeps).
static const Case s_cases[] = {
    {"en", styleDecimal, mloc_parseLenient, "1,234.56", "1234.56", 8},
    {"en", styleDecimal, mloc_parseLenient, "12 apples", "12", 2},
    {"en", styleDecimal, mloc_parseLenient,
     " \xE2\x80\x8E"
     "12",
     "12", 6},
    {"en", styleDecimal, mloc_parseLenient, "1,1", "1", 1},
    {"en", styleDecimal, mloc_parseLenient, "1,234,", "1234", 5},
    {"en", styleDecimal, mloc_parseLenient,
     "1\xEF\xBC\x8C"
     "234",
     "1234", 7},
    {"en", styleDecimal, mloc_parseLenient, "1.234.567", "1.234", 5},
    {"en", styleDecimal, mloc_parseLenient, "-1.50", "-1.50", 5},
    {"en", styleDecimal, mloc_parseLenient,
     "\xE2\x88\x92"
     "5",
     "-5", 4},
    {"en", styleDecimal, mloc_parseLenient, "5-", "5", 1},
    {"en", styleDecimal, mloc_parseLenient, "(5)", nullptr, 0},
    {"en", styleDecimal, mloc_parseLenient, "+5", "5", 2},
    {"en", styleDecimal, mloc_parseLenient, "1,2345", "12345", 6},
    {"en", styleDecimal, mloc_parseLenient, "\xD9\xA1\xD9\xA2\xD9\xA3", "123", 6},
    {"en", styleDecimal, mloc_parseLenient, "1e3", "1000", 3},
    {"en", styleDecimal, mloc_parseLenient, "1E-3x", "0.001", 4},
    {"en", styleDecimal, mloc_parseLenient, "\xE2\x88\x9E", "Infinity", 3},
    {"en", styleDecimal, mloc_parseLenient, "-\xE2\x88\x9E", "-Infinity", 4},
    {"en", styleDecimal, mloc_parseLenient, "nan", "NaN", 3},
    {"en", styleDecimal, mloc_parseLenient, "-0", "-0", 2},
    {"en", styleDecimal, mloc_parseLenient, "-0.00", "-0.00", 5},
    {"en", styleDecimal, mloc_parseLenient, ",5", "5", 2},
    {"en", styleDecimal, mloc_parseLenient, ".5", "0.5", 2},
    {"en", styleDecimal, mloc_parseLenient, "abc", nullptr, 0},
    {"en", styleDecimal, mloc_parseLenient, "", nullptr, 0},
    {"en", styleDecimal, mloc_parseLenient, "   ", nullptr, 0},
    {"en", styleDecimal, mloc_parseLenient, "-", nullptr, 1},
    {"en", styleDecimal, mloc_parseLenient, "0001.2300", "1.2300", 9},
    {"en", styleDecimal, mloc_parseStrict, "1,234.56", "1234.56", 8},
    {"en", styleDecimal, mloc_parseStrict, "1,23,4", nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict, "1234", "1234", 4},
    {"en", styleDecimal, mloc_parseStrict, "12,34", nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict, " 12", nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict,
     "\xE2\x80\x8E"
     "12",
     "12", 5},
    {"en", styleDecimal, mloc_parseStrict, "-12", "-12", 3},
    {"en", styleDecimal, mloc_parseStrict, "+12", nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict,
     "1\xEF\xBC\x8C"
     "234",
     "1234", 7},
    {"en", styleDecimal, mloc_parseStrict, "1,234,", "1234", 5},
    {"en", stylePercent, mloc_parseLenient, "50%", "0.50", 3},
    {"en", stylePercent, mloc_parseLenient, "50", "0.50", 2},
    {"en", stylePercent, mloc_parseLenient, "-12.5%", "-0.125", 6},
    {"en", stylePercent, mloc_parseLenient, "50\xE2\x80\xB0", "0.50", 2},
    {"en", stylePercent, mloc_parseStrict, "50%", "0.50", 3},
    {"en", stylePercent, mloc_parseStrict, "50", nullptr, 2},
    {"en", stylePerMille, mloc_parseLenient, "50\xE2\x80\xB0", "0.050", 5},
    {"en", stylePerMille, mloc_parseLenient, "50%", "0.050", 2},
    {"en", stylePerMille, mloc_parseStrict, "50\xE2\x80\xB0", "0.050", 5},
    {"en", styleScientific, mloc_parseLenient, "1.5E3", "1500", 5},
    {"en", styleScientific, mloc_parseLenient, "1.5E-3", "0.0015", 6},
    {"en", styleScientific, mloc_parseLenient, "1.5E", "1.5", 3},
    {"en", styleScientific, mloc_parseLenient, "1.5E+", "1.5", 3},
    {"en", stylePlus, mloc_parseStrict, "+12", "12", 3},
    {"en", stylePlus, mloc_parseStrict, "-12", "-12", 3},
    {"en", stylePlus, mloc_parseStrict, "12", nullptr, 2},
    {"de", styleDecimal, mloc_parseLenient, "1.234,5", "1234.5", 7},
    {"de", styleDecimal, mloc_parseLenient, "1,5", "1.5", 3},
    {"de", styleDecimal, mloc_parseLenient, "1.5", "1", 1},
    {"de", styleDecimal, mloc_parseStrict, "1.234,5", "1234.5", 7},
    {"fr", styleDecimal, mloc_parseLenient, "1 234,5", "1234.5", 7},
    {"fr", styleDecimal, mloc_parseLenient, "1 234,5", "1234.5", 7},
    {"fr", styleDecimal, mloc_parseLenient, "1.234,5", "1234.5", 7},
    {"fr", styleDecimal, mloc_parseStrict, "1 234,5", "1234.5", 7},
    {"fr", styleDecimal, mloc_parseStrict, "1.234", "1", 1},
    {"de-CH", styleDecimal, mloc_parseLenient,
     "1\xE2\x80\x99"
     "234.5",
     "1234.5", 9},
    {"de-CH", styleDecimal, mloc_parseLenient, "1'234.5", "1234.5", 7},
    {"hi", styleDecimal, mloc_parseLenient, "12,34,567", "1234567", 9},
    {"hi", styleDecimal, mloc_parseStrict, "12,34,567", "1234567", 9},
    {"hi", styleDecimal, mloc_parseStrict, "1,234,567", nullptr, 0},
    {"ar-EG", styleDecimal, mloc_parseLenient,
     "\xD8\x9C-\xD9\xA1\xD9\xAC\xD9\xA2\xD9\xA3\xD9\xA0\xD9\xAB\xD9\xA0\xD9\xA5", "-1230.05", 19},
    {"ar-EG", styleDecimal, mloc_parseLenient, "-1,230.05", "-1.230", 6},
    {"ar-EG", styleDecimal, mloc_parseStrict,
     "\xD8\x9C-\xD9\xA1\xD9\xAC\xD9\xA2\xD9\xA3\xD9\xA0\xD9\xAB\xD9\xA0\xD9\xA5", "-1230.05", 19},
    {"ar", stylePercent, mloc_parseLenient,
     "\xE2\x80\x8E"
     "50\xE2\x80\x8E%\xE2\x80\x8E",
     "0.50", 12},
    {"en-u-nu-thai", styleDecimal, mloc_parseLenient,
     "\xE0\xB9\x91,\xE0\xB9\x92\xE0\xB9\x93\xE0\xB9\x94", "1234", 13},
    {"sv", styleDecimal, mloc_parseLenient,
     "\xE2\x88\x92"
     "5",
     "-5", 4},
    {"sv", styleDecimal, mloc_parseLenient, "-5", "-5", 2},
    {"sv", styleDecimal, mloc_parseStrict,
     "\xE2\x88\x92"
     "5",
     "-5", 4},
    {"sv", styleDecimal, mloc_parseStrict, "-5", "-5", 2},
    {"fa", styleDecimal, mloc_parseLenient, "\xDB\xB1\xDB\xB2\xD9\xAB\xDB\xB5", "12.5", 8},
    {"fa", styleDecimal, mloc_parseLenient, "\xE2\x80\x8E\xE2\x88\x92\xDB\xB1\xDB\xB2", "-12", 10},
    {"he", styleDecimal, mloc_parseLenient, "\xE2\x80\x8E-12", "-12", 6},
    {"en", styleDecimal, mloc_parseLenient, "1,234.5.6", "1234.5", 7},
    {"en", styleDecimal, mloc_parseLenient, "1.2,3", "1.2", 3},
    {"en", styleDecimal, mloc_parseLenient, "1,,234", "1234", 6},
    {"en", styleDecimal, mloc_parseStrict, "1,,234", "1", 1},
    // ICU's lead set (the strict separators in lenient mode), separators
    // after the decimal one, the exponent's own signs (bidi controls
    // first), digits that are not decimal digits (hanidec).
    {"en", styleDecimal, mloc_parseLenient,
     "\xE3\x80\x82"
     "5",
     nullptr, 0},
    {"en", styleDecimal, mloc_parseLenient,
     "\xE3\x80\x81"
     "5",
     nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict,
     "\xE3\x80\x82"
     "5",
     nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict,
     "\xEF\xBC\x8E"
     "5",
     "0.5", 4},
    {"en", styleDecimal, mloc_parseLenient,
     "\xEF\xBC\x8E"
     "5",
     "0.5", 4},
    {"en", styleDecimal, mloc_parseStrict, "50%", "50", 2},
    {"en", styleScientific, mloc_parseLenient, "1E3.5", "1000", 3},
    {"en", styleDecimal, mloc_parseLenient, "1,234.5,6", "1234.5", 7},
    {"en", styleDecimal, mloc_parseLenient, "1,2,345", "1", 1},
    {"en", styleDecimal, mloc_parseStrict, "1,2,345", nullptr, 0},
    {"he", styleScientific, mloc_parseLenient, "1E\xE2\x80\x8E\xE2\x80\x8E-3", "0.001", 10},
    {"he", styleScientific, mloc_parseLenient, "1E\xE2\x80\x8E\xE2\x80\x8E+3", "1", 1},
    {"he", styleScientific, mloc_parseLenient,
     "1E\xE2\x80\x8E\xE2\x80\x8E"
     "5",
     "1", 1},
    {"he", styleScientific, mloc_parseLenient, "1E\xE2\x80\x8E-3", "0.001", 7},
    {"zh-u-nu-hanidec", styleDecimal, mloc_parseLenient, "\xE4\xB8\x80\xE4\xBA\x8C\xE4\xB8\x89",
     "123", 9},
    {"zh-u-nu-hanidec", styleDecimal, mloc_parseLenient, "\xE3\x80\x87.\xE4\xBA\x94", "0.5", 7},
    {"zh-u-nu-hanidec", styleDecimal, mloc_parseStrict,
     "\xE4\xB8\x80,\xE4\xBA\x8C\xE4\xB8\x89\xE5\x9B\x9B", "1234", 13},
    {"en", styleDecimal, mloc_parseLenient, "1,23.4", "123.4", 6},
    {"en", styleDecimal, mloc_parseStrict, "1,23.4", nullptr, 0},
    {"en", styleDecimal, mloc_parseStrict, "1,234.5,6", "1234.5", 7},
    // ICU's comparisons fold case, the Turkic way for I.
    {"ru", styleDecimal, mloc_parseLenient,
     "\xD0\x9D\xD0\x95\xC2\xA0\xD0\xA7\xD0\x98\xD0\xA1\xD0\x9B\xD0\x9E", "NaN", 16},
    {"uk", styleScientific, mloc_parseLenient,
     "1,5\xD0\xB5"
     "3",
     "1500", 6},
    {"fi", styleDecimal, mloc_parseStrict, "EP\xC3\x84LUKU", "NaN", 8},
    {"hy", styleDecimal, mloc_parseLenient, "\xD5\xB8\xD5\xB9\xD5\xA9", "NaN", 6},
};

// Parses text from a copy of its exact length, so that the sanitizer
// builds catch a read past it.
static mlocResult ParseCopy(const mlocNumberFormatter* formatter, const char* text, size_t length,
                            mlocParseMode mode, mlocDecimal* decimal, size_t* end)
{
    char* copy = malloc(length == 0 ? 1 : length);
    if (copy == nullptr)
    {
        return (mlocResult)-100;
    }
    memcpy(copy, text, length);
    mlocResult r = mlocParseNumber(formatter, copy, length, mode, decimal, end);
    free(copy);
    return r;
}

static bool Parses(const Case* c)
{
    mlocNumberFormatter formatter;
    mlocDecimal decimal;
    size_t end = 0;
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    if (!Formatter(c->id, c->style, &formatter))
    {
        return false;
    }
    mlocResult r = ParseCopy(&formatter, c->text, strlen(c->text), c->mode, &decimal, &end);
    if (c->expected == nullptr)
    {
        return r == mloc_errorSyntax && end == c->end;
    }
    return r == mloc_success && end == c->end &&
           mlocFormatDecimal(&decimal, &output) == mloc_success &&
           output.length == strlen(c->expected) && memcmp(buffer, c->expected, output.length) == 0;
}

static void TestCases(void)
{
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        if (!Parses(&s_cases[i]))
        {
            CHECK(false, s_cases[i].text);
        }
    }
}

// Formats a value and reads the text back, all of it, in both modes, to
// the same decimal, fraction digits too.
static bool RoundTrips(const char* id, Style style, const char* value)
{
    mlocNumberFormatter formatter;
    mlocDecimal decimal;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    if (!Formatter(id, style, &formatter) ||
        mlocParseDecimal(value, strlen(value), &decimal).status != mloc_success ||
        mlocFormatNumber(&formatter, &decimal, &output) != mloc_success)
    {
        return false;
    }
    for (int mode = mloc_parseLenient; mode <= mloc_parseStrict; mode++)
    {
        mlocDecimal back;
        size_t end = 0;
        if (ParseCopy(&formatter, buffer, output.length, (mlocParseMode)mode, &back, &end) !=
                mloc_success ||
            end != output.length || memcmp(&back, &decimal, sizeof(back)) != 0)
        {
            return false;
        }
    }
    return true;
}

static void TestRoundTrip(void)
{
    static const char* const s_locales[] = {
        "en",    "de", "fr", "de-CH",        "es", "hi", "ar", "ar-EG", "fa", "he",
        "bn",    "mr", "my", "th-u-nu-thai", "ja", "zh", "ko", "ru",    "sv", "fi",
        "pt",    "tr", "pl", "uk",           "ta", "ur", "ps", "sw",    "am", "km",
        "en-IN", "ne", "lo", "dz",           "it", "nl", "cs", "el",    "vi", "id"};
    static const struct
    {
        Style style;
        const char* value;
    } s_values[] = {
        {styleDecimal, "1234567.891"},
        {styleDecimal, "-0.5"},
        {styleDecimal, "-0"},
        {styleDecimal, "1234.56789"},
        {styleDecimal, "987654321987654321"},
        {stylePercent, "0.25"},
        {stylePercent, "-12.345"},
        {stylePerMille, "0.0125"},
        {styleScientific, "123456"},
        {styleScientific, "-0.000123"},
        {stylePlus, "42"},
        {stylePlus, "-42"},
    };
    for (size_t l = 0; l < sizeof(s_locales) / sizeof(s_locales[0]); l++)
    {
        for (size_t v = 0; v < sizeof(s_values) / sizeof(s_values[0]); v++)
        {
            if (!RoundTrips(s_locales[l], s_values[v].style, s_values[v].value))
            {
                CHECK(false, s_locales[l]);
            }
        }
    }
}

static mlocResult ParseIn(const char* id, Style style, const char* text, size_t length,
                          mlocDecimal* decimal, size_t* end)
{
    mlocNumberFormatter formatter;
    if (!Formatter(id, style, &formatter))
    {
        return (mlocResult)-100;
    }
    return ParseCopy(&formatter, text, length, mloc_parseLenient, decimal, end);
}

static void TestBounds(void)
{
    char text[700];
    mlocDecimal d;
    size_t end = 0;
    memset(text, '0', 300);
    text[300] = '1';
    CHECK(ParseIn("en", styleDecimal, text, 301, &d, &end) == mloc_success && end == 301 &&
              d.count == 1 && d.exponent == 0,
          "leading zeros are not digits held");
    memset(text, '0', sizeof(text));
    text[0] = '1';
    CHECK(ParseIn("en", styleDecimal, text, 600, &d, &end) == mloc_success && end == 600 &&
              d.count == 1 && d.exponent == 599,
          "trailing zeros are counted");
    text[0] = '0';
    text[1] = '.';
    text[599] = '7';
    CHECK(ParseIn("en", styleDecimal, text, 600, &d, &end) == mloc_success && d.count == 1 &&
              d.exponent == -598 && d.fractionDigits == 598,
          "a long fraction");
    memset(text, '9', sizeof(text));
    CHECK(ParseIn("en", styleDecimal, text, mlocDecimalDigits, &d, &end) == mloc_success &&
              d.count == mlocDecimalDigits,
          "as many digits as a decimal holds");
    CHECK(ParseIn("en", styleDecimal, text, mlocDecimalDigits + 1, &d, &end) == mloc_errorCapacity,
          "one more");
    CHECK(ParseIn("en", styleDecimal, "1E999999", 8, &d, &end) == mloc_success &&
              d.exponent == 999999,
          "the largest exponent");
    CHECK(ParseIn("en", styleDecimal, "1E-999999", 9, &d, &end) == mloc_success &&
              d.exponent == -999999 && d.fractionDigits == 999999,
          "the smallest exponent");
    CHECK(ParseIn("en", styleDecimal, "1E1000000", 9, &d, &end) == mloc_errorCapacity,
          "an exponent past it");
    CHECK(ParseIn("en", styleDecimal, "1E-9999999999", 13, &d, &end) == mloc_errorCapacity,
          "an exponent past 999,999,999");
    CHECK(ParseIn("en", stylePercent, "1E-999998", 9, &d, &end) == mloc_errorCapacity,
          "a scale past the smallest exponent");
    CHECK(ParseIn("en", styleDecimal,
                  "\xFF"
                  "12",
                  3, &d, &end) == mloc_errorSyntax &&
              end == 0,
          "a byte that does not begin UTF-8");
    CHECK(ParseIn("en", styleDecimal, "12\xE2\x82", 4, &d, &end) == mloc_success && end == 2,
          "a truncated sequence ends the number");
    CHECK(ParseIn("en", styleDecimal, "1\xC0\x80", 3, &d, &end) == mloc_success && end == 1,
          "an overlong form is no character");
    CHECK(ParseIn("en", styleDecimal, "1\xED\xA0\x80", 4, &d, &end) == mloc_success && end == 1,
          "nor a surrogate");
    CHECK(ParseIn("en", styleDecimal, "12", 1, &d, &end) == mloc_success && end == 1 &&
              d.digits[0] == 1,
          "the text ends at its length");
}

static void TestRefusals(void)
{
    mlocNumberFormatter formatter;
    mlocDecimal d;
    size_t end = 0;
    CHECK(Formatter("en", styleDecimal, &formatter), "a formatter");
    CHECK(mlocParseNumber(nullptr, "1", 1, mloc_parseLenient, &d, &end) == mloc_errorInvalid &&
              mlocParseNumber(&formatter, nullptr, 1, mloc_parseLenient, &d, &end) ==
                  mloc_errorInvalid &&
              mlocParseNumber(&formatter, "1", 1, mloc_parseLenient, nullptr, &end) ==
                  mloc_errorInvalid &&
              mlocParseNumber(&formatter, "1", 1, mloc_parseLenient, &d, nullptr) ==
                  mloc_errorInvalid,
          "null pointers");
    CHECK(mlocParseNumber(&formatter, nullptr, 0, mloc_parseLenient, &d, &end) ==
                  mloc_errorSyntax &&
              end == 0,
          "no text is no number");
    CHECK(mlocParseNumber(&formatter, "1", 1, (mlocParseMode)2, &d, &end) == mloc_errorInvalid,
          "a mode out of range");
    mlocNumberFormatter zero = {0};
    CHECK(mlocParseNumber(&zero, "1", 1, mloc_parseLenient, &d, &end) == mloc_errorInvalid,
          "a formatter the library did not make");
    mlocLocale locale;
    mlocNumberOptions compact = {.notation = mloc_notationCompactShort};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitNumberFormatter(&formatter, &s_data, &locale, &compact) == mloc_success &&
              mlocParseNumber(&formatter, "1K", 2, mloc_parseLenient, &d, &end) ==
                  mloc_errorInvalid,
          "compact notation is not read");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestCases();
    TestRoundTrip();
    TestBounds();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
