// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit formatting with the unit names data file (records mloc-0024 and
// mloc-0025): the widths, the plural categories and their fallbacks,
// patterns without the number, percent and per mille, compound names
// built from the patterns, compact notation, ranges, the parts, the
// display names and the refusals. Every expected text is ICU4C 79's for
// the same input ("generic" aside, which ICU does not take). Then the
// formatter's pool of patterns on names built here.

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
    return mlocInitUnitFormatter(formatter, &s_data, &locale, unit, strlen(unit), options);
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
           mlocFormatUnit(&formatter, &decimal, &output) == mloc_success &&
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
           mlocFormatUnitRange(&formatter, &a, &b, &output) == mloc_success &&
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
           mlocFormatUnit(&formatter, &decimal, &output) == mloc_success;
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
              mlocGetUnitDisplayName(&s_data, &locale, "meter", 5, (mlocUnitWidth)3, &output) ==
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
                      mlocFormatUnit(&special, &nan, &out) == mloc_success &&
                      Same(text, &out, "0 in, NaN ft");
    CHECK(nanWritten && mlocFormatUnit(&special, &infinity, &out) == mloc_success &&
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
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "foot-and-second", 15, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "celsius-and-kelvin", 18,
                                    nullptr) == mloc_errorInvalid &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "foot-and-inch", 13, &compact) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "foot-and-", 9, nullptr) ==
                  mloc_errorSyntax &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "foot-and-foot", 13, nullptr) ==
                  mloc_errorInvalid,
          "parts that do not convert alone, a notation other than standard, a part missing, "
          "parts of one size");
    mlocDecimal one;
    mlocOutput output = {.buffer = buffer, .capacity = 4};
    CHECK(mlocInitUnitFormatter(&formatter, &s_data, &locale, "foot-and-inch", 13, nullptr) ==
                  mloc_success &&
              mlocParseDecimal("1.5", 3, &one).status == mloc_success &&
              mlocFormatUnit(&formatter, &one, &output) == mloc_errorCapacity &&
              output.length == 10 &&
              mlocFormatUnitRange(&formatter, &one, &one, &output) == mloc_errorInvalid,
          "a text that does not fit gives its length; no range of a mixed unit");
}

static void TestRefusals(void)
{
    mlocUnitFormatter formatter;
    mlocLocale locale;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocInitUnitFormatter(&formatter, &s_data, &locale, "Meter", 5, nullptr) ==
                  mloc_errorSyntax &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "pow4-meter", 10, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitUnitFormatter(&formatter, &s_builtin, &locale, "meter", 5, nullptr) ==
                  mloc_errorNotFound,
          "a malformed identifier, a power past three, data without the names");
    CHECK(mlocInitUnitFormatter(&formatter, &s_data, &locale, "curr-usd", 8, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "100-meter", 9, nullptr) ==
                  mloc_errorInvalid,
          "a currency, a constant in the numerator");
    mlocUnitOptions percent = {.number = {.style = mloc_numberPercent}};
    mlocUnitOptions width = {.width = (mlocUnitWidth)3};
    CHECK(mlocInitUnitFormatter(&formatter, &s_data, &locale, "meter", 5, &percent) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "meter", 5, &width) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "meter", 0, nullptr) ==
                  mloc_errorInvalid &&
              mlocInitUnitFormatter(nullptr, &s_data, &locale, "meter", 5, nullptr) ==
                  mloc_errorInvalid,
          "a percent style, a width out of range, an empty unit, a null pointer");
    mlocUnitFormatter forged;
    memset(&forged, 0, sizeof(forged));
    mlocDecimal decimal;
    char buffer[16];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocParseDecimal("1", 1, &decimal).status == mloc_success &&
              mlocFormatUnit(&forged, &decimal, &output) == mloc_errorInvalid &&
              mlocFormatUnitRange(&forged, &decimal, &decimal, &output) == mloc_errorInvalid &&
              mlocFormatUnit(nullptr, &decimal, &output) == mloc_errorInvalid,
          "a formatter the library did not make");
    mlocUnitFormatter made;
    CHECK(mlocInitUnitFormatter(&made, &s_data, &locale, "meter", 5, nullptr) == mloc_success,
          "a formatter");
    UnitFormat* inside = (UnitFormat*)(void*)&made;
    inside->start[1][0] = 2 * unitPool;
    bool unused = mlocFormatUnit(&made, &decimal, &output) == mloc_success;
    inside->start[0][0] = unitPool;
    inside->length[0][0] = 0;
    bool emptyAtEnd = mlocFormatUnit(&made, &decimal, &output) == mloc_success;
    inside->start[0][0] = unitPool + 1;
    CHECK(unused && emptyAtEnd && mlocFormatUnit(&made, &decimal, &output) == mloc_errorInvalid,
          "a pattern's offsets checked within the pool, an unused part's not read");
    output.capacity = 2;
    CHECK(Init(&formatter, "en", "meter", nullptr) == mloc_success &&
              mlocFormatUnit(&formatter, &decimal, &output) == mloc_errorCapacity &&
              output.length == 3,
          "a text that does not fit gives its length");
}

