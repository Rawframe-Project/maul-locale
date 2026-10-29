// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit formatting with the unit names data file: the widths, the plural
// categories and their fallbacks, patterns without the number, percent and
// per mille, compound names built from the patterns, compact notation,
// ranges, the parts, the display names, mixed units, usages, numeric
// durations, coordinates, grammatical cases and genders, and the refusals.
// Every expected text and gender is ICU4C 79's for the same input
// ("generic" aside, which ICU does not take). Then the formatter's pool of
// patterns on names built here.

#include "currency_tables.h"
#include "kinds.h"
#include "test_harness.h"
#include "test_pack.h"
#include "unit_compound.h"
#include "unit_patterns.h"
#include "unit_tables.h"

#include "maul-locale/unit.h"

#include <stdio.h>
#include <string.h>

#define NBSP    "\xC2\xA0"
#define NNBSP   "\xE2\x80\xAF"
#define EN_DASH "\xE2\x80\x93"

static mlocDataSet s_builtin;
static mlocDataSet s_data;

static mlocResult Init(mlocUnitFormatter* formatter, const char* id, const char* unit,
                       const mlocUnitOptions* options)
{
    mlocLocale locale;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success)
    {
        return (mlocResult)-100;
    }
    return mlocInitUnitFormatter(&s_data, &locale, unit, strlen(unit), options, formatter);
}

static bool Same(const char* buffer, const mlocOutput* output, const char* expected)
{
    return output->length == strlen(expected) && memcmp(buffer, expected, output->length) == 0;
}

// Whether an amount formats as expected in a locale with options.
static bool Formats(const char* id, const char* unit, const mlocUnitOptions* options,
                    const char* amount, const char* expected)
{
    mlocUnitFormatter formatter;
    mlocDecimal decimal;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return Init(&formatter, id, unit, options) >= 0 &&
           mlocParseDecimal(amount, strlen(amount), &decimal).status == mloc_success &&
           mlocUnitFormatter_Format(&formatter, &decimal, &output) == mloc_success &&
           Same(buffer, &output, expected);
}

static bool Width(const char* id, const char* unit, mlocUnitWidth width, const char* amount,
                  const char* expected)
{
    mlocUnitOptions options = {.width = width};
    return Formats(id, unit, &options, amount, expected);
}

static bool Long(const char* id, const char* unit, const char* amount, const char* expected)
{
    return Width(id, unit, mloc_unitWidthLong, amount, expected);
}

static void TestWidths(void)
{
    CHECK(Formats("en", "meter", nullptr, "1", "1 m"), "short by default");
    CHECK(Width("en", "meter", mloc_unitWidthNarrow, "1", "1m"), "narrow");
    CHECK(Long("en", "meter", "1", "1 meter") && Long("en", "meter", "2", "2 meters"),
          "long, by plural category");
    CHECK(Long("en", "meter", "-1", "-1 meter") && Long("en", "meter", "0.5", "0.5 meters"),
          "the category of the number as written");
    CHECK(Long("de", "kilometer-per-hour", "5", "5 Kilometer pro Stunde"), "a compound CLDR names");
    CHECK(Long("fr", "meter", "1.5", "1,5" NBSP "m\xC3\xA8tre"), "French's one for 1.5");
    CHECK(Long("ja", "hour", "3", "3 \xE6\x99\x82\xE9\x96\x93"), "a locale of one category");
    CHECK(Formats("fr", "celsius", nullptr, "-3",
                  "-3" NNBSP "\xC2\xB0"
                  "C"),
          "a temperature");
}

static void TestFallbacks(void)
{
    CHECK(Long("vi", "kilogram", "1", "1 kil\xC3\xB4gam"),
          "a category no locale gives takes other's in its width");
    CHECK(Long("ro", "bar", "1", "1 bar"), "a category marked to inherit takes the short value");
    CHECK(Long("en", "day-person", "2", "2 days"), "a person's duration takes the plain one's");
    CHECK(Formats("en", "dot", nullptr, "3", "3 dots"), "a unit root aliases to another");
    CHECK(Long("en", "foodcalorie", "2", "2 Calories"), "a unit with its own names");
    CHECK(Long("ar", "revolution", "2", "\xD8\xAF\xD9\x88\xD8\xB1\xD8\xAA\xD8\xA7\xD9\x86"),
          "a pattern without the number writes none");
    CHECK(Formats("en", "generic", nullptr, "5", "5\xC2\xB0"), "a unit ICU does not take");
}

static void TestPercent(void)
{
    CHECK(Formats("en", "percent", nullptr, "5", "5%") &&
              Formats("de", "percent", nullptr, "5", "5" NBSP "%"),
          "percent in the locale's percent pattern, unscaled");
    CHECK(Formats("ur", "percent", nullptr, "3", "3\xD9\xAA"),
          "the locale's percent sign, not the unit's");
    CHECK(Formats("en", "permille", nullptr, "5", "5\xE2\x80\xB0"), "per mille");
    CHECK(Long("en", "percent", "5", "5 percent"), "long percent is a unit");
    mlocUnitOptions compact = {.number = {.notation = mloc_notationCompactShort}};
    CHECK(Formats("en", "percent", &compact, "1234", "1.2K%") &&
              Formats("en", "meter", &compact, "1234", "1.2K m"),
          "compact notation within the unit's pattern");
    mlocUnitOptions longCompact = {.number = {.notation = mloc_notationCompactShort},
                                   .width = mloc_unitWidthLong};
    CHECK(Formats("en", "meter", &longCompact, "1000000", "1M meters"),
          "the name in the whole value's category, as ICU4C 79");
}

static void TestCompounds(void)
{
    CHECK(Long("en", "kilometer-per-second", "2", "2 kilometers per second") &&
              Long("fr", "kilometer-per-second", "2", "2\xC2\xA0kilom\xC3\xA8tres par seconde"),
          "a quotient from the per pattern");
    CHECK(Long("ja", "kilometer-per-second", "3",
               "3 \xE3\x82\xAD\xE3\x83\xAD\xE3\x83\xA1\xE3\x83\xBC\xE3\x83\x88\xE3\x83\xAB/"
               "\xE7\xA7\x92"),
          "a quotient from the denominator's per pattern");
    CHECK(Long("en", "per-second-meter", "2", "2 per meter-second"),
          "per takes the rest, normalized");
    CHECK(Width("en", "meter-per-square-second", mloc_unitWidthShort, "3", "3 m/s\xC2\xB2") &&
              Long("en", "kilogram-meter-per-square-second", "2",
                   "2 kilogram-meters per square second"),
          "powers and products");
    CHECK(Long("de", "cubic-centimeter", "2", "2\xC2\xA0Kubikzentimeter") &&
              Long("de", "square-meter", "1", "1 Quadratmeter"),
          "a power lowercases the core unit in long names");
    CHECK(Long("pl", "square-kilometer", "5", "5 kilometr\xC3\xB3w kwadratowych"),
          "a power of the unit's gender, by plural category");
    CHECK(Long("ru", "kilogram-meter", "2",
               "2 \xD0\xBA\xD0\xB8\xD0\xBB\xD0\xBE\xD0\xB3\xD1\x80\xD0\xB0\xD0\xBC\xD0\xBC\xE2\x8B"
               "\x85\xD0\xBC\xD0\xB5\xD1\x82\xD1\x80\xD0\xB0"),
          "a product's derived plural categories");
    CHECK(Long("ar", "gram-meter", "2",
               "\xD8\xBA\xD8\xB1\xD8\xA7\xD9\x85\xE2\x8B\x85\xD9\x85\xD8\xAA\xD8\xB1"),
          "a product of patterns without the number writes none");
    CHECK(Width("lv", "per-100-kilometer", mloc_unitWidthShort, "0", "0/100\xE2\x8B\x85km") &&
              Width("cy", "gram-per-1000-meter", mloc_unitWidthShort, "1", "1 g/1000\xE2\x8B\x85m"),
          "a constant denominator");
    CHECK(Width("ps", "square-millimeter", mloc_unitWidthShort, "1", "\xDB\xB1 mm\xC2\xB2"),
          "a category ICU's data drops (markers alone) takes other's");
    CHECK(Long("de_CH", "jigger", "2", "2 Jigger"),
          "a restored category keeps the value resolved at its locale");
    CHECK(Long("fr", "square-second", "1",
               "1\xC2\xA0seconde carr\xC3\xA9"
               "e"),
          "a power of a feminine unit");
    CHECK(Long("de", "square-millimeter", "2", "2 Quadratmillimeter"),
          "a prefix lowercased under a power in long names");
    CHECK(Long("sw", "square-millimeter", "2", "milimita mraba 2") &&
              Long("sw", "kilometer-per-second", "2", "kilomita 2 kwa kila sekunde"),
          "the placeholder at the end, its joiner kept");
    CHECK(Long("en", "attometer", "2", "2 attometers"), "the first prefix");
}

