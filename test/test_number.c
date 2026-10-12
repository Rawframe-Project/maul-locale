// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number formatting on the compiled-in data (record mloc-0021): digits,
// symbols and grouping of locales of each shape, numbering systems from
// the locale, every precision option with ECMA-402's results, sign
// display, the parts, measuring, and the refusals. conform_numbers runs
// CLDR's test data in CI.

#include "test_harness.h"

#include "maul-locale/number.h"

#include <string.h>

static mlocDataSet s_data;

// Whether a number, as text, formats as expected in a locale with
// options.
static bool Formats(const char* id, const mlocNumberOptions* options, const char* number,
                    const char* expected)
{
    mlocLocale locale;
    mlocNumberFormatter formatter;
    mlocDecimal decimal;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocInitNumberFormatter(&formatter, &s_data, &locale, options) >= 0 &&
           mlocParseDecimal(number, strlen(number), &decimal).status == mloc_success &&
           mlocFormatNumber(&formatter, &decimal, &output) == mloc_success &&
           output.length == strlen(expected) && memcmp(buffer, expected, output.length) == 0;
}

static bool Plain(const char* id, const char* number, const char* expected)
{
    return Formats(id, nullptr, number, expected);
}

static mlocResult Init(const char* id, const mlocNumberOptions* options)
{
    mlocLocale locale;
    mlocNumberFormatter formatter;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success)
    {
        return (mlocResult)-100;
    }
    return mlocInitNumberFormatter(&formatter, &s_data, &locale, options);
}

static void TestLocales(void)
{
    CHECK(Plain("en", "1234567.891", "1,234,567.891"), "English groups by three");
    CHECK(Plain("en", "1.50", "1.50"), "the zero precision shows the digits the decimal shows");
    CHECK(Plain("en", "0.000", "0.000"), "zero with its fraction digits");
    CHECK(Plain("en", "-0", "-0"), "negative zero");
    CHECK(Plain("en", "123", "123") && Plain("en", "1234", "1,234"), "the first group");
    CHECK(Plain("de", "1234567.5", "1.234.567,5"), "German");
    CHECK(Plain("fr", "1234567.5",
                "1\xE2\x80\xAF"
                "234\xE2\x80\xAF"
                "567,5"),
          "French groups with a narrow no-break space");
    CHECK(Plain("de-CH", "1234567.5", "1'234'567.5"), "Swiss German");
    CHECK(Plain("es", "1234", "1234") && Plain("es", "12345", "12.345"),
          "Spanish groups from five integer digits");
    CHECK(Plain("hi", "1234567", "12,34,567"), "Hindi groups by two after the first three");
    CHECK(Plain("ar", "-1230.05", "\xE2\x80\x8E-1,230.05"), "Arabic in latn: LRM before minus");
    CHECK(Plain("ar-EG", "-1230.05",
                "\xD8\x9C-\xD9\xA1\xD9\xAC\xD9\xA2\xD9\xA3\xD9\xA0\xD9\xAB\xD9\xA0\xD9\xA5"),
          "Egyptian Arabic in arab digits");
    CHECK(Plain("qaa", "1234.5", "1,234.5"), "a language CLDR does not name takes root's");
    CHECK(Plain("en", "1e25", "10,000,000,000,000,000,000,000,000"), "a power past the digits");
    CHECK(Plain("en", "1e-7", "0.0000001"), "a small number in full");
}

static void TestSystems(void)
{
    CHECK(Plain("en-u-nu-thai", "1234567",
                "\xE0\xB9\x91,\xE0\xB9\x92\xE0\xB9\x93\xE0\xB9\x94,\xE0\xB9\x95\xE0\xB9\x96\xE0"
                "\xB9\x97"),
          "Thai digits with English symbols");
    CHECK(Plain("ar-EG-u-nu-latn", "1234", "1,234"), "latn in Egyptian Arabic");
    CHECK(Plain("ar-u-nu-native", "5", "\xD9\xA5"), "Arabic's native system");
    CHECK(Plain("ja-u-nu-hanidec", "2026", "\xE4\xBA\x8C,\xE3\x80\x87\xE4\xBA\x8C\xE5\x85\xAD"),
          "hanidec, whose digits are not consecutive");
    CHECK(Init("en", nullptr) == mloc_success && Init("ar-EG", nullptr) == mloc_success,
          "the locale's own system");
    CHECK(Init("en-u-nu-roman", nullptr) == mloc_fellBack && Plain("en-u-nu-roman", "5", "5"),
          "a system without digits takes the default");
    CHECK(Init("ta-u-nu-traditio", nullptr) == mloc_fellBack,
          "Tamil's traditional system is algorithmic");
    CHECK(Init("zh-u-nu-finance", nullptr) == mloc_fellBack, "Chinese finance digits too");
    CHECK(Init("en-u-nu-traditio", nullptr) == mloc_success &&
              Plain("en-u-nu-traditio", "12", "12"),
          "no traditional system: the native one");
    CHECK(Init("en-u-nu-finance", nullptr) == mloc_success, "no finance system: the default");
    CHECK(Init("en-u-nu-zzzz", nullptr) == mloc_fellBack, "an unknown system");
}

