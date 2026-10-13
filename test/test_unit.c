// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit conversion on the compiled-in data (record mloc-0023): UTS #35's
// worked examples, every form of identifier (prefixes, powers, "per",
// constants, aliases), temperatures with their offsets, reciprocals,
// Beaufort, values that are not finite, precisions, and the refusals.

#include "test_harness.h"

#include "maul-locale/unit.h"

#include <string.h>

static mlocDataSet s_data;

static mlocResult Convert(const char* value, const char* from, const char* to,
                          const mlocPrecision* precision, char* text, size_t capacity)
{
    mlocDecimal v;
    mlocDecimal out;
    if (mlocParseDecimal(value, strlen(value), &v).status != mloc_success)
    {
        return (mlocResult)-100;
    }
    mlocResult r =
        mlocConvertUnit(&s_data, from, strlen(from), to, strlen(to), precision, &v, &out);
    mlocOutput output = {.buffer = text, .capacity = capacity - 1};
    if (r == mloc_success && mlocFormatDecimal(&out, &output) == mloc_success)
    {
        text[output.length] = '\0';
    }
    return r;
}

// Whether a value converts to the expected text.
static bool Converts(const char* value, const char* from, const char* to, const char* expected)
{
    char text[128] = "";
    return Convert(value, from, to, nullptr, text, sizeof(text)) == mloc_success &&
           strcmp(text, expected) == 0;
}

static bool Rounds(const char* value, const char* from, const char* to,
                   const mlocPrecision* precision, const char* expected)
{
    char text[128] = "";
    return Convert(value, from, to, precision, text, sizeof(text)) == mloc_success &&
           strcmp(text, expected) == 0;
}

static mlocResult Refusal(const char* from, const char* to)
{
    char text[128];
    return Convert("1", from, to, nullptr, text, sizeof(text));
}

static void TestExamples(void)
{
    CHECK(Converts("50", "foot-per-minute", "mile-per-hour",
                   "0.56818181818181818181818181818181818182"),
          "UTS #35: foot per minute to miles per hour, 38 digits half even");
    CHECK(Converts("50", "mile-per-gallon", "liter-per-100-kilometer",
                   "4.7042916666666666666666666666666666667"),
          "UTS #35: a reciprocal");
    CHECK(Converts("50", "foot-per-minute", "hour-per-mile", "1.76"),
          "UTS #35: a reciprocal of a speed, exact");
    CHECK(Converts("1", "foot", "meter", "0.3048") && Converts("12", "inch", "foot", "1") &&
              Converts("1", "kilowatt-hour", "joule", "3600000"),
          "exact results as they are");
}

static void TestIdentifiers(void)
{
    CHECK(Converts("1", "kilometer", "meter", "1000") &&
              Converts("1", "kibibyte", "byte", "1024") &&
              Converts("1", "microsecond", "nanosecond", "1000") &&
              Converts("1", "gigagram", "microgram", "1000000000000000") &&
              Converts("1", "zettameter", "exameter", "1000") &&
              Converts("1000", "attometer", "femtometer", "1"),
          "SI and binary prefixes");
    CHECK(Converts("1", "kilogram", "gram", "1000"), "a unit the table names before a prefix");
    CHECK(Converts("1", "square-foot", "square-meter", "0.09290304") &&
              Converts("1", "cubic-centimeter", "milliliter", "1") &&
              Converts("1", "pow4-meter", "pow4-centimeter", "100000000") &&
              Converts("1", "pow2-meter", "square-meter", "1"),
          "powers");
    CHECK(Converts("1", "foot-per-second-per-second", "foot-per-square-second", "1") &&
              Converts("1", "revolution-per-second", "hertz", "1") &&
              Converts("60", "per-minute", "per-second", "1") &&
              Converts("1", "newton-meter", "joule", "1") &&
              Converts("1", "meter-kilogram-per-square-second", "newton", "1"),
          "products, \"per\" once and twice, an empty numerator");
    CHECK(Converts("5", "liter-per-100-kilometer", "liter-per-kilometer", "0.05") &&
              Converts("1", "part-per-1e6", "part-per-1e9", "1000") &&
              Converts("1", "per-12-meter", "per-meter",
                       "0.083333333333333333333333333333333333333") &&
              Converts("1", "12-meter", "meter", "12") &&
              Converts("18", "per-9-meter", "per-meter", "2"),
          "unit constants, in the denominator and the numerator");
    CHECK(Converts("1", "metric-ton", "kilogram", "1000") &&
              Converts("1", "meter-per-second-squared", "meter-per-square-second", "1") &&
              Converts("1", "metric-ton-per-hour", "kilogram-per-hour", "1000") &&
              Converts("2", "portion", "part", "2") && Converts("1", "inch-hg", "inch-ofhg", "1"),
          "deprecated identifiers, whole and inside a compound");
}

