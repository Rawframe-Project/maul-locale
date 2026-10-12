// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decimal values (record mloc-0019): parsing with the digits shown, the
// bounds and refusals, integers, writing back, equality by memcmp, and
// the plural operands with UTS #35's examples and wide integers.

#include "test_harness.h"

#include "maul-locale/decimal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool Parsed(const char* text, mlocDecimal* out)
{
    return mlocParseDecimal(text, strlen(text), out).status == mloc_success;
}

static bool Writes(const mlocDecimal* d, const char* expected)
{
    char buffer[128];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return mlocFormatDecimal(d, &output) == mloc_success && output.length == strlen(expected) &&
           memcmp(buffer, expected, output.length) == 0;
}

// Whether text parses and writes back as expected.
static bool Reads(const char* text, const char* expected)
{
    mlocDecimal d;
    return Parsed(text, &d) && Writes(&d, expected);
}

static bool Refuses(const char* text, mlocResult status, size_t offset)
{
    mlocDecimal d;
    memset(&d, 0x5A, sizeof(d));
    mlocDecimal before = d;
    mlocTextResult result = mlocParseDecimal(text, strlen(text), &d);
    return result.status == status && result.offset == offset &&
           memcmp(&d, &before, sizeof(d)) == 0;
}

static void TestParse(void)
{
    CHECK(Reads("0", "0"), "zero");
    CHECK(Reads("-0", "-0"), "negative zero keeps its sign");
    CHECK(Reads("1.50", "1.50"), "trailing zeros are shown");
    CHECK(Reads("001.500", "1.500"), "leading zeros are not");
    CHECK(Reads("1.50e1", "15.0"), "the exponent moves the point");
    CHECK(Reads("150e-2", "1.50"), "either way");
    CHECK(Reads("1e3", "1000"), "an integer by its exponent");
    CHECK(Reads("1E-3", "0.001"), "a capital E");
    CHECK(Reads(".5", "0.5"), "no integer digit");
    CHECK(Reads("5.", "5"), "no fraction digit");
    CHECK(Reads("+7", "7"), "a plus sign");
    CHECK(Reads("0.000", "0.000"), "a zero with fraction digits");
    CHECK(Reads("-1234.5", "-1234.5"), "a negative value");
    mlocDecimal d;
    CHECK(Parsed("1.50", &d) && d.count == 2 && d.digits[0] == 1 && d.digits[1] == 5 &&
              d.exponent == -1 && d.fractionDigits == 2,
          "the fields of 1.50");
    CHECK(Parsed("1200", &d) && d.count == 2 && d.exponent == 2 && d.fractionDigits == 0,
          "an integer's zeros are no digits");
    mlocDecimal a;
    mlocDecimal b;
    CHECK(Parsed("1.50e1", &a) && Parsed("15.0", &b) && memcmp(&a, &b, sizeof(a)) == 0,
          "the same value and digits compare equal");
    CHECK(Parsed("15", &b) && memcmp(&a, &b, sizeof(a)) != 0, "other digits shown do not");
}

static void TestBounds(void)
{
    const char* forty = "1234567890123456789012345678901234567891";
    CHECK(Reads(forty, forty), "forty significant digits");
    CHECK(Reads("1234567890123456789012345678901234567890",
                "1234567890123456789012345678901234567890"),
          "and forty digits whose last is 0");
    CHECK(Refuses("12345678901234567890123456789012345678901", mloc_errorCapacity, 0),
          "forty-one are refused");
    CHECK(Refuses("1000000000000000000000000000000000000000001", mloc_errorCapacity, 0),
          "zeros between digits count");
    CHECK(Parsed("100000000000000000000000000000000000000000000000000", &(mlocDecimal){0}),
          "zeros after the last digit do not");
    CHECK(Parsed("1e999999", &(mlocDecimal){0}) && Parsed("1e-999999", &(mlocDecimal){0}),
          "the largest and smallest powers");
    CHECK(Refuses("1e1000000", mloc_errorCapacity, 0), "past the largest power");
    CHECK(Refuses("1e-1000000", mloc_errorCapacity, 0), "past the smallest");
    CHECK(Parsed("12e999998", &(mlocDecimal){0}), "a first digit at the largest power");
    CHECK(Refuses("12e999999", mloc_errorCapacity, 0), "and one past it");
    CHECK(Parsed("0e-999999", &(mlocDecimal){0}), "a zero showing the most fraction digits");
    CHECK(Refuses("0e-1000000", mloc_errorCapacity, 0), "and one more");
    CHECK(Refuses("1e99999999999999999999999", mloc_errorCapacity, 0),
          "an exponent too long for any integer");
    CHECK(Parsed("0e99999999999999999999999", &(mlocDecimal){0}), "which a zero takes");
}