static mlocNumberOptions Fraction(uint8_t min, uint8_t max)
{
    return (mlocNumberOptions){.precision = {.kind = mloc_precisionFraction,
                                             .minFractionDigits = min,
                                             .maxFractionDigits = max}};
}

static mlocNumberOptions Significant(uint8_t min, uint8_t max)
{
    return (mlocNumberOptions){.precision = {.kind = mloc_precisionSignificant,
                                             .minSignificantDigits = min,
                                             .maxSignificantDigits = max}};
}

static void TestPrecision(void)
{
    mlocNumberOptions o = Fraction(0, 3);
    CHECK(Formats("en", &o, "1.23456", "1.235"), "three fraction digits, half-expand");
    CHECK(Formats("en", &o, "1.50", "1.5"), "a precision cuts zeros to its minimum");
    CHECK(Formats("en", &o, "-0.0001", "-0"), "a negative rounded to zero keeps its sign");
    o = Fraction(2, 2);
    CHECK(Formats("en", &o, "1", "1.00") && Formats("en", &o, "1.005", "1.01"),
          "two fraction digits shown");
    o.precision.mode = mloc_roundHalfEven;
    CHECK(Formats("en", &o, "1.005", "1.00") && Formats("en", &o, "1.015", "1.02"), "half to even");
    o.precision.mode = mloc_roundTrunc;
    CHECK(Formats("en", &o, "9.999", "9.99"), "toward zero");
    o = Fraction(0, 0);
    CHECK(Formats("en", &o, "999.5", "1,000"), "a carry adds a group");
    o = Significant(1, 2);
    CHECK(Formats("en", &o, "123456", "120,000") && Formats("en", &o, "0.012345", "0.012"),
          "two significant digits");
    CHECK(Formats("en", &o, "9.96", "10"), "a carry past a power");
    o = Significant(3, 5);
    CHECK(Formats("en", &o, "1", "1.00") && Formats("en", &o, "0", "0.00"),
          "three significant digits shown, zero too");
    CHECK(Formats("en", &o, "123456", "123,460"), "five at most");
    o = Significant(3, 3);
    CHECK(Formats("en", &o, "9.996", "10.0"), "a carry keeps the digits shown");
    o = (mlocNumberOptions){.precision = {.kind = mloc_precisionMorePrecision,
                                          .maxFractionDigits = 2,
                                          .minSignificantDigits = 1,
                                          .maxSignificantDigits = 2}};
    CHECK(Formats("en", &o, "1.23456", "1.23") && Formats("en", &o, "123.456", "123.46"),
          "more precision takes the fixed result when it keeps more");
    CHECK(Formats("en", &o, "0.0012", "0.0012"), "and the significant one when it does");
    o.precision.kind = mloc_precisionLessPrecision;
    CHECK(Formats("en", &o, "1.23456", "1.2") && Formats("en", &o, "0.0012", "0"),
          "less precision takes the other");
    o = (mlocNumberOptions){.precision = {.kind = mloc_precisionFraction,
                                          .minFractionDigits = 2,
                                          .maxFractionDigits = 2,
                                          .increment = 5}};
    CHECK(Formats("en", &o, "1.23", "1.25") && Formats("en", &o, "1.22", "1.20"),
          "an increment of 5 hundredths");
    o.precision.increment = 50;
    CHECK(Formats("en", &o, "1.26", "1.50"), "of 50 hundredths");
    o.precision.increment = 25;
    CHECK(Formats("en", &o, "1.13", "1.25") && Formats("en", &o, "1.12", "1.00"),
          "of 25 hundredths");
    o = (mlocNumberOptions){
        .precision = {.kind = mloc_precisionFraction, .maxFractionDigits = 0, .increment = 5000}};
    CHECK(Formats("en", &o, "12600", "15,000"), "of 5000 units");
    o = (mlocNumberOptions){.precision = {.kind = mloc_precisionFraction,
                                          .minFractionDigits = 2,
                                          .maxFractionDigits = 2,
                                          .trailingZeros = mloc_trailingZerosStripIfInteger}};
    CHECK(Formats("en", &o, "1.001", "1") && Formats("en", &o, "1.1", "1.10"),
          "an integer drops its zeros, others keep them");
    o = (mlocNumberOptions){.precision = {.trailingZeros = mloc_trailingZerosStripIfInteger}};
    CHECK(Formats("en", &o, "5.000", "5") && Formats("en", &o, "5.50", "5.50"),
          "stripping with no rounding");
    o = (mlocNumberOptions){.minIntegerDigits = 3};
    CHECK(Formats("en", &o, "5.5", "005.5") && Formats("en", &o, "0.25", "000.25"),
          "minimum integer digits");
    o.minIntegerDigits = 5;
    CHECK(Formats("en", &o, "5", "00,005"), "zeros are grouped like digits");
}