static bool Range(const char* id, const char* unit, mlocUnitWidth width, const char* start,
                  const char* end, const char* expected)
{
    mlocUnitFormatter formatter;
    mlocUnitOptions options = {.width = width};
    mlocDecimal a;
    mlocDecimal b;
    char buffer[256];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return Init(&formatter, id, unit, &options) >= 0 &&
           mlocParseDecimal(start, strlen(start), &a).status == mloc_success &&
           mlocParseDecimal(end, strlen(end), &b).status == mloc_success &&
           mlocUnitFormatter_FormatRange(&formatter, &a, &b, &output) == mloc_success &&
           Same(buffer, &output, expected);
}

static void TestRanges(void)
{
    CHECK(Range("en", "meter", mloc_unitWidthLong, "3", "5", "3" EN_DASH "5 meters"),
          "the unit once, in the range's category");
    CHECK(Range("gd", "acre", mloc_unitWidthLong, "3", "5", "3" EN_DASH "5 acair"),
          "other where CLDR gives no range");
    CHECK(Range("en", "meter", mloc_unitWidthShort, "2", "2", "~2 m"), "ends that write alike");
    CHECK(Range("en", "meter", mloc_unitWidthShort, "-5", "5", "-5 " EN_DASH " 5 m"),
          "a sign spaces the separator");
    CHECK(Range("en", "celsius", mloc_unitWidthShort, "1", "2",
                "1" EN_DASH "2\xC2\xB0"
                "C"),
          "a unit without a space");
    CHECK(Range("ar", "revolution", mloc_unitWidthLong, "2", "2",
                "\xD8\xAF\xD9\x88\xD8\xB1\xD8\xAA\xD8\xA7\xD9\x86"),
          "a pattern without the number replaces the range");
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
    }
    p->count++;
}

static bool PartsOf(const char* id, const char* unit, mlocUnitWidth width, const char* amount,
                    char* buffer, size_t capacity, Parts* parts)
{
    mlocUnitFormatter formatter;
    mlocUnitOptions options = {.width = width};
    mlocDecimal decimal;
    *parts = (Parts){0};
    mlocOutput output = {.buffer = buffer, .capacity = capacity, .part = Collect, .context = parts};
    return Init(&formatter, id, unit, &options) >= 0 &&
           mlocParseDecimal(amount, strlen(amount), &decimal).status == mloc_success &&
           mlocUnitFormatter_Format(&formatter, &decimal, &output) == mloc_success;
}

static void TestParts(void)
{
    char buffer[64];
    Parts parts;
    CHECK(PartsOf("en", "meter", mloc_unitWidthShort, "3", buffer, sizeof(buffer), &parts) &&
              parts.count == 3 && parts.kinds[0] == mloc_numberPartInteger &&
              parts.kinds[1] == mloc_numberPartLiteral && parts.lengths[1] == 1 &&
              parts.kinds[2] == mloc_numberPartUnit && parts.starts[2] == 2 &&
              parts.lengths[2] == 1,
          "the unit's name is its own part, the space literal");
    // Persian's zero, a left-to-right mark, a space and "cm" cubed: the
    // mark and the space are literal.
    CHECK(PartsOf("fa", "cubic-centimeter", mloc_unitWidthShort, "0", buffer, sizeof(buffer),
                  &parts) &&
              parts.count == 3 && parts.kinds[1] == mloc_numberPartLiteral &&
              parts.lengths[1] == 4 && parts.kinds[2] == mloc_numberPartUnit &&
              parts.lengths[2] == 4 && memcmp(buffer + parts.starts[2], "cm\xC2\xB3", 4) == 0,
          "bidi controls at the name's ends are literal, as ICU trims them");
    CHECK(PartsOf("en", "percent", mloc_unitWidthShort, "5", buffer, sizeof(buffer), &parts) &&
              parts.count == 2 && parts.kinds[1] == mloc_numberPartPercentSign,
          "percent's sign is the percent part");
    CHECK(PartsOf("en", "permille", mloc_unitWidthShort, "5", buffer, sizeof(buffer), &parts) &&
              parts.count == 2 && parts.kinds[1] == mloc_numberPartPerMilleSign,
          "per mille's sign is the per mille part");
    CHECK(PartsOf("ar", "degree", mloc_unitWidthNarrow, "1", buffer, sizeof(buffer), &parts) &&
              parts.count == 4 && parts.kinds[2] == mloc_numberPartUnit && parts.lengths[2] == 2 &&
              parts.kinds[3] == mloc_numberPartLiteral && parts.lengths[3] == 3,
          "a right-to-left mark after the name is literal");
}

static bool Named(const char* id, const char* unit, mlocUnitWidth width, const char* expected)
{
    mlocLocale locale;
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocGetUnitDisplayName(&s_data, &locale, unit, strlen(unit), width, &output) ==
               mloc_success &&
           Same(buffer, &output, expected);
}

static void TestNames(void)
{
    CHECK(Named("en", "meter", mloc_unitWidthLong, "meters") &&
              Named("en", "meter", mloc_unitWidthShort, "m") &&
              Named("de", "kilometer-per-hour", mloc_unitWidthLong, "Kilometer pro Stunde"),
          "display names");
    CHECK(Named("en", "metric-ton", mloc_unitWidthShort, "t"), "a unit by another identifier");
    mlocLocale locale;
    char buffer[4];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocGetUnitDisplayName(&s_data, &locale, "meter", 5, mloc_unitWidthLong, &output) ==
                  mloc_errorCapacity &&
              output.length == 6,
          "a name that does not fit gives its length");
    CHECK(mlocGetUnitDisplayName(&s_data, &locale, "per", 3, mloc_unitWidthLong, &output) ==
                  mloc_errorSyntax &&
              mlocGetUnitDisplayName(&s_data, &locale, "furlong-per-fortnight", 21,
                                     mloc_unitWidthLong, &output) == mloc_errorNotFound &&
              mlocGetUnitDisplayName(&s_builtin, &locale, "meter", 5, mloc_unitWidthLong,
                                     &output) == mloc_errorNotFound,
          "a malformed identifier, a compound CLDR does not name, data without the names");
    CHECK(mlocGetUnitDisplayName(nullptr, &locale, "meter", 5, mloc_unitWidthLong, &output) ==
                  mloc_errorInvalid &&
              mlocGetUnitDisplayName(&s_data, &locale, "meter", 0, mloc_unitWidthLong, &output) ==
                  mloc_errorInvalid &&
              mlocGetUnitDisplayName(&s_data, &locale, "meter", 5, (mlocUnitWidth)4, &output) ==
                  mloc_errorInvalid &&
              mlocGetUnitDisplayName(&s_data, &locale, "meter", 5, mloc_unitWidthLong, nullptr) ==
                  mloc_errorInvalid,
          "a null pointer, an empty unit, a width out of range");
}