static void TestTemperatures(void)
{
    CHECK(Converts("100", "celsius", "fahrenheit", "212") &&
              Converts("-40", "celsius", "fahrenheit", "-40") &&
              Converts("0", "kelvin", "celsius", "-273.15") &&
              Converts("-273.15", "celsius", "kelvin", "0") &&
              Converts("-459.67", "fahrenheit", "kelvin", "0") &&
              Converts("1e-30", "celsius", "fahrenheit", "32.0000000000000000000000000000018"),
          "temperatures with their offsets");
    CHECK(Converts("1", "celsius-per-second", "kelvin-per-second", "1") &&
              Converts("1", "square-celsius", "square-kelvin", "1") &&
              Converts("1", "fahrenheit-per-second", "kelvin-per-second",
                       "0.55555555555555555555555555555555555556"),
          "a temperature in a compound has no offset");
    char text[128];
    mlocDecimal v;
    mlocDecimal out;
    CHECK(Convert("1e700", "celsius", "kelvin", nullptr, text, sizeof(text)) ==
                  mloc_errorCapacity &&
              mlocParseDecimal("1e600", 5, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "celsius", 7, "kelvin", 6, nullptr, &v, &out) ==
                  mloc_success &&
              out.count == 1 && out.exponent == 600,
          "an offset needs the power within the integers");
}

static void TestLargeValues(void)
{
    mlocDecimal v;
    mlocDecimal out;
    CHECK(mlocParseDecimal("1e999990", 8, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "kilometer", 9, "meter", 5, nullptr, &v, &out) ==
                  mloc_success &&
              out.count == 1 && out.digits[0] == 1 && out.exponent == 999993,
          "a scale keeps the power apart");
    CHECK(mlocParseDecimal("1e999999", 8, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "kilometer", 9, "meter", 5, nullptr, &v, &out) ==
                  mloc_errorCapacity,
          "a result past the decimal's exponent");
    CHECK(mlocParseDecimal("1e-999999", 9, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "meter", 5, "kilometer", 9, nullptr, &v, &out) ==
                  mloc_errorCapacity,
          "a result below the decimal's exponent");
    CHECK(mlocParseDecimal("1e-999990", 9, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "foot", 4, "meter", 5, nullptr, &v, &out) == mloc_success &&
              out.exponent == -999994 && out.count == 4,
          "an exact result near the smallest exponent");
    CHECK(mlocParseDecimal("1e-999950", 9, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "inch", 4, "foot", 4, nullptr, &v, &out) == mloc_success &&
              out.exponent == -999989 && out.count == 38,
          "an inexact result near the smallest exponent");
    CHECK(mlocParseDecimal("1e-999970", 9, &v).status == mloc_success &&
              mlocConvertUnit(&s_data, "inch", 4, "foot", 4, nullptr, &v, &out) ==
                  mloc_errorCapacity,
          "an inexact result whose digits pass the smallest exponent");
}

