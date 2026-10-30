// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for number formatting with the options a caller
// chooses. The first byte chooses a locale of the compiled-in data, the
// next twelve every field of mlocNumberOptions (now and then one out of
// range), the next whether parts are reported and the length of a
// decimal's text that follows; the rest is a second decimal's text.
// Making a formatter gives a listed result; a formatter writes the same
// text twice and measures its length; its parts lie in order within the
// text; a range gives a listed result, refuses a NaN end alone, and of
// two equal ends fails or fits as the one number does.

#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"
#include "maul-locale/number.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
    headBytes = 15,
    maxValueText = 64,
    maxText = 4096,
    // The most the approximately sign and its spacing add to a range of
    // equal ends.
    rangeSlack = 64,
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

// The parts reported for one text: each must start at or after the end
// of the one before and end within the text.
typedef struct Parts
{
    size_t end;
    size_t count;
    bool ordered;
} Parts;

static void OnPart(void* context, mlocPart part, size_t start, size_t length)
{
    Parts* p = context;
    p->ordered = p->ordered && part <= UINT16_MAX && start >= p->end && length > 0 &&
                 length <= SIZE_MAX - start;
    p->end = start + length;
    p->count++;
}

// The decimal of a text ("N", "I" and "-I" for NaN and the infinities,
// which a decimal's text cannot write), else of the text's length, so
// that every input has a value.
static mlocDecimal ValueOf(const uint8_t* text, size_t length)
{
    mlocDecimal d;
    bool infinity = (length == 1 && text[0] == 'I') || (length == 2 && memcmp(text, "-I", 2) == 0);
    if (infinity || (length == 1 && text[0] == 'N'))
    {
        double special = !infinity     ? (double)NAN
                         : length == 1 ? (double)INFINITY
                                       : -(double)INFINITY;
        Require(mlocMakeShortestDecimal(special, &d) == mloc_success);
    }
    else if (mlocParseDecimal((const char*)text, length, &d).status != mloc_success)
    {
        Require(mlocMakeDecimal((int64_t)length - 32, &d) == mloc_success);
    }
    return d;
}

// Formats a value: the same text twice, its length measured, its parts
// within it. Returns the result and the text's length.
static mlocResult Format(const mlocNumberFormatter* f, const mlocDecimal* value, bool parts,
                         size_t* lengthOut)
{
    static char s_text[2][maxText];
    mlocOutput one = {.buffer = s_text[0], .capacity = maxText};
    mlocResult r = mlocNumberFormatter_Format(f, value, &one);
    Require(ONE_OF(r, mloc_success, mloc_errorCapacity, mloc_errorLimit));
    *lengthOut = one.length;
    if (r == mloc_errorCapacity)
    {
        Require(one.length > maxText);
    }
    if (r != mloc_success)
    {
        return r;
    }
    Parts p = {.ordered = true};
    mlocOutput two = {.buffer = s_text[1], .capacity = maxText};
    if (parts)
    {
        two.part = OnPart;
        two.context = &p;
    }
    mlocOutput measure = {0};
    Require(mlocNumberFormatter_Format(f, value, &two) == mloc_success &&
            two.length == one.length && memcmp(s_text[0], s_text[1], one.length) == 0);
    Require(!parts || (p.ordered && p.end <= one.length && (p.count > 0) == (one.length > 0)));
    Require(mlocNumberFormatter_Format(f, value, &measure) == mloc_errorCapacity &&
            measure.length == one.length);
    return r;
}