static void TestMixed(void)
{
    CHECK(Width("en", "foot-and-inch", mloc_unitWidthShort, "5.5", "5 ft, 6 in") &&
              Long("en", "foot-and-inch", "5.5", "5 feet, 6 inches") &&
              Width("en", "foot-and-inch", mloc_unitWidthNarrow, "-5.25",
                    "-5\xE2\x80\xB2 3\xE2\x80\xB3"),
          "a mixed unit in each width, the sign on the first part");
    CHECK(Long("en", "hour-and-minute-and-second", "1.5083", "1 hour, 30 minutes, 29.88 seconds") &&
              Long("en", "pound-and-ounce", "0.999999", "0 pounds, 15.999984 ounces"),
          "the amount split exactly");
    CHECK(
        Width("en", "inch-and-foot", mloc_unitWidthShort, "5.5", "6 in, 5 ft") &&
            Width("en", "inch-and-foot-and-yard", mloc_unitWidthShort, "2.5", "6 in, 1 ft, 2 yd") &&
            Width("en", "foot-and-yard-and-inch", mloc_unitWidthShort, "2.5", "1 ft, 2 yd, 6 in"),
        "the parts in the order given, the amount in the largest");
    CHECK(Long("es", "foot-and-inch", "1.5", "1 pie y 6 pulgadas") &&
              Long("sw", "foot-and-inch", "2.5", "futi 2 na inchi 6") &&
              Long("ar", "foot-and-inch", "2.5",
                   "2 \xD9\x82\xD8\xAF\xD9\x85 \xD9\x88"
                   "6 \xD8\xA8\xD9\x88\xD8\xB5\xD8\xA9"),
          "the locale's unit list");
    CHECK(
        Width("en", "meter-and-centimeter", mloc_unitWidthShort, "1234.567", "1,234 m, 56.7 cm") &&
            Width("en", "square-foot-and-square-inch", mloc_unitWidthShort, "1.5",
                  "1 sq ft, 72 in\xC2\xB2"),
        "larger parts grouped, built names among the parts");
    mlocUnitOptions whole = {.number.precision = {.kind = mloc_precisionFraction}};
    CHECK(Formats("en", "foot-and-inch", &whole, "5.99", "6 ft, 0 in") &&
              Formats("en", "yard-and-foot-and-inch", &whole, "2.9999", "3 yd, 0 ft, 0 in"),
          "a rounded smallest part that reaches the next carries up");
    mlocUnitOptions options = {.number = {.minIntegerDigits = 2, .grouping = mloc_groupingOff}};
    CHECK(Formats("en", "foot-and-inch", &options, "1234.5", "1,234 ft, 06 in"),
          "the options shape the smallest part alone");
    options = (mlocUnitOptions){.number.signDisplay = mloc_signAlways};
    CHECK(Formats("en", "foot-and-inch", &options, "5.5", "+5 ft, 6 in") &&
              Formats("en", "foot-and-inch", nullptr, "-0.5", "-0 ft, 6 in"),
          "the sign display on the first part, a negative zero kept");
    mlocUnitFormatter special;
    mlocDecimal nan = {.kind = mloc_decimalNaN};
    mlocDecimal infinity = {.kind = mloc_decimalInfinity, .negative = 1};
    char text[32];
    mlocOutput out = {.buffer = text, .capacity = sizeof(text)};
    bool nanWritten = Init(&special, "en", "inch-and-foot", nullptr) == mloc_success &&
                      mlocUnitFormatter_Format(&special, &nan, &out) == mloc_success &&
                      Same(text, &out, "0 in, NaN ft");
    CHECK(nanWritten && mlocUnitFormatter_Format(&special, &infinity, &out) == mloc_success &&
              Same(text, &out, "0 in, -\xE2\x88\x9E ft"),
          "NaN and the infinities in the largest part, with their sign");
    char buffer[64];
    Parts parts;
    CHECK(PartsOf("en", "foot-and-inch", mloc_unitWidthShort, "5.5", buffer, sizeof(buffer),
                  &parts) &&
              parts.count == 7 && parts.kinds[0] == mloc_numberPartInteger &&
              parts.kinds[1] == mloc_numberPartLiteral && parts.kinds[2] == mloc_numberPartUnit &&
              parts.kinds[3] == mloc_numberPartLiteral &&
              parts.kinds[4] == mloc_numberPartInteger &&
              parts.kinds[5] == mloc_numberPartLiteral && parts.kinds[6] == mloc_numberPartUnit,
          "each part's number and name, the list's text literal");
    mlocUnitFormatter formatter;
    mlocLocale locale;
    mlocUnitOptions compact = {.number.notation = mloc_notationCompactShort};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocInitUnitFormatter(&s_data, &locale, "foot-and-second", 15, nullptr, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "celsius-and-kelvin", 18, nullptr,
                                    &formatter) == mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "foot-and-inch", 13, &compact, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "foot-and-", 9, nullptr, &formatter) ==
                  mloc_errorSyntax &&
              mlocInitUnitFormatter(&s_data, &locale, "foot-and-foot", 13, nullptr, &formatter) ==
                  mloc_errorInvalid,
          "parts that do not convert alone, a notation other than standard, a part missing, "
          "parts of one size");
    mlocDecimal one;
    mlocOutput output = {.buffer = buffer, .capacity = 4};
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "foot-and-inch", 13, nullptr, &formatter) ==
                  mloc_success &&
              mlocParseDecimal("1.5", 3, &one).status == mloc_success &&
              mlocUnitFormatter_Format(&formatter, &one, &output) == mloc_errorCapacity &&
              output.length == 10 &&
              mlocUnitFormatter_FormatRange(&formatter, &one, &one, &output) == mloc_errorInvalid,
          "a text that does not fit gives its length; no range of a mixed unit");
}

static bool Used(const char* id, const char* unit, const char* usage, mlocUnitWidth width,
                 const char* amount, const char* expected)
{
    mlocUnitOptions options = {.width = width, .usage = usage, .usageLength = strlen(usage)};
    return Formats(id, unit, &options, amount, expected);
}

static bool UsedRange(const char* id, const char* unit, const char* usage, const char* start,
                      const char* end, const char* expected)
{
    mlocUnitOptions options = {.usage = usage, .usageLength = strlen(usage)};
    mlocUnitFormatter formatter;
    mlocDecimal a;
    mlocDecimal b;
    char buffer[128];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    return Init(&formatter, id, unit, &options) >= 0 &&
           mlocParseDecimal(start, strlen(start), &a).status == mloc_success &&
           mlocParseDecimal(end, strlen(end), &b).status == mloc_success &&
           mlocUnitFormatter_FormatRange(&formatter, &a, &b, &output) == mloc_success &&
           Same(buffer, &output, expected);
}