static void TestBeaufort(void)
{
    CHECK(Converts("0", "beaufort", "meter-per-second", "0.15") &&
              Converts("12.5", "beaufort", "meter-per-second", "39.15") &&
              Converts("17", "beaufort", "meter-per-second", "58.6") &&
              Converts("100", "beaufort", "meter-per-second", "58.6") &&
              Converts("-3", "beaufort", "meter-per-second", "4.45") &&
              Converts("1e100", "beaufort", "meter-per-second", "58.6") &&
              Converts("1e-100", "beaufort", "meter-per-second", "0.15"),
          "a force's middle speed, rounded half up and within the table");
    CHECK(Converts("10", "meter-per-second", "beaufort", "5") &&
              Converts("10.8", "meter-per-second", "beaufort", "6") &&
              Converts("0.29", "meter-per-second", "beaufort", "0") &&
              Converts("0", "meter-per-second", "beaufort", "0") &&
              Converts("-10", "meter-per-second", "beaufort", "5") &&
              Converts("1e100", "meter-per-second", "beaufort", "17") &&
              Converts("1e-100", "meter-per-second", "beaufort", "0") &&
              Converts("100", "kilometer-per-hour", "beaufort", "10") &&
              Converts("3", "beaufort", "beaufort", "3"),
          "the force whose range holds a speed");
    CHECK(Refusal("beaufort-per-second", "meter-per-square-second") == mloc_errorInvalid &&
              Refusal("beaufort", "second-per-meter") == mloc_errorInvalid &&
              Refusal("square-beaufort", "square-meter-per-square-second") == mloc_errorInvalid,
          "beaufort only alone and not as a reciprocal");
}

// Converts a double (an infinity or NaN) and writes the result's text.
static bool ConvertsDouble(double value, const char* from, const char* to, const char* expected)
{
    mlocDecimal v;
    mlocDecimal out;
    char text[64];
    mlocOutput output = {.buffer = text, .capacity = sizeof(text)};
    return mlocMakeShortestDecimal(value, &v) == mloc_success &&
           mlocConvertUnit(&s_data, from, strlen(from), to, strlen(to), nullptr, &v, &out) ==
               mloc_success &&
           mlocFormatDecimal(&out, &output) == mloc_success && output.length == strlen(expected) &&
           memcmp(text, expected, output.length) == 0;
}

static void TestNotFinite(void)
{
    double infinity = 1.0 / 0.0;
    CHECK(Converts("0", "mile-per-gallon", "liter-per-100-kilometer", "Infinity") &&
              ConvertsDouble(infinity, "mile-per-gallon", "liter-per-100-kilometer", "0") &&
              ConvertsDouble(-infinity, "meter", "foot", "-Infinity") &&
              ConvertsDouble(infinity, "celsius", "kelvin", "Infinity") &&
              ConvertsDouble(infinity - infinity, "meter", "foot", "NaN") &&
              Converts("-0", "meter", "foot", "-0") &&
              ConvertsDouble(infinity, "beaufort", "meter-per-second", "58.6") &&
              ConvertsDouble(infinity, "meter-per-second", "beaufort", "17"),
          "zero, infinities and NaN");
}

static void TestPrecision(void)
{
    mlocPrecision two = {.kind = mloc_precisionFraction, .maxFractionDigits = 2};
    mlocPrecision three = {.kind = mloc_precisionSignificant,
                           .minSignificantDigits = 1,
                           .maxSignificantDigits = 3,
                           .mode = mloc_roundCeil};
    CHECK(Rounds("1", "radian", "degree", &two, "57.3") &&
              Rounds("1", "radian", "degree", &three, "57.3") &&
              Rounds("1", "foot", "meter", &two, "0.3") &&
              Rounds("1", "inch", "foot", &three, "0.0834"),
          "a precision rounds the exact value once");
    mlocPrecision bad = {.kind = mloc_precisionFraction, .maxFractionDigits = 101};
    char text[128];
    CHECK(Convert("1", "foot", "meter", &bad, text, sizeof(text)) == mloc_errorInvalid,
          "an invalid precision");
}