static void TestSyntax(void)
{
    CHECK(Refuses("", mloc_errorSyntax, 0), "empty text");
    CHECK(Refuses("-", mloc_errorSyntax, 1), "a sign alone");
    CHECK(Refuses(".", mloc_errorSyntax, 1), "a point alone");
    CHECK(Refuses("1.2.3", mloc_errorSyntax, 3), "a second point");
    CHECK(Refuses("1e", mloc_errorSyntax, 2), "an exponent without digits");
    CHECK(Refuses("1e+", mloc_errorSyntax, 3), "or after its sign");
    CHECK(Refuses("abc", mloc_errorSyntax, 0), "letters");
    CHECK(Refuses("1x", mloc_errorSyntax, 1), "a letter after digits");
    CHECK(Refuses(" 1", mloc_errorSyntax, 0), "a space");
    CHECK(Refuses("--1", mloc_errorSyntax, 1), "two signs");
    CHECK(Refuses("1e5x", mloc_errorSyntax, 3), "a letter after the exponent");
    mlocDecimal d;
    CHECK(mlocParseDecimal(nullptr, 1, &d).status == mloc_errorInvalid &&
              mlocParseDecimal("1", 1, nullptr).status == mloc_errorInvalid,
          "null pointers");
    CHECK(mlocParseDecimal(nullptr, 0, &d).status == mloc_errorSyntax, "no text at all");
    CHECK(mlocParseDecimal("12", 1, &d).status == mloc_success && Writes(&d, "1"),
          "only the length given is read");
    CHECK(mlocParseDecimal("1e12", 3, &d).status == mloc_success && Writes(&d, "10"),
          "in the exponent too");
}

static void TestIntegers(void)
{
    mlocDecimal d;
    CHECK(mlocMakeDecimal(INT64_MIN, &d) == mloc_success && Writes(&d, "-9223372036854775808"),
          "the smallest int64_t");
    CHECK(mlocMakeDecimal(INT64_MAX, &d) == mloc_success && Writes(&d, "9223372036854775807"),
          "the largest");
    CHECK(mlocMakeUnsignedDecimal(UINT64_MAX, &d) == mloc_success &&
              Writes(&d, "18446744073709551615"),
          "the largest uint64_t");
    CHECK(mlocMakeDecimal(0, &d) == mloc_success && Writes(&d, "0") && d.count == 0 &&
              d.negative == 0,
          "zero");
    CHECK(mlocMakeDecimal(-1200, &d) == mloc_success && Writes(&d, "-1200") && d.count == 2 &&
              d.exponent == 2,
          "an integer's zeros");
    mlocDecimal parsed;
    CHECK(mlocMakeDecimal(-1200, &d) == mloc_success && Parsed("-1200", &parsed) &&
              memcmp(&d, &parsed, sizeof(d)) == 0,
          "equal to the same integer parsed");
    CHECK(mlocMakeDecimal(1, nullptr) == mloc_errorInvalid &&
              mlocMakeUnsignedDecimal(1, nullptr) == mloc_errorInvalid,
          "null pointers");
}