static void TestUsage(void)
{
    CHECK(Used("en-US", "meter", "road", mloc_unitWidthShort, "0.3", "1 ft") &&
              Used("en-US", "meter", "road", mloc_unitWidthShort, "1000", "0.62 mi") &&
              Used("fr", "meter", "road", mloc_unitWidthLong, "12345.678",
                   "12\xC2\xA0kilom\xC3\xA8tres"),
          "the output unit chosen by the value, rounded as the preference says");
    CHECK(Used("en-US", "meter", "person-height", mloc_unitWidthShort, "1.8", "5 ft, 11 in") &&
              Used("en-GB", "kilogram", "person", mloc_unitWidthShort, "80", "12 st, 8.4 lb"),
          "a mixed output");
    CHECK(Used("en-GB-u-mu-fahrenhe", "celsius", "weather", mloc_unitWidthShort, "20",
               "68\xC2\xB0"
               "F") &&
              Used("en-US-u-ms-metric", "meter", "road", mloc_unitWidthShort, "1000", "1 km"),
          "the -u-mu and -u-ms keys");
    CHECK(Used("en-US", "meter", "nonsense", mloc_unitWidthShort, "5", "16 ft") &&
              Used("en-US", "kilogram-meter", "default", mloc_unitWidthShort, "3",
                   "3 kg\xE2\x8B\x85m"),
          "an unknown usage takes the default, a quantity without preferences its base unit");
    mlocUnitOptions precise = {
        .number.precision = {.kind = mloc_precisionFraction, .maxFractionDigits = 3},
        .usage = "road",
        .usageLength = 4};
    CHECK(Formats("en-US", "meter", &precise, "1000", "0.621 mi"), "a precision given wins");
    CHECK(UsedRange("en-US", "meter", "road", "10", "20",
                    "30\xE2\x80\x93"
                    "70 ft") &&
              UsedRange("en-US", "meter", "road", "10", "2000", "30 ft \xE2\x80\x93 1.2 mi") &&
              UsedRange("en-US", "meter", "road", "100", "100", "~350 ft") &&
              UsedRange("en-US", "meter", "person-height", "1.7", "1.8",
                        "5 ft, 6.9 in \xE2\x80\x93 5 ft, 11 in"),
          "a range in one output collapses; ends in two, or mixed, are written whole");
    mlocUnitFormatter formatter;
    mlocLocale locale;
    mlocUnitOptions bad = {.usage = "Road", .usageLength = 4};
    mlocUnitOptions shortUsage = {.usage = "ro", .usageLength = 2};
    mlocUnitOptions compact = {
        .number.notation = mloc_notationCompactShort, .usage = "road", .usageLength = 4};
    mlocUnitOptions road = {.usage = "road", .usageLength = 4};
    mlocUnitOptions null = {.usageLength = 4};
    CHECK(mlocParseLocale("en-US", 5, &locale).status == mloc_success &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &bad, &formatter) ==
                  mloc_errorSyntax &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &shortUsage, &formatter) ==
                  mloc_errorSyntax &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &compact, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "foot-and-inch", 13, &road, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &null, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_builtin, &locale, "meter", 5, &road, &formatter) ==
                  mloc_errorNotFound,
          "a usage not well-formed, compact notation, a mixed input, a null usage, no names");
    mlocDecimal amount;
    mlocDecimal nan = {.kind = mloc_decimalNaN};
    char buffer[32];
    mlocOutput output = {.buffer = buffer, .capacity = 3};
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &road, &formatter) == mloc_success &&
              mlocParseDecimal("1000", 4, &amount).status == mloc_success &&
              mlocUnitFormatter_Format(&formatter, &amount, &output) == mloc_errorCapacity &&
              output.length == 7 &&
              mlocUnitFormatter_FormatRange(&formatter, &nan, &amount, &output) ==
                  mloc_errorInvalid,
          "a text that does not fit gives its length; a NaN end is refused");
    output.capacity = sizeof(buffer);
    CHECK(mlocUnitFormatter_Format(&formatter, &nan, &output) == mloc_success &&
              Same(buffer, &output, "NaN mi"),
          "NaN in the first output, as the largest value");
    mlocOutput measure = {0};
    CHECK(mlocUnitFormatter_Format(&formatter, &amount, &measure) == mloc_errorCapacity &&
              measure.length == 7,
          "a null buffer measures");
    mlocUnitFormatter forged;
    UnitFormat inside;
    memcpy(&inside, &formatter, sizeof(inside));
    inside.table.simples = nullptr;
    memcpy(&forged, &formatter, sizeof(forged));
    memcpy(&forged, &inside, sizeof(inside));
    CHECK(mlocUnitFormatter_Format(&forged, &amount, &output) == mloc_errorInvalid,
          "a usage formatter without its unit table");
    // A usage of 64 bytes is read (it falls back to the default); one of
    // 65 is refused.
    static const char s_long[] =
        "abcdefgh-abcdefgh-abcdefgh-abcdefgh-abcdefgh-abcdefgh-abcdef-abcd";
    mlocUnitOptions longest = {.usage = s_long, .usageLength = sizeof(s_long) - 2};
    mlocUnitOptions past = {.usage = s_long, .usageLength = sizeof(s_long) - 1};
    char unit[64];
    mlocOutput unitOut = {.buffer = unit, .capacity = sizeof(unit)};
    mlocUnitAmounts amounts;
    CHECK(Formats("en-US", "meter", &longest, "3", "9.8 ft") &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &past, &formatter) ==
                  mloc_errorSyntax &&
              mlocGetUnitPreference(&s_builtin, &locale, "meter", 5, s_long, sizeof(s_long) - 2,
                                    &amount, nullptr, &unitOut, &amounts) == mloc_success &&
              mlocGetUnitPreference(&s_builtin, &locale, "meter", 5, s_long, sizeof(s_long) - 1,
                                    &amount, nullptr, &unitOut, &amounts) == mloc_errorSyntax,
          "the longest usage read");
    CHECK(Used("en-US", "meter-second-kilogram-ampere-kelvin-mole-candela-bit", "default",
               mloc_unitWidthShort, "3",
               "1,806,642,228,000,000,000,000,000 cd\xE2\x8B\x85kg\xE2\x8B\x85m\xE2\x8B\x85sec"
               "\xE2\x8B\x85"
               "A\xE2\x8B\x85K\xE2\x8B\x85item\xE2\x8B\x85"
               "bit"),
          "an input of the most single units");
}

static void TestNumeric(void)
{
    mlocUnitWidth n = mloc_unitWidthNumeric;
    CHECK(Width("en", "hour-and-minute-and-second", n, "1.5083", "1:30:29.88") &&
              Width("en", "hour-and-minute", n, "1.5", "1:30") &&
              Width("en", "minute-and-second", n, "5.15", "5:09") &&
              Width("en", "hour-and-second", n, "1.0025", "1:00:09"),
          "hours, minutes and seconds by the duration patterns, two digits where doubled");
    CHECK(Width("fi", "hour-and-minute-and-second", n, "1.5083", "1.30.29,88") &&
              Width("en", "hour-and-minute", n, "1234.5", "1,234:30"),
          "the locale's patterns and number symbols, larger fields grouped");
    CHECK(Width("en", "hour-and-minute-and-second", n, "-1.5", "-1h 30m 0s") &&
              Width("en", "hour", n, "5", "5h") &&
              Width("en", "foot-and-inch", n, "5.5", "5\xE2\x80\xB2 6\xE2\x80\xB3") &&
              Named("en", "meter", n, "m"),
          "a negative amount, and any other unit, as narrow");
    mlocUnitOptions ungrouped = {.width = n, .number.grouping = mloc_groupingOff};
    CHECK(Formats("en", "hour-and-minute", &ungrouped, "1234.5", "1,234:30") &&
              Formats("en", "minute-and-second", &ungrouped, "1.5", "1:30"),
          "the larger fields in the default format, the options for the smallest");
    mlocUnitOptions whole = {.width = n, .number.precision = {.kind = mloc_precisionFraction}};
    CHECK(Formats("en", "hour-and-minute-and-second", &whole, "1.99999", "2:00:00"),
          "the smallest field rounded by the options, a carry moving up");
}