alignas(8) static uint8_t s_built[4096];

// Builds an identifiers unit naming "meter" and root's short names of it,
// a pattern per category zero to other: "{0}" and that many bytes of the
// category's letter ("a" to "f"); opens them as a pack, false when it is
// refused.
static bool OpenNames(mlocData* source, const uint32_t* lengths)
{
    uint8_t* ids = s_built;
    memset(s_built, 0, sizeof(s_built));
    PackPut(ids, 1, 2);
    PackPut(ids + 4, 5, 4);
    PackPut(ids + unitIdsHeaderSize + 4, 5, 4);
    memcpy(ids + unitIdsHeaderSize + 9, "meter", 5);
    uint32_t idsSize = unitIdsHeaderSize + 9 + 5;
    uint8_t* names = s_built + 64;
    enum
    {
        strings = 6,
        rowSize = 1 + 2 * strings,
        head = sparseHeaderSize + 2 * (strings + 1) + 2 + 4 + rowSize,
    };
    uint8_t* offsets = names + sparseHeaderSize;
    uint8_t* row = offsets + 2 * (strings + 1) + 2 + 4;
    uint8_t* text = names + head;
    uint32_t at = 0;
    row[0] = 0x7E;
    for (uint32_t k = 0; k < strings; k++)
    {
        PackPut(offsets + 2 * k, at, 2);
        PackPut(row + 1 + 2 * k, k, 2);
        memcpy(text + at, "{0}", 3);
        memset(text + at + 3, 'a' + (int)k, lengths[k]);
        at += 3 + lengths[k];
    }
    PackPut(offsets + 2 * strings, at, 2);
    PackPut(row - 4, 0, 2);
    PackPut(row - 2, rowSize, 2);
    PackPut(names, strings, 2);
    PackPut(names + 2, 1, 2);
    PackPut(names + 4, at, 4);
    PackPut(names + 8, rowSize, 4);
    PackUnit units[2] = {
        {kindUnitIds, {ids, idsSize}, false},
        {kindUnitNamesShort, {names, head + at}, true},
    };
    return OpenRootPack(units, 2, source);
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
                 mlocInitUnitFormatter(&formatter, &set, &locale, "foot-and-inch", 13, &whole) ==
                     mloc_success &&
                 mlocParseDecimal("1.25", 4, &amount).status == mloc_success &&
                 mlocFormatUnit(&formatter, &amount, &output) == mloc_success &&
                 Same(buffer, &output, "1 ft e in 3");
    CHECK(other && mlocParseDecimal("1.0833", 6, &amount).status == mloc_success &&
              mlocFormatUnit(&formatter, &amount, &output) == mloc_success &&
              Same(buffer, &output, "1 ft y 1 inch"),
          "Spanish's y read against the smallest part's pattern for its category");
}

static void TestPool(void)
{
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    mlocDataSet set = {sources, 2};
    mlocLocale locale;
    mlocUnitFormatter formatter;
    // Arabic takes all six categories; each pattern is marked in one byte
    // for its "{0}".
    static const uint32_t s_fill[6] = {170, 170, 170, 170, 170, 168};
    static const uint32_t s_past[6] = {170, 170, 170, 170, 170, 169};
    static const uint32_t s_room[6] = {1, 1, 1, 1, 1, unitPatternRoom - 3};
    static const uint32_t s_long[6] = {1, 1, 1, 1, 1, unitPatternRoom - 2};
    CHECK(mlocParseLocale("ar", 2, &locale).status == mloc_success, "a locale");
    CHECK(OpenNames(&sources[1], s_fill) &&
              mlocInitUnitFormatter(&formatter, &set, &locale, "meter", 5, nullptr) == mloc_success,
          "patterns that fill the pool");
    CHECK(OpenNames(&sources[1], s_past) &&
              mlocInitUnitFormatter(&formatter, &set, &locale, "meter", 5, nullptr) ==
                  mloc_errorCapacity,
          "patterns a byte past the pool");
    CHECK(OpenNames(&sources[1], s_room) &&
              mlocInitUnitFormatter(&formatter, &set, &locale, "meter", 5, nullptr) == mloc_success,
          "a pattern that fills a pattern's room");
    CHECK(OpenNames(&sources[1], s_long) &&
              mlocInitUnitFormatter(&formatter, &set, &locale, "meter", 5, nullptr) ==
                  mloc_errorCapacity,
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
           mlocOpenData(&s_sources[1], s_names, size) == mloc_success;
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
    TestRanges();
    TestParts();
    TestNames();
    TestRefusals();
    TestPool();
    TestMixedContext();
    return s_failures == 0 ? 0 : 1;
}