static void TestFormat(void)
{
    mlocDecimal d;
    char buffer[3];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(Parsed("1.50", &d) && mlocFormatDecimal(&d, &output) == mloc_errorCapacity &&
              output.length == 4,
          "a short buffer reports the length needed");
    mlocDecimal special = {.kind = mloc_decimalInfinity};
    CHECK(Writes(&special, "Infinity"), "infinity");
    special.negative = 1;
    CHECK(Writes(&special, "-Infinity"), "negative infinity");
    special = (mlocDecimal){.kind = mloc_decimalNaN};
    CHECK(Writes(&special, "NaN"), "NaN");
    CHECK(Parsed("1e-3", &d) && Writes(&d, "0.001"), "zeros after the point");
    char exact[2] = {'?', '?'};
    mlocOutput tight = {.buffer = exact, .capacity = 1};
    CHECK(Parsed("5", &d) && mlocFormatDecimal(&d, &tight) == mloc_success && tight.length == 1 &&
              exact[0] == '5' && exact[1] == '?',
          "an integer writes no point");
    CHECK(mlocFormatDecimal(nullptr, &output) == mloc_errorInvalid &&
              mlocFormatDecimal(&d, nullptr) == mloc_errorInvalid,
          "null pointers");
}

// A decimal the library did not make, spoiled one field at a time.
static void TestMadeUp(void)
{
    mlocDecimal good;
    CHECK(Parsed("1.50", &good), "a decimal to spoil");
    mlocDecimal d = good;
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocPluralOperands operands;
    d.digits[1] = 10;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a digit past 9");
    d = good;
    d.digits[2] = 1;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a digit past the count");
    d = good;
    d.digits[1] = 0;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a last digit of 0");
    d = good;
    d.digits[0] = 0;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a first digit of 0");
    d = good;
    d.count = 41;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a count past 40");
    d = good;
    d.fractionDigits = 0;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "fewer fraction digits shown");
    d = good;
    d.fractionDigits = mlocDecimalMaxExponent + 1;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "too many");
    d = good;
    d.exponent = -mlocDecimalMaxExponent - 1;
    d.fractionDigits = mlocDecimalMaxExponent + 1;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a last digit too small");
    d = good;
    d.exponent = mlocDecimalMaxExponent;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a first digit too large");
    d = good;
    d.reserved = 1;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "the reserved byte");
    d = good;
    d.negative = 2;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a sign past 1");
    d = good;
    d.kind = 3;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "no kind");
    d = good;
    d.kind = mloc_decimalInfinity;
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "infinity with digits");
    d = (mlocDecimal){.kind = mloc_decimalNaN, .negative = 1};
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a negative NaN");
    d = (mlocDecimal){.exponent = 1};
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "a zero with an exponent");
    d = (mlocDecimal){.fractionDigits = -1};
    CHECK(mlocFormatDecimal(&d, &output) == mloc_errorInvalid, "negative fraction digits");
    d = (mlocDecimal){.kind = mloc_decimalInfinity};
    CHECK(mlocGetPluralOperands(&d, &operands) == mloc_errorInvalid, "infinity has no operands");
    d = good;
    d.reserved = 1;
    CHECK(mlocGetPluralOperands(&d, &operands) == mloc_errorInvalid, "nor a made-up decimal");
}

static bool Operands(const char* text, uint64_t i, uint32_t v, uint32_t w, uint64_t f, uint64_t t)
{
    mlocDecimal d;
    mlocPluralOperands o;
    memset(&o, 0x5A, sizeof(o));
    return Parsed(text, &d) && mlocGetPluralOperands(&d, &o) == mloc_success && o.i == i &&
           o.v == v && o.w == w && o.f == f && o.t == t && o.c == 0;
}