// Whether an amount formats as expected in a locale, width and case.
static bool InCase(const char* id, const char* unit, mlocUnitWidth width, mlocGrammaticalCase gcase,
                   const char* amount, const char* expected)
{
    mlocUnitOptions options = {.width = width, .grammaticalCase = gcase};
    return Formats(id, unit, &options, amount, expected);
}

static void TestCases(void)
{
    mlocUnitWidth l = mloc_unitWidthLong;
    CHECK(InCase("de", "meter", l, mloc_caseDative, "2", "2 Metern") &&
              InCase("de", "meter", l, mloc_caseDative, "1", "1 Meter") &&
              InCase("ru", "meter", l, mloc_caseGenitive, "5",
                     "5 \xD0\xBC\xD0\xB5\xD1\x82\xD1\x80\xD0\xBE\xD0\xB2") &&
              InCase("ru", "meter", l, mloc_caseInstrumental, "1",
                     "1 \xD0\xBC\xD0\xB5\xD1\x82\xD1\x80\xD0\xBE\xD0\xBC") &&
              InCase("fi", "meter", l, mloc_casePartitive, "2", "2 metri\xC3\xA4"),
          "a unit's patterns of a case, a category the case leaves out the plain one's");
    CHECK(InCase("de", "kilometer-per-hour", l, mloc_caseDative, "5", "5 Kilometern pro Stunde") &&
              InCase("de", "newton-meter", l, mloc_caseDative, "2", "2 Newtonmetern") &&
              InCase("de", "hectometer", l, mloc_caseDative, "2", "2 Hektometern") &&
              InCase("de", "hectometer", l, mloc_caseDative, "1", "1 Hektometer") &&
              InCase("de", "hectosecond", l, mloc_caseDative, "1", "1 Hektosekunde") &&
              InCase("pl", "kilometer-per-hour", l, mloc_caseGenitive, "5",
                     "5 kilometr\xC3\xB3w na godzin\xC4\x99"),
          "compound names: the case through per and times, a prefixed core lowercased in it");
    CHECK(InCase("cs", "cubic-decimeter", l, mloc_caseDative, "1", "1 decimetr krychlov\xC3\xA9mu"),
          "a power's pattern of the unit's gender and the case");
    CHECK(InCase("de", "meter-and-centimeter", l, mloc_caseDative, "2.5",
                 "2 Metern, 50 Zentimetern") &&
              InCase("de-CH", "foot", l, mloc_caseDative, "2", "2 Fu\xC3\x9F") &&
              InCase("de-CH", "foot", l, mloc_caseGenitive, "2", "2 Fuss"),
          "a mixed unit's parts; a locale's chain as ICU reads it");
    CHECK(InCase("de", "meter", mloc_unitWidthShort, mloc_caseDative, "2", "2 m") &&
              InCase("de", "meter", l, mloc_caseVocative, "2", "2 Meter") &&
              InCase("en", "meter", l, mloc_caseDative, "2", "2 meters"),
          "no case in short; a case the locale lacks is the nominative");
    mlocUnitOptions road = {
        .width = l, .usage = "road", .usageLength = 4, .grammaticalCase = mloc_caseDative};
    CHECK(Formats("de", "meter", &road, "2000", "2 Kilometern"), "a usage's output in the case");
    mlocUnitFormatter formatter;
    mlocUnitOptions past = {.width = l, .grammaticalCase = (mlocGrammaticalCase)28};
    mlocUnitOptions dative = {.width = l, .grammaticalCase = mloc_caseDative};
    CHECK(Init(&formatter, "de", "meter", &past) == mloc_errorInvalid, "a case out of range");
    mlocLocale locale;
    CHECK(mlocParseLocale("de", 2, &locale).status == mloc_success &&
              mlocInitUnitFormatter(&s_builtin, &locale, "meter", 5, &dative, &formatter) ==
                  mloc_errorNotFound,
          "data without the case patterns");
}

// Whether a unit's gender in a locale is as expected.
static bool GenderIs(const char* id, const char* unit, mlocGender expected)
{
    mlocLocale locale;
    mlocGender gender = (mlocGender)99;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocGetUnitGender(&s_data, &locale, unit, strlen(unit), &gender) == mloc_success &&
           gender == expected;
}

static void TestGenders(void)
{
    CHECK(GenderIs("de", "meter", mloc_genderMasculine) &&
              GenderIs("cs", "cubic-decimeter", mloc_genderInanimate) &&
              GenderIs("de", "generic", mloc_genderNeuter),
          "a unit CLDR names: its long name's gender");
    CHECK(GenderIs("de", "square-yard", mloc_genderNeuter) &&
              GenderIs("de", "pixel-per-inch", mloc_genderNeuter) &&
              GenderIs("de", "dunam", mloc_genderNone),
          "one CLDR names without a gender: its parts' through the derivations");
    CHECK(GenderIs("de", "newton-second", mloc_genderFeminine) &&
              GenderIs("de", "second-newton", mloc_genderFeminine) &&
              GenderIs("fr", "newton-second", mloc_genderMasculine) &&
              GenderIs("de", "kilocalorie-per-gram", mloc_genderFeminine) &&
              GenderIs("fr", "square-second", mloc_genderFeminine) &&
              GenderIs("de", "hectoliter", mloc_genderMasculine),
          "built names: German's product takes its last unit's, French's its first, per the "
          "numerator's, a power and a prefix the unit's");
    CHECK(GenderIs("de", "per-second", mloc_genderNone) &&
              GenderIs("de", "foot-and-inch", mloc_genderNone) &&
              GenderIs("en", "meter", mloc_genderNone),
          "none: no numerator, a mixed unit, a language without genders");
    mlocLocale de;
    mlocGender gender = mloc_genderNeuter;
    CHECK(mlocParseLocale("de", 2, &de).status == mloc_success &&
              mlocGetUnitGender(&s_data, &de, "Meter", 5, &gender) == mloc_errorSyntax &&
              mlocGetUnitGender(&s_data, &de, "furlong-x", 9, &gender) == mloc_errorNotFound &&
              mlocGetUnitGender(&s_builtin, &de, "meter", 5, &gender) == mloc_errorNotFound &&
              mlocGetUnitGender(&s_data, &de, "meter", 0, &gender) == mloc_errorInvalid &&
              mlocGetUnitGender(nullptr, &de, "meter", 5, &gender) == mloc_errorInvalid &&
              mlocGetUnitGender(&s_data, &de, "meter", 5, nullptr) == mloc_errorInvalid &&
              gender == mloc_genderNeuter,
          "refusals, the gender left unchanged");
}