static void TestRefusals(void)
{
    static const char* const s_syntax[] = {"",
                                           "meter-",
                                           "-meter",
                                           "Meter",
                                           "meter--foot",
                                           "per",
                                           "meter-per",
                                           "per-per-meter",
                                           "meter-per-per-second",
                                           "square",
                                           "square-12",
                                           "1-meter",
                                           "0100-meter",
                                           "per-123456789-meter",
                                           "foot-and",
                                           "and-foot",
                                           "foot-and-and-inch",
                                           "foot-and-12",
                                           "foot-and-inch-per-second",
                                           "curr-us",
                                           "curr",
                                           "square-1e3",
                                           "per-2e0",
                                           "me",
                                           "meter-1x"};
    for (size_t i = 0; i < sizeof(s_syntax) / sizeof(s_syntax[0]); i++)
    {
        CHECK(Refusal(s_syntax[i], "meter") == mloc_errorSyntax, s_syntax[i]);
    }
    CHECK(Refusal("blorp", "meter") == mloc_errorNotFound &&
              Refusal("kiloblorp", "meter") == mloc_errorNotFound &&
              Refusal("xxx-knuts", "meter") == mloc_errorNotFound &&
              Refusal("x-knuts", "meter") == mloc_errorNotFound &&
              Refusal("meter", "pow16-meter") == mloc_errorNotFound,
          "unknown and private-use units");
    CHECK(Refusal("meter", "second") == mloc_errorInvalid &&
              Refusal("foot-and-inch", "meter") == mloc_errorInvalid &&
              Refusal("curr-usd", "curr-eur") == mloc_errorInvalid &&
              Refusal("kilogram", "meter") == mloc_errorInvalid &&
              Refusal("meter", "foot-and-inch") == mloc_errorInvalid,
          "units that do not convert");
    CHECK(Refusal("pow15-dalton-pow15-dalton", "kilogram") == mloc_errorCapacity &&
              Refusal("meter-meter-meter-meter-meter-meter-meter-meter-meter", "meter") ==
                  mloc_errorCapacity &&
              Refusal("foot-and-foot-and-foot-and-foot-and-foot", "foot") == mloc_errorCapacity &&
              Refusal("pow15-yobibyte-pow15-yobibyte", "pow30-bit") == mloc_errorCapacity &&
              Refusal("per-1e20", "part") == mloc_errorCapacity &&
              Refusal("per-1e9-1e9-1e9", "part") == mloc_errorCapacity,
          "past the limits");
    char longId[300];
    memset(longId, 'm', sizeof(longId) - 1);
    longId[sizeof(longId) - 1] = '\0';
    CHECK(Refusal(longId, "meter") == mloc_errorCapacity, "an identifier past 255 bytes");
    // 255 bytes of one-letter tokens: the most tokens an identifier holds.
    for (size_t i = 0; i < 255; i++)
    {
        longId[i] = i % 2 == 0 ? 'm' : '-';
    }
    longId[255] = '\0';
    CHECK(Refusal(longId, "meter") == mloc_errorSyntax, "128 tokens");
    CHECK(Refusal("zblorp", "meter") == mloc_errorNotFound, "an unknown unit from z");
    mlocDecimal v;
    mlocDecimal out;
    CHECK(mlocMakeDecimal(1, &v) == mloc_success &&
              mlocConvertUnit(nullptr, "meter", 5, "foot", 4, nullptr, &v, &out) ==
                  mloc_errorInvalid &&
              mlocConvertUnit(&s_data, nullptr, 5, "foot", 4, nullptr, &v, &out) ==
                  mloc_errorInvalid &&
              mlocConvertUnit(&s_data, "meter", 5, nullptr, 4, nullptr, &v, &out) ==
                  mloc_errorInvalid &&
              mlocConvertUnit(&s_data, "meter", 5, "foot", 4, nullptr, nullptr, &out) ==
                  mloc_errorInvalid &&
              mlocConvertUnit(&s_data, "meter", 5, "foot", 4, nullptr, &v, nullptr) ==
                  mloc_errorInvalid,
          "a null pointer");
    v.reserved = 1;
    CHECK(mlocConvertUnit(&s_data, "meter", 5, "foot", 4, nullptr, &v, &out) == mloc_errorInvalid,
          "a decimal the library did not make");
    mlocDataSet none = {nullptr, 0};
    v.reserved = 0;
    CHECK(mlocConvertUnit(&none, "meter", 5, "foot", 4, nullptr, &v, &out) == mloc_errorNotFound,
          "data without the unit table");
}

int main(void)
{
    static mlocData s_builtin;
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    TestExamples();
    TestIdentifiers();
    TestTemperatures();
    TestLargeValues();
    TestBeaufort();
    TestNotFinite();
    TestPrecision();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
