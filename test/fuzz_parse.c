// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for parsing. The first two bytes choose a locale of the
// compiled-in data, the next two the formatter (a number formatter's
// notation, style, sign display and grouping, or a currency formatter's
// currency, display and ISO append, with the currency names file) and the
// mode; the rest is the text. A parse refuses with syntax or a limit
// alone; it ends inside the text, but past a limit, which leaves the end,
// the amount and the code as they were, as a syntax error leaves the
// amount and the code. It reads the same twice, and an amount it reads
// writes text the formatter reads back whole, in both modes, to the same
// text again.

#include "maul-locale/currency.h"
#include "maul-locale/data.h"
#include "maul-locale/locale.h"
#include "maul-locale/number.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

// A formatter of either kind and its parse.
typedef struct Formatter
{
    bool currency;
    // The sign display always: ICU's strict mode wants the plus sign,
    // which NaN is written without.
    bool plusAlways;
    // en-US-POSIX's per mille sign, "0/00", begins with a digit, which
    // the number reads (as ICU reads it): its text does not come back.
    bool ambiguous;
    // The hidden display writes no currency, which any currency requires.
    bool hidden;
    mlocNumberFormatter number;
    mlocCurrencyFormatter money;
} Formatter;

static mlocResult Parse(const Formatter* f, const char* text, size_t length, mlocParseMode mode,
                        uint8_t any, mlocDecimal* decimal, char* code, size_t* end)
{
    return f->currency
               ? mlocCurrencyFormatter_Parse(&f->money, text, length, mode, any, decimal, code, end)
               : mlocNumberFormatter_Parse(&f->number, text, length, mode, decimal, end);
}

static mlocResult Format(const Formatter* f, const mlocDecimal* decimal, mlocOutput* output)
{
    return f->currency ? mlocCurrencyFormatter_Format(&f->money, decimal, output)
                       : mlocNumberFormatter_Format(&f->number, decimal, output);
}

// Whether every byte still holds the pattern it was filled with.
static bool Kept(const void* output, size_t size)
{
    const unsigned char* bytes = output;
    for (size_t i = 0; i < size; i++)
    {
        if (bytes[i] != 0xA5)
        {
            return false;
        }
    }
    return true;
}