static void TestGrouping(void)
{
    mlocNumberOptions o = {.grouping = mloc_groupingAlways};
    CHECK(Formats("es", &o, "1234", "1.234"), "always groups Spanish thousands");
    o.grouping = mloc_groupingMin2;
    CHECK(Formats("en", &o, "1234", "1234") && Formats("en", &o, "12345", "12,345"),
          "min2 needs two digits before the first separator");
    o.grouping = mloc_groupingOff;
    CHECK(Formats("en", &o, "1234567", "1234567"), "off never groups");
}

static void TestSigns(void)
{
    mlocNumberOptions o = {.signDisplay = mloc_signAlways};
    CHECK(Formats("en", &o, "1", "+1") && Formats("en", &o, "0", "+0") &&
              Formats("en", &o, "-0", "-0") && Formats("en", &o, "-1", "-1"),
          "always");
    CHECK(Formats("ar", &o, "1", "\xE2\x80\x8E+1"), "the plus sign's own bidi mark");
    o.signDisplay = mloc_signNever;
    CHECK(Formats("en", &o, "-1", "1"), "never");
    o.signDisplay = mloc_signExceptZero;
    CHECK(Formats("en", &o, "1", "+1") && Formats("en", &o, "0", "0") &&
              Formats("en", &o, "-0", "0") && Formats("en", &o, "-1", "-1"),
          "except zero");
    o.signDisplay = mloc_signNegative;
    CHECK(Formats("en", &o, "-1", "-1") && Formats("en", &o, "-0", "0") &&
              Formats("en", &o, "1", "1"),
          "negative");
    o = (mlocNumberOptions){.signDisplay = mloc_signExceptZero,
                            .precision = {.kind = mloc_precisionFraction}};
    CHECK(Formats("en", &o, "0.4", "0") && Formats("en", &o, "-0.4", "0"),
          "zero is judged after rounding");
    mlocLocale locale;
    mlocNumberFormatter formatter;
    char buffer[32];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocDecimal special = {.kind = mloc_decimalInfinity, .negative = 1};
    o = (mlocNumberOptions){.signDisplay = mloc_signAlways};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitNumberFormatter(&formatter, &s_data, &locale, &o) == mloc_success &&
              mlocFormatNumber(&formatter, &special, &output) == mloc_success &&
              output.length == 4 && memcmp(buffer, "-\xE2\x88\x9E", 4) == 0,
          "negative infinity");
    special = (mlocDecimal){.kind = mloc_decimalNaN};
    CHECK(mlocFormatNumber(&formatter, &special, &output) == mloc_success && output.length == 3 &&
              memcmp(buffer, "NaN", 3) == 0,
          "NaN shows no sign");
}

typedef struct Parts
{
    uint32_t kinds[32];
    size_t starts[32];
    size_t lengths[32];
    int count;
} Parts;

static void Collect(void* context, uint32_t part, size_t start, size_t length)
{
    Parts* p = context;
    if (p->count < 32)
    {
        p->kinds[p->count] = part;
        p->starts[p->count] = start;
        p->lengths[p->count] = length;
        p->count++;
    }
}