// A set whose first source is a pack of one derivations unit, of German
// (per and times: the first, power: feminine, prefix: common), French
// (per: feminine, no other rules) and Italian (per: the second, times:
// common, power: the unit's, prefix: masculine), before the names.
static bool OpenGenderRules(mlocData* sources)
{
    static uint8_t s_unit[unitDerivationsHeaderSize + 3 * unitDerivationSize];
    static const char* const s_languages[3] = {"de", "fr", "it"};
    static const uint8_t s_rules[3][unitDeriveStructures] = {
        {unitGenderFirst, unitGenderFirst, mloc_genderFeminine, mloc_genderCommon},
        {mloc_genderFeminine, unitDerivedNone, unitDerivedNone, unitDerivedNone},
        {unitGenderSecond, mloc_genderCommon, unitGenderFirst, mloc_genderMasculine},
    };
    memset(s_unit, 0, sizeof(s_unit));
    s_unit[0] = 3;
    for (int i = 0; i < 3; i++)
    {
        uint8_t* record = s_unit + unitDerivationsHeaderSize + i * unitDerivationSize;
        memcpy(record, s_languages[i], 2);
        memset(record + unitDerivationPlural, unitDerivedNone,
               unitDerivationSize - unitDerivationPlural);
        memcpy(record + unitDerivationGender, s_rules[i], unitDeriveStructures);
    }
    PackUnit unit = {kindUnitDerivations, {s_unit, sizeof(s_unit)}, false};
    sources[1] = s_data.sources[0];
    sources[2] = s_data.sources[1];
    return OpenRootPack(&unit, 1, &sources[0]);
}

// Whether a unit's gender under the crafted rules is as expected.
static bool RuledGender(const mlocDataSet* set, const char* id, const char* unit,
                        mlocGender expected)
{
    mlocLocale locale;
    mlocGender gender = (mlocGender)99;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocGetUnitGender(set, &locale, unit, strlen(unit), &gender) == mloc_success &&
           gender == expected;
}

static void TestGenderRules(void)
{
    mlocData sources[3];
    CHECK(OpenGenderRules(sources), "a pack of crafted gender rules");
    mlocDataSet set = {sources, 3};
    CHECK(RuledGender(&set, "de", "newton-second", mloc_genderNeuter) &&
              RuledGender(&set, "it", "liter-per-hour", mloc_genderFeminine) &&
              RuledGender(&set, "de", "attoliter", mloc_genderCommon) &&
              RuledGender(&set, "de", "square-newton", mloc_genderFeminine),
          "built names: the times and per rules' sides, a fixed prefix's and power's gender");
    CHECK(RuledGender(&set, "it", "pixel-per-inch", mloc_genderMasculine) &&
              RuledGender(&set, "de", "pound-force-foot", mloc_genderNone) &&
              RuledGender(&set, "de", "square-yard", mloc_genderFeminine) &&
              RuledGender(&set, "it", "square-yard", mloc_genderMasculine),
          "names without a gender: the rules top down, a fixed prefix beside a power");
    CHECK(RuledGender(&set, "fr", "newton-second", mloc_genderFeminine) &&
              RuledGender(&set, "fr", "pixel-per-inch", mloc_genderFeminine) &&
              RuledGender(&set, "fr", "liter-per-second", mloc_genderFeminine),
          "a fixed per rule");
    CHECK(RuledGender(&set, "fr", "square-yard", mloc_genderNone) &&
              RuledGender(&set, "fr", "pound-force-foot", mloc_genderNone),
          "no rule: no gender");
}

static void TestCoordinate(void)
{
    mlocUnitOptions lat = {.width = mloc_unitWidthNarrow, .coordinate = mloc_coordinateLatitude};
    mlocUnitOptions lon = {.width = mloc_unitWidthLong, .coordinate = mloc_coordinateLongitude};
    CHECK(Formats("en", "degree", &lat, "12.5", "12.5\xC2\xB0N") &&
              Formats("en", "degree", &lat, "-12.5", "12.5\xC2\xB0S") &&
              Formats("en", "degree", &lat, "0", "0\xC2\xB0N") &&
              Formats("en", "degree", &lon, "-77.03", "77.03 degrees west") &&
              Formats("fi", "degree", &lat, "60.17", "60,17\xC2\xB0P"),
          "the direction by the sign, the amount unsigned");
    CHECK(Formats("en", "degree-and-arc-minute-and-arc-second", &lat, "40.446111",
                  "40\xC2\xB0 26\xE2\x80\xB2 45.9996\xE2\x80\xB3N"),
          "a mixed angle");
    mlocUnitOptions always = {.number.signDisplay = mloc_signAlways,
                              .coordinate = mloc_coordinateLongitude};
    mlocUnitOptions numeric = {.width = mloc_unitWidthNumeric,
                               .coordinate = mloc_coordinateLatitude};
    CHECK(Formats("en", "degree", &always, "8", "8 deg E") &&
              Formats("en", "degree", &numeric, "12.5", "12.5\xC2\xB0N"),
          "no sign written; the numeric width takes the narrow directions");
    mlocUnitFormatter formatter;
    mlocLocale locale;
    mlocDecimal nan = {.kind = mloc_decimalNaN};
    mlocDecimal south = {.kind = mloc_decimalInfinity, .negative = 1};
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    bool nanNorth =
        mlocParseLocale("en", 2, &locale).status == mloc_success &&
        mlocInitUnitFormatter(&s_data, &locale, "degree", 6, &lat, &formatter) == mloc_success &&
        mlocUnitFormatter_Format(&formatter, &nan, &output) == mloc_success &&
        Same(buffer, &output, "NaN\xC2\xB0N");
    CHECK(nanNorth && mlocUnitFormatter_Format(&formatter, &south, &output) == mloc_success &&
              Same(buffer, &output, "\xE2\x88\x9E\xC2\xB0S") &&
              mlocUnitFormatter_FormatRange(&formatter, &nan, &nan, &output) == mloc_errorInvalid,
          "NaN north, an infinity by its sign; no range");
    mlocUnitOptions road = {
        .usage = "road", .usageLength = 4, .coordinate = mloc_coordinateLatitude};
    mlocUnitOptions bad = {.coordinate = (mlocCoordinate)3};
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &lat, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "degree", 6, &road, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "degree", 6, &bad, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_builtin, &locale, "degree", 6, &lat, &formatter) ==
                  mloc_errorNotFound,
          "a unit not an angle, a usage, a coordinate out of range, data without the names");
}

