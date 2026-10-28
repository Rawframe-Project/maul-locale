// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for units, the units file beside the compiled-in data.
// The first byte chooses a locale, the next five the width, the case, the
// coordinate, the usage and the unit to convert to, the next the length of
// a decimal's text that follows; the rest is a unit identifier. Every
// unit call gives one of the results its documentation lists. A value
// converted to its own unit comes back as it was (but by the Beaufort
// table, which rounds to a force); a formatter writes the same text twice
// and measures its length.

#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/unit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    headBytes = 7,
    maxValueText = 64,
    maxText = 4096,
};

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static bool OneOf(mlocResult r, const mlocResult* allowed, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (r == allowed[i])
        {
            return true;
        }
    }
    return false;
}

#define ONE_OF(r, ...)                                                                             \
    OneOf((r), (const mlocResult[]){__VA_ARGS__},                                                  \
          sizeof((const mlocResult[]){__VA_ARGS__}) / sizeof(mlocResult))

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// The compiled-in data and the units file; false when the file does not
// open.
static bool OpenData(mlocDataSet* set)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_units[8 << 20];
    static int s_state;
    if (s_state == 0)
    {
        s_state = -1;
        mlocOpenBuiltinData(&s_sources[0]);
        FILE* file = fopen(MLOC_UNITS_FILE, "rb");
        if (file != nullptr)
        {
            size_t size = fread(s_units, 1, sizeof(s_units), file);
            fclose(file);
            if (size > 0 && size < sizeof(s_units) &&
                mlocOpenData(&s_sources[1], s_units, size) == mloc_success)
            {
                s_state = 1;
            }
        }
    }
    *set = (mlocDataSet){s_sources, 2};
    return s_state == 1;
}

// The decimal of a text, else of the text's length, so that every input
// has a value.
static mlocDecimal ValueOf(const uint8_t* text, size_t length)
{
    mlocDecimal d;
    if (mlocParseDecimal((const char*)text, length, &d).status != mloc_success)
    {
        Require(mlocMakeDecimal((int64_t)length - 32, &d) == mloc_success);
    }
    return d;
}

static bool SameValue(const mlocDecimal* a, const mlocDecimal* b)
{
    return a->kind == b->kind && a->negative == b->negative && a->count == b->count &&
           a->exponent == b->exponent && memcmp(a->digits, b->digits, a->count) == 0;
}

static void Convert(const mlocDataSet* set, const char* unit, size_t length, const char* to,
                    const mlocDecimal* value)
{
    mlocDecimal out;
    mlocResult r = mlocConvertUnit(set, unit, length, to, strlen(to), nullptr, value, &out);
    Require(ONE_OF(r, mloc_success, mloc_errorSyntax, mloc_errorNotFound, mloc_errorInvalid,
                   mloc_errorLimit));
    // To its own unit, a nonzero finite value is exact and comes back.
    r = mlocConvertUnit(set, unit, length, unit, length, nullptr, value, &out);
    Require(ONE_OF(r, mloc_success, mloc_errorSyntax, mloc_errorNotFound, mloc_errorInvalid,
                   mloc_errorLimit));
    bool beaufort = false;
    for (size_t at = 0; at + 8 <= length; at++)
    {
        beaufort = beaufort || memcmp(unit + at, "beaufort", 8) == 0;
    }
    if (r == mloc_success && value->kind == mloc_decimalFinite && value->count > 0 && !beaufort)
    {
        Require(SameValue(&out, value));
    }
}