static void TestOperands(void)
{
    // UTS #35 Part 3's table of operand examples.
    CHECK(Operands("1", 1, 0, 0, 0, 0), "1");
    CHECK(Operands("1.0", 1, 1, 0, 0, 0), "1.0");
    CHECK(Operands("1.00", 1, 2, 0, 0, 0), "1.00");
    CHECK(Operands("1.3", 1, 1, 1, 3, 3), "1.3");
    CHECK(Operands("1.30", 1, 2, 1, 30, 3), "1.30");
    CHECK(Operands("1.03", 1, 2, 2, 3, 3), "1.03");
    CHECK(Operands("1.230", 1, 3, 2, 230, 23), "1.230");
    CHECK(Operands("1200000", 1200000, 0, 0, 0, 0), "1200000");
    CHECK(Operands("1.2e3", 1200, 0, 0, 0, 0), "1.2e3 is 1200");
    CHECK(Operands("0.001", 0, 3, 3, 1, 1), "0.001");
    CHECK(Operands("-5.5", 5, 1, 1, 5, 5), "the sign is no operand");
    const uint64_t e18 = 1000000000000000000u;
    CHECK(Operands("999999999999999999", e18 - 1, 0, 0, 0, 0), "18 digits are themselves");
    CHECK(Operands("1e18", e18, 0, 0, 0, 0), "10^18");
    CHECK(Operands("1000000000000000001", e18 + 1, 0, 0, 0, 0), "19 digits: 10^18 and the low 18");
    CHECK(Operands("123456789012345678901", e18 + 456789012345678901u, 0, 0, 0, 0), "21 digits");
    CHECK(Operands("0.12345678901234567890", 0, 20, 19, e18 + 345678901234567890u,
                   e18 + 234567890123456789u),
          "a wide fraction");
    CHECK(Operands("1e-30", 0, 30, 30, 1, 1), "a fraction digit far out");
    CHECK(Operands("5e-19", 0, 19, 19, 5, 5), "one at the 19th place");
    CHECK(Operands("5.000000000000000000001", 5, 21, 21, 1, 1),
          "leading zeros of a fraction make no wide f");
    CHECK(Operands("0.1000000000000000000001", 0, 22, 22, e18 + 1, e18 + 1),
          "a fraction of 22 digits");
    CHECK(Operands("12345678901234567890123456789012345678910", 1456789012345678910u, 0, 0, 0, 0),
          "forty digits after a zero");
    CHECK(Operands("1e999999", e18, 0, 0, 0, 0), "the largest power has operands");
    CHECK(Operands("1e-999999", 0, 999999, 999999, 1, 1), "and the smallest");
    CHECK(Operands("0e-999999", 0, 999999, 0, 0, 0), "a zero with the most fraction digits");
    mlocDecimal d;
    CHECK(Parsed("1", &d) && mlocGetPluralOperands(&d, nullptr) == mloc_errorInvalid &&
              mlocGetPluralOperands(nullptr, &(mlocPluralOperands){0}) == mloc_errorInvalid,
          "null pointers");
}

// Whether a double's shortest decimal has the digits and exponent given.
static bool Shortest(double value, const char* digits, int32_t exponent, int32_t fraction)
{
    mlocDecimal d;
    if (mlocMakeShortestDecimal(value, &d) != mloc_success || d.kind != mloc_decimalFinite ||
        d.count != strlen(digits) || d.exponent != exponent || d.fractionDigits != fraction)
    {
        return false;
    }
    for (int k = 0; k < d.count; k++)
    {
        if (d.digits[k] != digits[k] - '0')
        {
            return false;
        }
    }
    return true;
}