static void TestRefusals(void)
{
    mlocUnitFormatter formatter;
    mlocLocale locale;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "Meter", 5, nullptr, &formatter) ==
                  mloc_errorSyntax &&
              mlocInitUnitFormatter(&s_data, &locale, "pow4-meter", 10, nullptr, &formatter) ==
                  mloc_errorNotFound &&
              mlocInitUnitFormatter(&s_builtin, &locale, "meter", 5, nullptr, &formatter) ==
                  mloc_errorNotFound,
          "a malformed identifier, a power past three, data without the names");
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "curr-usd", 8, nullptr, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "100-meter", 9, nullptr, &formatter) ==
                  mloc_errorInvalid,
          "a currency, a constant in the numerator");
    mlocUnitOptions percent = {.number = {.style = mloc_numberPercent}};
    mlocUnitOptions width = {.width = (mlocUnitWidth)4};
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &percent, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, &width, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 0, nullptr, &formatter) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&s_data, &locale, "meter", 5, nullptr, nullptr) ==
                  mloc_errorInvalid,
          "a percent style, a width out of range, an empty unit, a null pointer");
    mlocUnitFormatter forged;
    memset(&forged, 0, sizeof(forged));
    mlocDecimal decimal;
    char buffer[16];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocParseDecimal("1", 1, &decimal).status == mloc_success &&
              mlocUnitFormatter_Format(&forged, &decimal, &output) == mloc_errorInvalid &&
              mlocUnitFormatter_FormatRange(&forged, &decimal, &decimal, &output) ==
                  mloc_errorInvalid &&
              mlocUnitFormatter_Format(nullptr, &decimal, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
    mlocUnitFormatter made;
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "meter", 5, nullptr, &made) == mloc_success,
          "a formatter");
    UnitFormat inside;
    memcpy(&inside, &made, sizeof(inside));
    inside.start[1][0] = 2 * unitPool;
    memcpy(&made, &inside, sizeof(inside));
    bool unused = mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_success;
    inside.start[0][0] = unitPool;
    inside.length[0][0] = 0;
    memcpy(&made, &inside, sizeof(inside));
    bool emptyAtEnd = mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_success;
    inside.start[0][0] = unitPool + 1;
    memcpy(&made, &inside, sizeof(inside));
    CHECK(unused && emptyAtEnd &&
              mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_errorInvalid,
          "a pattern's offsets checked within the pool, an unused part's not read");
    inside.start[0][0] = 0;
    inside.outputs = 0;
    memcpy(&made, &inside, sizeof(inside));
    bool none = mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_errorInvalid;
    inside.outputs = unitOutputs + 1;
    memcpy(&made, &inside, sizeof(inside));
    CHECK(none && mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_errorInvalid,
          "the output units counted within the formatter's");
    output.capacity = 2;
    CHECK(Init(&formatter, "en", "meter", nullptr) == mloc_success &&
              mlocUnitFormatter_Format(&formatter, &decimal, &output) == mloc_errorCapacity &&
              output.length == 3,
          "a text that does not fit gives its length");
    mlocUnitOptions numeric = {.width = mloc_unitWidthNumeric};
    output.capacity = sizeof(buffer);
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "hour-and-minute", 15, &numeric, &made) ==
                  mloc_success &&
              mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_success &&
              Same(buffer, &output, "1:00"),
          "a numeric formatter");
    memcpy(&inside, &made, sizeof(inside));
    struct
    {
        int8_t field;
        uint8_t numeric;
        bool duration;
    } numerics[] = {{2, 1, true}, {0, unitPatternHms + 2, true}, {0, 1, false}};
    for (size_t i = 0; i < sizeof(numerics) / sizeof(numerics[0]); i++)
    {
        UnitFormat spoiled = inside;
        spoiled.output[0].field[1] = numerics[i].field != 0 ? numerics[i].field : 1;
        spoiled.output[0].numeric = numerics[i].numeric;
        spoiled.duration[0] = numerics[i].duration ? spoiled.duration[0] : nullptr;
        memcpy(&made, &spoiled, sizeof(spoiled));
        CHECK(mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_errorInvalid,
              "a numeric field past the parts, a pattern past the three, a pattern missing");
    }
    mlocUnitOptions lat = {.coordinate = mloc_coordinateLatitude};
    CHECK(mlocInitUnitFormatter(&s_data, &locale, "degree", 6, &lat, &made) == mloc_success,
          "a coordinate formatter");
    memcpy(&inside, &made, sizeof(inside));
    UnitFormat spoiled = inside;
    spoiled.coordinate = mloc_coordinateLongitude + 1;
    memcpy(&made, &spoiled, sizeof(spoiled));
    bool past = mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_errorInvalid;
    spoiled = inside;
    spoiled.direction[3] = nullptr;
    memcpy(&made, &spoiled, sizeof(spoiled));
    CHECK(past && mlocUnitFormatter_Format(&made, &decimal, &output) == mloc_errorInvalid,
          "a coordinate past longitude, a direction missing");
}

alignas(8) static uint8_t s_built[8192];

// Builds an identifiers unit naming units (sorted) and root's short names
// of them, a pattern per category zero to other: "{0}" and that many
// bytes of the category's letter ("a" to "f"); opens them as a pack,
// false when it is refused.
static bool OpenUnitNames(mlocData* source, const char* const* units, uint32_t count,
                          const uint32_t (*lengths)[6])
{
    uint8_t* ids = s_built;
    memset(s_built, 0, sizeof(s_built));
    uint32_t idText = 0;
    for (uint32_t u = 0; u < count; u++)
    {
        idText += (uint32_t)strlen(units[u]);
    }
    PackPut(ids, count, 2);
    PackPut(ids + 4, idText, 4);
    uint8_t* idOffsets = ids + unitIdsHeaderSize;
    uint8_t* idChars = idOffsets + 4 * (count + 1) + count;
    for (uint32_t u = 0, at = 0; u < count; u++)
    {
        memcpy(idChars + at, units[u], strlen(units[u]));
        at += (uint32_t)strlen(units[u]);
        PackPut(idOffsets + 4 * (u + 1), at, 4);
    }
    uint32_t idsSize = (uint32_t)(idChars + idText - ids);
    uint8_t* names = s_built + ((idsSize + 7) / 8 * 8);
    uint32_t strings = 6 * count;
    uint32_t rowSize = 1 + 2 * 6;
    uint8_t* offsets = names + sparseHeaderSize;
    uint8_t* codes = offsets + 2 * (strings + 1);
    uint8_t* rowOffsets = codes + 2 * count;
    uint8_t* rows = rowOffsets + 2 * (count + 1);
    uint8_t* text = rows + rowSize * count;
    uint32_t at = 0;
    for (uint32_t u = 0; u < count; u++)
    {
        PackPut(codes + 2 * u, u, 2);
        PackPut(rowOffsets + 2 * (u + 1), rowSize * (u + 1), 2);
        uint8_t* row = rows + rowSize * u;
        row[0] = 0x7E;
        for (uint32_t k = 0; k < 6; k++)
        {
            uint32_t string = 6 * u + k;
            PackPut(offsets + 2 * string, at, 2);
            PackPut(row + 1 + 2 * k, string, 2);
            memcpy(text + at, "{0}", 3);
            memset(text + at + 3, 'a' + (int)k, lengths[u][k]);
            at += 3 + lengths[u][k];
        }
    }
    PackPut(offsets + 2 * strings, at, 2);
    PackPut(names, strings, 2);
    PackPut(names + 2, count, 2);
    PackPut(names + 4, at, 4);
    PackPut(names + 8, rowSize * count, 4);
    PackUnit packed[2] = {
        {kindUnitIds, {ids, idsSize}, false},
        {kindUnitNamesShort, {names, (uint32_t)(text + at - names)}, true},
    };
    return OpenRootPack(packed, 2, source);
}

static bool OpenNames(mlocData* source, const uint32_t* lengths)
{
    static const char* const s_meter[1] = {"meter"};
    return OpenUnitNames(source, s_meter, 1, (const uint32_t (*)[6])lengths);
}

