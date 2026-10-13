// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit formatting with the unit names data file (record mloc-0024): the
// widths, the plural categories and their fallbacks, patterns without the
// number, percent and per mille, compact notation, ranges, the parts, the
// display names and the refusals. Every expected text is ICU4C 79's for
// the same input ("generic" aside, which ICU does not take).

#include "test_harness.h"

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

static bool PartsOf(const char* id, const char* unit, const char* amount, char* buffer,
                    size_t capacity, Parts* parts)
{
    mlocUnitFormatter formatter;
    mlocDecimal decimal;
    *parts = (Parts){0};
    mlocOutput output = {.buffer = buffer, .capacity = capacity, .part = Collect, .context = parts};
    return Init(&formatter, id, unit, nullptr) >= 0 &&
           mlocParseDecimal(amount, strlen(amount), &decimal).status == mloc_success &&
           mlocFormatUnit(&formatter, &decimal, &output) == mloc_success;
}

static void TestParts(void)
{
    char buffer[64];
    Parts parts;
    CHECK(PartsOf("en", "meter", "3", buffer, sizeof(buffer), &parts) && parts.count == 3 &&
              parts.kinds[0] == mloc_numberPartInteger &&
              parts.kinds[1] == mloc_numberPartLiteral && parts.lengths[1] == 1 &&
              parts.kinds[2] == mloc_numberPartUnit && parts.starts[2] == 2 &&
              parts.lengths[2] == 1,
          "the unit's name is its own part, the space literal");
    // Persian's zero, a left-to-right mark, a space and "cm" cubed: the
    // mark and the space are literal.
    CHECK(PartsOf("fa", "cubic-centimeter", "0", buffer, sizeof(buffer), &parts) &&
              parts.count == 3 && parts.kinds[1] == mloc_numberPartLiteral &&
              parts.lengths[1] == 4 && parts.kinds[2] == mloc_numberPartUnit &&
              parts.lengths[2] == 4 && memcmp(buffer + parts.starts[2], "cm\xC2\xB3", 4) == 0,
          "bidi controls at the name's ends are literal, as ICU trims them");
    CHECK(PartsOf("en", "percent", "5", buffer, sizeof(buffer), &parts) && parts.count == 2 &&
              parts.kinds[1] == mloc_numberPartPercentSign,
          "percent's sign is the percent part");
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
    mlocLocale locale;
    char buffer[4];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success &&
              mlocGetUnitDisplayName(&s_data, &locale, "meter", 5, mloc_unitWidthLong, &output) ==
                  mloc_errorCapacity &&
              output.length == 6,
          "a name that does not fit gives its length");
    CHECK(mlocGetUnitDisplayName(&s_data, &locale, "per", 3, mloc_unitWidthLong, &output) ==
                  mloc_errorNotFound &&
              mlocGetUnitDisplayName(&s_data, &locale, "furlong-per-fortnight", 21,
                                     mloc_unitWidthLong, &output) == mloc_errorNotFound &&
              mlocGetUnitDisplayName(&s_builtin, &locale, "meter", 5, mloc_unitWidthLong,
                                     &output) == mloc_errorNotFound,
          "a compound key, a unit the names lack, data without the names");
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

static void TestRefusals(void)
{
    mlocUnitFormatter formatter;
    mlocLocale locale;
    CHECK(mlocParseLocale("en", 2, &locale).status == mloc_success, "a locale");
    CHECK(mlocInitUnitFormatter(&formatter, &s_data, &locale, "power2", 6, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitUnitFormatter(&formatter, &s_data, &locale, "Meter", 5, nullptr) ==
                  mloc_errorNotFound &&
              mlocInitUnitFormatter(&formatter, &s_builtin, &locale, "meter", 5, nullptr) ==
                  mloc_errorNotFound,
          "a compound key, a unit the names lack, data without the names");
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
    output.capacity = 2;
    CHECK(Init(&formatter, "en", "meter", nullptr) == mloc_success &&
              mlocFormatUnit(&formatter, &decimal, &output) == mloc_errorCapacity &&
              output.length == 3,
          "a text that does not fit gives its length");
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
    TestRanges();
    TestParts();
    TestNames();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