static double FromBits(uint64_t bits)
{
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void TestShortest(void)
{
    // Python's repr, the shortest round-trip digits, is the reference.
    CHECK(Shortest(0.1, "1", -1, 1), "0.1");
    CHECK(Shortest(0.3, "3", -1, 1), "0.3");
    CHECK(Shortest(1.0, "1", 0, 0), "1.0 shows no fraction digit");
    CHECK(Shortest(100.0, "1", 2, 0), "100");
    CHECK(Shortest(0.5, "5", -1, 1), "0.5");
    CHECK(Shortest(1e23, "1", 23, 0), "1e23, not 9.999999999999999e22");
    CHECK(Shortest(FromBits(1), "5", -324, 324), "the smallest subnormal");
    CHECK(Shortest(FromBits(3), "15", -324, 324), "three times it");
    CHECK(Shortest(FromBits(0x7FEFFFFFFFFFFFFF), "17976931348623157", 292, 0), "the largest");
    CHECK(Shortest(2.98023223876953125e-08, "29802322387695312", -24, 24),
          "a value exactly halfway rounds to even");
    CHECK(Shortest(123456.789, "123456789", -3, 3), "123456.789");
    CHECK(Shortest(FromBits(0x0010000000000000), "22250738585072014", -324, 324),
          "the smallest normal");
    CHECK(Shortest(FromBits(0x000FFFFFFFFFFFFF), "2225073858507201", -323, 323),
          "the largest subnormal");
    CHECK(Shortest(9007199254740992.0, "9007199254740992", 0, 0), "2^53");
    CHECK(Shortest(1e-7, "1", -7, 7), "1e-7");
    CHECK(Shortest(1.5e300, "15", 299, 0), "1.5e300");
    CHECK(Shortest(-2.5, "25", -1, 1), "a negative value");
    mlocDecimal d;
    CHECK(mlocMakeShortestDecimal(-2.5, &d) == mloc_success && d.negative == 1, "keeps its sign");
    CHECK(mlocMakeShortestDecimal(-0.0, &d) == mloc_success && d.count == 0 && d.negative == 1 &&
              Writes(&d, "-0"),
          "negative zero");
    CHECK(mlocMakeShortestDecimal(0.0, &d) == mloc_success && Writes(&d, "0"), "zero");
    CHECK(mlocMakeShortestDecimal(FromBits(0x7FF0000000000000), &d) == mloc_success &&
              Writes(&d, "Infinity"),
          "infinity");
    CHECK(mlocMakeShortestDecimal(FromBits(0xFFF0000000000000), &d) == mloc_success &&
              Writes(&d, "-Infinity"),
          "negative infinity");
    CHECK(mlocMakeShortestDecimal(FromBits(0xFFF8000000000001), &d) == mloc_success &&
              Writes(&d, "NaN") && d.negative == 0,
          "NaN, whatever its sign bit");
    CHECK(mlocMakeShortestDecimal(1.0, nullptr) == mloc_errorInvalid, "a null pointer");
}

// Random doubles read back from their shortest digits.
static void TestRoundTrip(void)
{
    uint64_t state = 0x9E3779B97F4A7C15u;
    int failures = 0;
    for (int n = 0; n < 200000; n++)
    {
        state = state * 6364136223846793005u + 1442695040888963407u;
        uint64_t bits = state ^ (state >> 29);
        double value = FromBits(bits);
        mlocDecimal d;
        if (mlocMakeShortestDecimal(value, &d) != mloc_success)
        {
            failures++;
            continue;
        }
        if (d.kind != mloc_decimalFinite)
        {
            continue;
        }
        char text[64];
        int at = d.negative ? snprintf(text, sizeof(text), "-") : 0;
        for (int k = 0; k < d.count; k++)
        {
            text[at++] = (char)('0' + d.digits[k]);
        }
        snprintf(text + at, sizeof(text) - (size_t)at, "%se%d", d.count == 0 ? "0" : "",
                 (int)d.exponent);
        double back = strtod(text, nullptr);
        failures += memcmp(&back, &value, sizeof(value)) != 0 || d.count > 17;
    }
    CHECK(failures == 0, "200,000 random doubles read back from at most 17 digits");
}

int main(void)
{
    TestParse();
    TestBounds();
    TestSyntax();
    TestIntegers();
    TestFormat();
    TestMadeUp();
    TestOperands();
    TestShortest();
    TestRoundTrip();
    return s_failures == 0 ? 0 : 1;
}