static void TestOutput(void)
{
    mlocLocale locale;
    mlocNumberFormatter formatter;
    mlocDecimal decimal;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitNumberFormatter(&formatter, &s_data, &locale, nullptr) == mloc_success &&
              mlocParseDecimal("-1234567.25", 11, &decimal).status == mloc_success,
          "a formatter and a number");
    mlocOutput measure = {0};
    CHECK(mlocFormatNumber(&formatter, &decimal, &measure) == mloc_errorCapacity &&
              measure.length == 13,
          "a null buffer measures");
    char buffer[16];
    mlocOutput shortOne = {.buffer = buffer, .capacity = 12};
    CHECK(mlocFormatNumber(&formatter, &decimal, &shortOne) == mloc_errorCapacity &&
              shortOne.length == 13,
          "a buffer a byte short");
    Parts parts = {0};
    mlocOutput output = {.buffer = buffer, .capacity = 13, .part = Collect, .context = &parts};
    CHECK(mlocFormatNumber(&formatter, &decimal, &output) == mloc_success &&
              memcmp(buffer, "-1,234,567.25", 13) == 0,
          "an exact buffer");
    static const uint32_t s_kinds[] = {mloc_numberPartMinusSign, mloc_numberPartInteger,
                                       mloc_numberPartGroup,     mloc_numberPartInteger,
                                       mloc_numberPartGroup,     mloc_numberPartInteger,
                                       mloc_numberPartDecimal,   mloc_numberPartFraction};
    static const size_t s_starts[] = {0, 1, 2, 3, 6, 7, 10, 11};
    bool same = parts.count == 8;
    for (int i = 0; same && i < 8; i++)
    {
        same = parts.kinds[i] == s_kinds[i] && parts.starts[i] == s_starts[i] &&
               parts.starts[i] + parts.lengths[i] == (i == 7 ? 13 : s_starts[i + 1]);
    }
    CHECK(same, "the parts in order");
    Parts arabic = {0};
    mlocOutput withParts = {
        .buffer = buffer, .capacity = sizeof(buffer), .part = Collect, .context = &arabic};
    mlocDecimal one;
    CHECK(mlocParseLocale("ar", 2, &locale).status == mloc_success &&
              mlocInitNumberFormatter(&formatter, &s_data, &locale, nullptr) == mloc_success &&
              mlocParseDecimal("-1", 2, &one).status == mloc_success &&
              mlocFormatNumber(&formatter, &one, &withParts) == mloc_success && arabic.count == 2 &&
              arabic.kinds[0] == mloc_numberPartMinusSign && arabic.lengths[0] == 4 &&
              arabic.kinds[1] == mloc_numberPartInteger,
          "a minus sign with its bidi mark is one part");
}