static void Range(const mlocNumberFormatter* f, const mlocDecimal* start, const mlocDecimal* end,
                  bool parts)
{
    static char s_text[maxText];
    Parts p = {.ordered = true};
    mlocOutput out = {.buffer = s_text, .capacity = maxText};
    if (parts)
    {
        out.part = OnPart;
        out.context = &p;
    }
    mlocResult r = mlocNumberFormatter_FormatRange(f, start, end, &out);
    bool nan = start->kind == mloc_decimalNaN || end->kind == mloc_decimalNaN;
    Require(nan ? r == mloc_errorInvalid
                : ONE_OF(r, mloc_success, mloc_errorCapacity, mloc_errorLimit));
    Require(r != mloc_success || !parts || (p.ordered && p.end <= out.length));
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    static const char* const s_locales[] = {"en",
                                            "de",
                                            "de-CH",
                                            "fr",
                                            "ar",
                                            "ar-EG",
                                            "fa",
                                            "hi",
                                            "en-IN",
                                            "bn",
                                            "ja",
                                            "zh",
                                            "ko",
                                            "ru",
                                            "pl",
                                            "sv",
                                            "es-419",
                                            "my",
                                            "ps",
                                            "he",
                                            "ta-u-nu-native",
                                            "ja-u-nu-traditio",
                                            "zh-u-nu-finance",
                                            "th-u-nu-thai",
                                            "ar-u-nu-latn",
                                            "en-u-nu-arab",
                                            "sw",
                                            "fi"};
    static const uint16_t s_increments[] = {0,   1,   2,   5,    10,   20,   25,   50, 100,
                                            200, 250, 500, 1000, 2000, 2500, 5000, 3,  7};
    static mlocData s_builtin;
    static mlocDataSet s_set;
    static bool s_opened;
    if (!s_opened)
    {
        mlocOpenBuiltinData(&s_builtin);
        s_set = (mlocDataSet){&s_builtin, 1};
        s_opened = true;
    }
    if (size < headBytes)
    {
        return 0;
    }
    const char* id = s_locales[data[0] % (sizeof(s_locales) / sizeof(s_locales[0]))];
    mlocLocale locale;
    Require(mlocCanonicalizeLocale(&s_set, id, strlen(id), &locale, nullptr).status ==
            mloc_success);
    // Each field takes one value past its last now and then.
    mlocNumberOptions options = {
        .notation = (mlocNotation)(data[1] % (mloc_notationCompactLong + 2)),
        .style = (mlocNumberStyle)(data[2] % (mloc_numberPerMille + 2)),
        .grouping = (mlocGrouping)(data[3] % (mloc_groupingOff + 2)),
        .signDisplay = (mlocSignDisplay)(data[3] / 16 % (mloc_signNegative + 2)),
        .precision =
            {
                .kind = (mlocPrecisionKind)(data[4] % (mloc_precisionLessPrecision + 2)),
                .mode = (mlocRoundingMode)(data[5] % (mloc_roundTrunc + 2)),
                .trailingZeros =
                    (mlocTrailingZeros)(data[6] % (mloc_trailingZerosStripIfInteger + 2)),
                .minFractionDigits = (uint8_t)(data[7] % 102),
                .maxFractionDigits = (uint8_t)(data[8] % 102),
                .minSignificantDigits = (uint8_t)(data[9] % 23),
                .maxSignificantDigits = (uint8_t)(data[10] % 23),
                .increment =
                    s_increments[data[11] % (sizeof(s_increments) / sizeof(s_increments[0]))],
            },
        .minIntegerDigits = (uint8_t)(data[12] % 23),
    };
    bool parts = (data[13] & 1) != 0;
    size_t valueLength = data[14] % (maxValueText + 1);
    if (size < headBytes + valueLength)
    {
        return 0;
    }
    mlocDecimal one = ValueOf(data + headBytes, valueLength);
    mlocDecimal two = ValueOf(data + headBytes + valueLength, size - headBytes - valueLength);

    mlocNumberFormatter formatter;
    mlocResult r = mlocInitNumberFormatter(&s_set, &locale, &options, &formatter);
    Require(ONE_OF(r, mloc_success, mloc_fellBack, mloc_errorInvalid));
    if (r == mloc_errorInvalid)
    {
        return 0;
    }
    size_t length = 0;
    mlocResult single = Format(&formatter, &one, parts, &length);
    (void)Format(&formatter, &two, parts, &length);
    Range(&formatter, &one, &two, parts);
    Range(&formatter, &two, &one, parts);
    if (one.kind != mloc_decimalNaN)
    {
        // Equal ends are prepared as the one number is: they fail alike,
        // and write the number with the approximately sign when it fits.
        static char s_text[maxText];
        mlocOutput same = {.buffer = s_text, .capacity = maxText};
        size_t singleLength = 0;
        single = Format(&formatter, &one, false, &singleLength);
        mlocResult r2 = mlocNumberFormatter_FormatRange(&formatter, &one, &one, &same);
        Require(single != mloc_errorLimit || r2 == mloc_errorLimit);
        Require(single != mloc_success || singleLength + rangeSlack > maxText ||
                (r2 == mloc_success && same.length >= singleLength));
    }
    return 0;
}