static void Format(const mlocUnitFormatter* formatter, const mlocDecimal* value)
{
    static char s_text[2][maxText];
    mlocOutput one = {.buffer = s_text[0], .capacity = maxText};
    mlocResult r = mlocFormatUnit(formatter, value, &one);
    Require(ONE_OF(r, mloc_success, mloc_errorCapacity, mloc_errorLimit));
    if (r == mloc_success)
    {
        mlocOutput two = {.buffer = s_text[1], .capacity = maxText};
        mlocOutput measure = {0};
        Require(mlocFormatUnit(formatter, value, &two) == mloc_success &&
                two.length == one.length && memcmp(s_text[0], s_text[1], one.length) == 0);
        Require(mlocFormatUnit(formatter, value, &measure) == mloc_errorCapacity &&
                measure.length == one.length);
    }
    mlocDecimal end;
    Require(mlocMakeDecimal(1000, &end) == mloc_success);
    mlocOutput range = {.buffer = s_text[0], .capacity = maxText};
    r = mlocFormatUnitRange(formatter, value, &end, &range);
    Require(ONE_OF(r, mloc_success, mloc_errorCapacity, mloc_errorLimit, mloc_errorInvalid));
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const char* const s_locales[] = {"en",     "de",
                                            "ar",     "fi",
                                            "ru",     "hi",
                                            "ja",     "fr",
                                            "cs",     "he",
                                            "pl",     "en-GB",
                                            "es-419", "sl",
                                            "ko",     "en-u-ms-ussystem",
                                            "da",     "de-u-mu-fahrenhe"};
    static const char* const s_usages[] = {
        "",       "road", "person-height", "default", "weather", "rainfall", "vehicle-fuel",
        "person", "food", "media",         "x"};
    static const char* const s_targets[] = {"meter",
                                            "foot",
                                            "celsius",
                                            "fahrenheit",
                                            "kilometer-per-hour",
                                            "beaufort",
                                            "second",
                                            "foot-and-inch",
                                            "kilogram",
                                            "square-meter",
                                            "mile-per-gallon",
                                            "liter-per-100-kilometer",
                                            "byte",
                                            "degree",
                                            ""};
    mlocDataSet set;
    if (size < headBytes || !OpenData(&set))
    {
        return 0;
    }
    const char* id = s_locales[data[0] % (sizeof(s_locales) / sizeof(s_locales[0]))];
    mlocLocale locale;
    Require(mlocCanonicalizeLocale(&set, id, strlen(id), &locale, nullptr).status == mloc_success);
    size_t valueLength = data[6] % (maxValueText + 1);
    if (size < headBytes + valueLength)
    {
        return 0;
    }
    mlocDecimal value = ValueOf(data + headBytes, valueLength);
    const char* unit = (const char*)data + headBytes + valueLength;
    size_t unitLength = size - headBytes - valueLength;
    const char* usage = s_usages[data[4] % (sizeof(s_usages) / sizeof(s_usages[0]))];
    const char* to = s_targets[data[5] % (sizeof(s_targets) / sizeof(s_targets[0]))];

    Convert(&set, unit, unitLength, to, &value);

    static char s_unit[512];
    mlocOutput chosen = {.buffer = s_unit, .capacity = sizeof(s_unit)};
    mlocUnitAmounts amounts;
    mlocResult r = mlocGetUnitPreference(&set, &locale, unit, unitLength, usage, strlen(usage),
                                         &value, nullptr, &chosen, &amounts);
    Require(ONE_OF(r, mloc_success, mloc_errorCapacity, mloc_errorLimit, mloc_errorSyntax,
                   mloc_errorNotFound, mloc_errorInvalid));
    Require(r != mloc_success ||
            (chosen.length > 0 && amounts.count >= 1 && amounts.count <= MLOC_MAX_UNIT_PARTS));

    mlocUnitWidth width = (mlocUnitWidth)(data[1] % 5);
    static char s_name[maxText];
    mlocOutput name = {.buffer = s_name, .capacity = sizeof(s_name)};
    r = mlocGetUnitDisplayName(&set, &locale, unit, unitLength, width, &name);
    Require(ONE_OF(r, mloc_success, mloc_fellBack, mloc_errorCapacity, mloc_errorSyntax,
                   mloc_errorLimit, mloc_errorNotFound, mloc_errorInvalid));

    mlocGender gender = mloc_genderNone;
    r = mlocGetUnitGender(&set, &locale, unit, unitLength, &gender);
    Require(ONE_OF(r, mloc_success, mloc_errorSyntax, mloc_errorNotFound, mloc_errorLimit,
                   mloc_errorInvalid));
    Require(gender <= mloc_genderPersonal);

    mlocUnitOptions options = {
        .width = width,
        .usage = (data[3] & 0x80) != 0 ? usage : nullptr,
        .usageLength = (data[3] & 0x80) != 0 ? strlen(usage) : 0,
        .coordinate = (mlocCoordinate)(data[3] % 4),
        .grammaticalCase = (mlocGrammaticalCase)(data[2] % 30),
    };
    mlocUnitFormatter formatter;
    r = mlocInitUnitFormatter(&formatter, &set, &locale, unit, unitLength, &options);
    Require(ONE_OF(r, mloc_success, mloc_fellBack, mloc_errorSyntax, mloc_errorNotFound,
                   mloc_errorLimit, mloc_errorInvalid));
    if (r == mloc_success || r == mloc_fellBack)
    {
        Format(&formatter, &value);
    }
    return 0;
}