static void TestRefusals(void)
{
    mlocLocale locale;
    mlocNumberFormatter formatter;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocInitNumberFormatter(nullptr, &s_data, &locale, nullptr) == mloc_errorInvalid &&
              mlocInitNumberFormatter(&formatter, nullptr, &locale, nullptr) == mloc_errorInvalid &&
              mlocInitNumberFormatter(&formatter, &s_data, nullptr, nullptr) == mloc_errorInvalid,
          "null pointers");
    mlocDataSet empty = {nullptr, 0};
    CHECK(mlocInitNumberFormatter(&formatter, &empty, &locale, nullptr) == mloc_errorNotFound,
          "data without numbers");
    struct
    {
        mlocNumberOptions options;
        const char* name;
    } const s_bad[] = {
        {{.notation = mloc_notationScientific}, "a notation not yet built"},
        {{.notation = (mlocNotation)5}, "no notation"},
        {{.style = mloc_numberPercent}, "a style not yet built"},
        {{.grouping = (mlocGrouping)4}, "no grouping"},
        {{.signDisplay = (mlocSignDisplay)5}, "no sign display"},
        {{.minIntegerDigits = 22}, "22 integer digits"},
        {{.precision = {.kind = (mlocPrecisionKind)5}}, "no precision kind"},
        {{.precision = {.mode = (mlocRoundingMode)9}}, "no rounding mode"},
        {{.precision = {.trailingZeros = (mlocTrailingZeros)2}}, "no trailing zero display"},
        {{.precision = {.maxFractionDigits = 2}}, "fraction digits with no precision"},
        {{.precision = {.maxSignificantDigits = 2}}, "significant digits with no precision"},
        {{.precision = {.kind = mloc_precisionFraction,
                        .minFractionDigits = 3,
                        .maxFractionDigits = 2}},
         "a fraction minimum past its maximum"},
        {{.precision = {.kind = mloc_precisionFraction, .maxFractionDigits = 101}},
         "101 fraction digits"},
        {{.precision = {.kind = mloc_precisionFraction,
                        .maxFractionDigits = 2,
                        .maxSignificantDigits = 2}},
         "significant digits with the fraction kind"},
        {{.precision = {.kind = mloc_precisionSignificant, .maxSignificantDigits = 2}},
         "no minimum significant digits"},
        {{.precision = {.kind = mloc_precisionSignificant,
                        .minSignificantDigits = 1,
                        .maxSignificantDigits = 22}},
         "22 significant digits"},
        {{.precision = {.kind = mloc_precisionSignificant,
                        .minSignificantDigits = 3,
                        .maxSignificantDigits = 2}},
         "a significant minimum past its maximum"},
        {{.precision = {.kind = mloc_precisionSignificant,
                        .minSignificantDigits = 1,
                        .maxSignificantDigits = 2,
                        .maxFractionDigits = 1}},
         "fraction digits with the significant kind"},
        {{.precision = {.increment = 2}}, "an increment with no precision"},
        {{.precision = {.kind = mloc_precisionFraction, .maxFractionDigits = 2, .increment = 5}},
         "an increment with unequal fraction limits"},
        {{.precision = {.kind = mloc_precisionFraction, .increment = 3}}, "an increment of 3"},
        {{.precision = {.kind = mloc_precisionFraction, .increment = 10000}},
         "an increment of 10000"},
        {{.precision = {.kind = mloc_precisionFraction, .increment = 30}}, "an increment of 30"},
        {{.precision = {.kind = mloc_precisionMorePrecision,
                        .minSignificantDigits = 1,
                        .maxSignificantDigits = 2,
                        .increment = 5}},
         "an increment with more precision"},
    };
    for (size_t i = 0; i < sizeof(s_bad) / sizeof(s_bad[0]); i++)
    {
        CHECK(mlocInitNumberFormatter(&formatter, &s_data, &locale, &s_bad[i].options) ==
                  mloc_errorInvalid,
              s_bad[i].name);
    }
    mlocNumberOptions edges = {.minIntegerDigits = 21,
                               .precision = {.kind = mloc_precisionMorePrecision,
                                             .maxFractionDigits = 100,
                                             .minSignificantDigits = 21,
                                             .maxSignificantDigits = 21}};
    CHECK(mlocInitNumberFormatter(&formatter, &s_data, &locale, &edges) == mloc_success,
          "every limit at its largest");
    edges = (mlocNumberOptions){.precision = {.kind = mloc_precisionFraction, .increment = 1}};
    CHECK(mlocInitNumberFormatter(&formatter, &s_data, &locale, &edges) == mloc_success,
          "an increment of 1 is none");
    mlocDecimal decimal = {.count = 1, .digits = {1}};
    char buffer[16];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocInitNumberFormatter(&formatter, &s_data, &locale, nullptr) == mloc_success,
          "a formatter");
    CHECK(mlocFormatNumber(nullptr, &decimal, &output) == mloc_errorInvalid &&
              mlocFormatNumber(&formatter, nullptr, &output) == mloc_errorInvalid &&
              mlocFormatNumber(&formatter, &decimal, nullptr) == mloc_errorInvalid,
          "format refusals");
    mlocOutput badOutput = {.buffer = nullptr, .capacity = 4};
    CHECK(mlocFormatNumber(&formatter, &decimal, &badOutput) == mloc_errorInvalid,
          "a null buffer with room");
    mlocDecimal madeUp = decimal;
    madeUp.digits[0] = 10;
    CHECK(mlocFormatNumber(&formatter, &madeUp, &output) == mloc_errorInvalid,
          "a decimal the library did not make");
    mlocNumberFormatter zero = {0};
    CHECK(mlocFormatNumber(&zero, &decimal, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
    mlocNumberFormatter spoiled = formatter;
    memset((uint8_t*)spoiled.opaque + 128, 0xFF, sizeof(spoiled) - 128);
    CHECK(mlocFormatNumber(&spoiled, &decimal, &output) == mloc_errorInvalid,
          "a spoiled formatter");
    mlocDecimal huge;
    mlocNumberOptions coarse = Significant(1, 1);
    CHECK(mlocParseDecimal("9.9e999999", 10, &huge).status == mloc_success &&
              mlocInitNumberFormatter(&formatter, &s_data, &locale, &coarse) == mloc_success &&
              mlocFormatNumber(&formatter, &huge, &output) == mloc_errorCapacity,
          "rounding past the decimal's limits");
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    TestLocales();
    TestSystems();
    TestPrecision();
    TestGrouping();
    TestSigns();
    TestOutput();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