// Builds an identifiers unit naming "foot" and "inch" and root's short
// names of them: "{0} ft" for every category of the foot, "{0} inch" for
// one and "in {0}" for the others of the inch; opens them as a pack.
static bool OpenFootAndInch(mlocData* source)
{
    static uint8_t s_units[512];
    memset(s_units, 0, sizeof(s_units));
    uint8_t* ids = s_units;
    PackPut(ids, 2, 2);
    PackPut(ids + 4, 8, 4);
    PackPut(ids + unitIdsHeaderSize + 4, 4, 4);
    PackPut(ids + unitIdsHeaderSize + 8, 8, 4);
    memcpy(ids + unitIdsHeaderSize + 14, "footinch", 8);
    uint32_t idsSize = unitIdsHeaderSize + 14 + 8;
    static const char* const s_strings[3] = {"{0} ft", "{0} inch", "in {0}"};
    uint8_t* names = s_units + 64;
    uint8_t* offsets = names + sparseHeaderSize;
    uint8_t* codes = offsets + 2 * 4;
    uint8_t* rowOffsets = codes + 2 * 2;
    uint8_t* rows = rowOffsets + 2 * 3;
    uint8_t* text = rows + 8;
    uint32_t at = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        PackPut(offsets + 2 * i, at, 2);
        memcpy(text + at, s_strings[i], strlen(s_strings[i]));
        at += (uint32_t)strlen(s_strings[i]);
    }
    PackPut(offsets + 2 * 3, at, 2);
    PackPut(codes + 2, 1, 2);
    PackPut(rowOffsets + 2, 3, 2);
    PackPut(rowOffsets + 4, 8, 2);
    rows[0] = 1u << unitNameOther;
    rows[3] = 1u << (unitNameZero + 1) | 1u << unitNameOther;
    PackPut(rows + 4, 1, 2);
    PackPut(rows + 6, 2, 2);
    PackPut(names, 3, 2);
    PackPut(names + 2, 2, 2);
    PackPut(names + 4, at, 4);
    PackPut(names + 8, 8, 4);
    PackUnit units[2] = {
        {kindUnitIds, {ids, idsSize}, false},
        {kindUnitNamesShort, {names, (uint32_t)(text + at - names)}, true},
    };
    return OpenRootPack(units, 2, source);
}

static void TestMixedContext(void)
{
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    mlocDataSet set = {sources, 2};
    mlocLocale locale;
    mlocUnitFormatter formatter;
    mlocUnitOptions whole = {.number.precision = {.kind = mloc_precisionFraction}};
    mlocDecimal amount;
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    bool other = OpenFootAndInch(&sources[1]) &&
                 mlocParseLocale("es", 2, &locale).status == mloc_success &&
                 mlocInitUnitFormatter(&set, &locale, "foot-and-inch", 13, &whole, &formatter) ==
                     mloc_success &&
                 mlocParseDecimal("1.25", 4, &amount).status == mloc_success &&
                 mlocUnitFormatter_Format(&formatter, &amount, &output) == mloc_success &&
                 Same(buffer, &output, "1 ft e in 3");
    CHECK(other && mlocParseDecimal("1.0833", 6, &amount).status == mloc_success &&
              mlocUnitFormatter_Format(&formatter, &amount, &output) == mloc_success &&
              Same(buffer, &output, "1 ft y 1 inch"),
          "Spanish's y read against the smallest part's pattern for its category");
}

// A formatter is the same value whatever its bytes held before: the room
// the patterns leave in the pool is written as zero. A refusal leaves
// the bytes as they were.
static void TestWholeValue(void)
{
    static const mlocUnitOptions s_options[] = {
        {.width = mloc_unitWidthLong},
        {.width = mloc_unitWidthShort},
        {.usage = "road", .usageLength = 4},
        {.coordinate = mloc_coordinateLatitude},
    };
    static const char* const s_units[] = {"meter", "foot-and-inch", "meter", "degree"};
    static mlocUnitFormatter s_zeros;
    static mlocUnitFormatter s_ones;
    static mlocUnitFormatter s_before;
    bool same = true;
    for (size_t i = 0; i < sizeof(s_units) / sizeof(s_units[0]); i++)
    {
        memset(&s_zeros, 0, sizeof(s_zeros));
        memset(&s_ones, 0xFF, sizeof(s_ones));
        same = same && Init(&s_zeros, "en", s_units[i], &s_options[i]) == mloc_success &&
               Init(&s_ones, "en", s_units[i], &s_options[i]) == mloc_success &&
               memcmp(&s_zeros, &s_ones, sizeof(s_zeros)) == 0;
    }
    CHECK(same, "a formatter whatever its bytes held");
    memcpy(&s_before, &s_ones, sizeof(s_before));
    CHECK(Init(&s_ones, "en", "meter-per-meter-per", nullptr) < 0 &&
              memcmp(&s_before, &s_ones, sizeof(s_before)) == 0,
          "a refusal leaves the bytes");
}

static void TestPool(void)
{
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    mlocDataSet set = {sources, 2};
    mlocLocale locale;
    mlocUnitFormatter formatter;
    // Arabic takes all six categories; each pattern is marked in one byte
    // for its "{0}". Four parts of six patterns fill the pool.
    static const char* const s_parts[4] = {"foot", "inch", "mile", "yard"};
    static uint32_t s_fill[4][6];
    for (uint32_t i = 0; i < 24; i++)
    {
        s_fill[i / 6][i % 6] = (unitPool - 24) / 24 + (i < (unitPool - 24) % 24);
    }
    static const char s_mixed[] = "mile-and-yard-and-foot-and-inch";
    static const uint32_t s_room[6] = {1, 1, 1, 1, 1, unitPatternRoom - 3};
    static const uint32_t s_long[6] = {1, 1, 1, 1, 1, unitPatternRoom - 2};
    CHECK(mlocParseLocale("ar", 2, &locale).status == mloc_success, "a locale");
    CHECK(OpenUnitNames(&sources[1], s_parts, 4, s_fill) &&
              mlocInitUnitFormatter(&set, &locale, s_mixed, sizeof(s_mixed) - 1, nullptr,
                                    &formatter) == mloc_success,
          "patterns that fill the pool");
    s_fill[3][5]++;
    CHECK(OpenUnitNames(&sources[1], s_parts, 4, s_fill) &&
              mlocInitUnitFormatter(&set, &locale, s_mixed, sizeof(s_mixed) - 1, nullptr,
                                    &formatter) == mloc_errorLimit,
          "patterns a byte past the pool");
    CHECK(OpenNames(&sources[1], s_room) &&
              mlocInitUnitFormatter(&set, &locale, "meter", 5, nullptr, &formatter) == mloc_success,
          "a pattern that fills a pattern's room");
    CHECK(OpenNames(&sources[1], s_long) &&
              mlocInitUnitFormatter(&set, &locale, "meter", 5, nullptr, &formatter) ==
                  mloc_errorLimit,
          "a pattern a byte past a pattern's room");
}

// The compiled-in data, and with it the unit names data file, at the
// path the build gives.
static bool OpenData(void)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_names[8 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_builtin = (mlocDataSet){s_sources, 1};
    FILE* file = fopen(MLOC_UNITS_FILE, "rb");
    size_t size = file == nullptr ? 0 : fread(s_names, 1, sizeof(s_names), file);
    if (file != nullptr)
    {
        fclose(file);
    }
    s_data = (mlocDataSet){s_sources, 2};
    return size > 0 && size < sizeof(s_names) &&
           mlocOpenData(s_names, size, &s_sources[1]) == mloc_success;
}

int main(void)
{
    if (!OpenData())
    {
        fputs("test_unit_format: cannot open " MLOC_UNITS_FILE "\n", stderr);
        return 2;
    }
    TestWidths();
    TestFallbacks();
    TestPercent();
    TestCompounds();
    TestMixed();
    TestUsage();
    TestNumeric();
    TestCoordinate();
    TestCases();
    TestGenders();
    TestGenderRules();
    TestRanges();
    TestParts();
    TestNames();
    TestRefusals();
    TestWholeValue();
    TestPool();
    TestMixedContext();
    return s_failures == 0 ? 0 : 1;
}