static bool Same(const mlocDecimal* a, const mlocDecimal* b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

// The compiled-in data, and with the currency names file when it opens.
static bool OpenData(mlocDataSet* builtin, mlocDataSet* withNames)
{
    static mlocData s_sources[2];
    alignas(8) static uint8_t s_names[4 << 20];
    static size_t s_count;
    if (s_count == 0)
    {
        mlocOpenBuiltinData(&s_sources[0]);
        s_count = 1;
        FILE* file = fopen(MLOC_NAMES_FILE, "rb");
        if (file != nullptr)
        {
            size_t size = fread(s_names, 1, sizeof(s_names), file);
            fclose(file);
            s_count =
                size < sizeof(s_names) && mlocOpenData(s_names, size, &s_sources[1]) == mloc_success
                    ? 2
                    : 1;
        }
    }
    *builtin = (mlocDataSet){s_sources, 1};
    *withNames = (mlocDataSet){s_sources, s_count};
    return true;
}

static bool MakeFormatter(const mlocDataSet* data, const mlocLocale* locale, bool posix,
                          uint8_t kind, uint8_t options, Formatter* out)
{
    static const char* const s_codes[] = {"USD", "EUR", "JPY", "CHF", "INR",
                                          "CVE", "GBP", "CNY", "BHD", "XXX"};
    memset(out, 0, sizeof(*out));
    out->currency = (kind & 1) != 0;
    mlocSignDisplay sign =
        out->currency ? (mlocSignDisplay)(kind >> 5 & 3) : (mlocSignDisplay)(options / 9 % 5);
    out->plusAlways = sign == mloc_signAlways;
    out->ambiguous = posix && !out->currency && options / 3 % 3 == mloc_numberPerMille;
    if (!out->currency)
    {
        mlocNumberOptions o = {
            .notation = (mlocNotation)(options % 3),
            .style = (mlocNumberStyle)(options / 3 % 3),
            .signDisplay = sign,
            .grouping = (mlocGrouping)(kind >> 4 & 3),
        };
        return mlocInitNumberFormatter(data, locale, &o, &out->number) == mloc_success;
    }
    static const mlocCurrencyDisplay s_displays[] = {
        mloc_currencySymbol,       mloc_currencyNarrowSymbol,  mloc_currencyCode,
        mloc_currencyFormalSymbol, mloc_currencyVariantSymbol, mloc_currencyHidden,
        mloc_currencyName,
    };
    mlocCurrencyDisplay display = s_displays[options / 10 % 7];
    out->hidden = display == mloc_currencyHidden;
    mlocCurrencyOptions o = {
        .display = display,
        .appendCode = (uint8_t)((kind >> 4 & 1) != 0 && display <= mloc_currencyVariantSymbol),
        .number = {.signDisplay = sign},
    };
    return mlocInitCurrencyFormatter(data, locale, s_codes[options % 10], 3, &o, &out->money) ==
           mloc_success;
}

// Whether text is all ICU's ignorables, [[:Zs:][\t][:Bidi_Control:]],
// which a parse reads without moving its end ("12 " ends at 2).
static bool Ignorable(const char* text, size_t length)
{
    static const uint32_t s_ranges[][2] = {
        {0x09, 0x09},     {0x20, 0x20},     {0xA0, 0xA0},     {0x61C, 0x61C},
        {0x1680, 0x1680}, {0x2000, 0x200A}, {0x200E, 0x200F}, {0x202A, 0x202F},
        {0x205F, 0x205F}, {0x2066, 0x2069}, {0x3000, 0x3000},
    };
    const uint8_t* s = (const uint8_t*)text;
    for (size_t at = 0; at < length;)
    {
        size_t n = s[at] < 0x80 ? 1 : s[at] < 0xE0 ? 2 : s[at] < 0xF0 ? 3 : 4;
        if (at + n > length)
        {
            return false;
        }
        uint32_t cp = n == 1 ? s[at] : s[at] & (0x7F >> n);
        for (size_t i = 1; i < n; i++)
        {
            cp = cp << 6 | (s[at + i] & 0x3F);
        }
        bool found = false;
        for (size_t r = 0; r < sizeof(s_ranges) / sizeof(s_ranges[0]); r++)
        {
            found = found || (cp >= s_ranges[r][0] && cp <= s_ranges[r][1]);
        }
        if (!found)
        {
            return false;
        }
        at += n;
    }
    return true;
}

// An amount read writes text read back whole but its trailing
// ignorables, in both modes (NaN shown without the plus sign the always
// display wants in lenient mode alone), to the same text again; one too
// long to write (an exponent of thousands of digits) is passed.
static void RoundTrip(const Formatter* f, const mlocDecimal* decimal, uint8_t any)
{
    static char s_first[4096];
    static char s_second[4096];
    mlocOutput first = {.buffer = s_first, .capacity = sizeof(s_first)};
    mlocResult result = Format(f, decimal, &first);
    if (result == mloc_errorCapacity || result == mloc_errorLimit)
    {
        return;
    }
    Require(result == mloc_success);
    int last =
        f->plusAlways && decimal->kind == mloc_decimalNaN ? mloc_parseLenient : mloc_parseStrict;
    for (int mode = mloc_parseLenient; mode <= last; mode++)
    {
        mlocDecimal back;
        char code[3];
        size_t end = 0;
        Require(Parse(f, s_first, first.length, (mlocParseMode)mode, any, &back, code, &end) ==
                    mloc_success &&
                Ignorable(s_first + end, first.length - end));
        mlocOutput second = {.buffer = s_second, .capacity = sizeof(s_second)};
        Require(Format(f, &back, &second) == mloc_success && second.length == first.length &&
                memcmp(s_first, s_second, first.length) == 0);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 4)
    {
        return 0;
    }
    mlocDataSet builtin;
    mlocDataSet withNames;
    Require(OpenData(&builtin, &withNames));
    // A locale of the data, its id as a tag.
    const char* id = nullptr;
    size_t length = 0;
    size_t count = mlocData_GetLocaleCount(&builtin.sources[0]);
    Require(mlocData_GetLocale(&builtin.sources[0], (data[0] | (size_t)data[1] << 8) % count, &id,
                               &length) == mloc_success);
    char tag[64];
    mlocLocale locale;
    if (length >= sizeof(tag))
    {
        return 0;
    }
    for (size_t i = 0; i < length; i++)
    {
        tag[i] = id[i] == '_' ? '-' : id[i];
    }
    bool posix = length >= 5 && memcmp(tag + length - 5, "POSIX", 5) == 0;
    Formatter f;
    if (mlocParseLocale(tag, length, &locale).status != mloc_success ||
        !MakeFormatter((data[2] & 1) != 0 ? &withNames : &builtin, &locale, posix, data[2], data[3],
                       &f))
    {
        return 0;
    }
    mlocParseMode mode = (data[2] & 2) != 0 ? mloc_parseStrict : mloc_parseLenient;
    uint8_t any = (uint8_t)((data[2] >> 2) & (f.currency ? 1 : 0));
    const char* text = (const char*)data + 4;
    size_t textLength = size - 4;
    // Outputs a failure must leave as they were (base.h): the end, but
    // where a syntax error stopped, the amount and the code.
    mlocDecimal decimal;
    char code[3];
    size_t end = SIZE_MAX;
    memset(&decimal, 0xA5, sizeof(decimal));
    memset(code, 0xA5, sizeof(code));
    mlocResult result = Parse(&f, text, textLength, mode, any, &decimal, code, &end);
    Require(result == mloc_success || result == mloc_errorSyntax || result == mloc_errorLimit);
    Require(result == mloc_errorLimit ? end == SIZE_MAX : end <= textLength);
    Require(result == mloc_success ||
            (Kept(&decimal, sizeof(decimal)) && Kept(code, sizeof(code))));
    // The same text reads the same.
    mlocDecimal again;
    char codeAgain[3];
    size_t endAgain = SIZE_MAX;
    memset(codeAgain, 0xA5, sizeof(codeAgain));
    Require(Parse(&f, text, textLength, mode, any, &again, codeAgain, &endAgain) == result &&
            endAgain == end);
    if (result != mloc_success)
    {
        return 0;
    }
    Require(end > 0 && Same(&decimal, &again) && memcmp(code, codeAgain, 3) == 0);
    if (!f.ambiguous)
    {
        RoundTrip(&f, &decimal, f.hidden ? 0 : any);
    }
    return 0;
}
