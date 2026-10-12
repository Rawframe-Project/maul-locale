// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number formatting (record mloc-0021): the formatter is the core and its
// options; formatting rounds the decimal, then writes it.

#include "maul-locale/number.h"

#include "decimal.h"
#include "number_core.h"
#include "number_precision.h"

#include <string.h>

enum
{
    // Marks a value the library made.
    formatterStamp = 0x4E,
    maxMinInteger = 21,
};

// The opaque value's contents.
typedef struct Formatter
{
    NumberCore core;
    mlocPrecision precision;
    uint8_t notation;
    uint8_t style;
    uint8_t grouping;
    uint8_t signDisplay;
    uint8_t minInteger;
    uint8_t stamp;
} Formatter;

static_assert(sizeof(Formatter) <= sizeof(mlocNumberFormatter),
              "a formatter fits its opaque value");

static bool ValidOptions(const mlocNumberOptions* o)
{
    // Only standard notation and the decimal style are built yet.
    return o->notation == mloc_notationStandard && o->style == mloc_numberDecimal &&
           (uint32_t)o->grouping <= mloc_groupingOff &&
           (uint32_t)o->signDisplay <= mloc_signNegative && o->minIntegerDigits <= maxMinInteger &&
           mlocValidPrecision(&o->precision);
}

mlocResult mlocInitNumberFormatter(mlocNumberFormatter* formatterOut, const mlocDataSet* data,
                                   const mlocLocale* locale, const mlocNumberOptions* options)
{
    static const mlocNumberOptions s_defaults = {0};
    const mlocNumberOptions* o = options == nullptr ? &s_defaults : options;
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || !ValidOptions(o))
    {
        return mloc_errorInvalid;
    }
    Formatter f;
    memset(&f, 0, sizeof(f));
    mlocResult result = mlocInitNumberCore(&f.core, data, locale);
    if (result < 0)
    {
        return result;
    }
    f.precision = o->precision;
    f.notation = (uint8_t)o->notation;
    f.style = (uint8_t)o->style;
    f.grouping = (uint8_t)o->grouping;
    f.signDisplay = (uint8_t)o->signDisplay;
    f.minInteger = o->minIntegerDigits == 0 ? 1 : o->minIntegerDigits;
    f.stamp = formatterStamp;
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

// The formatter of an opaque value, or false when the library did not
// make it.
static bool Read(const mlocNumberFormatter* opaque, Formatter* f)
{
    if (opaque == nullptr)
    {
        return false;
    }
    memcpy(f, opaque, sizeof(*f));
    return f->stamp == formatterStamp && f->core.symbolText != nullptr &&
           f->core.affixText != nullptr && f->core.digitLength >= 1 &&
           f->core.digitLength <= numberDigitSize && f->minInteger >= 1 &&
           f->minInteger <= maxMinInteger && f->grouping <= mloc_groupingOff &&
           f->signDisplay <= mloc_signNegative && f->notation == mloc_notationStandard &&
           f->style == mloc_numberDecimal && (f->core.primary == 0) == (f->core.secondary == 0) &&
           mlocValidPrecision(&f->precision);
}

mlocResult mlocFormatNumber(const mlocNumberFormatter* formatter, const mlocDecimal* decimal,
                            mlocOutput* output)
{
    Formatter formatterValue;
    const Formatter* f = &formatterValue;
    if (!Read(formatter, &formatterValue) || decimal == nullptr || output == nullptr ||
        (output->buffer == nullptr && output->capacity > 0) || !mlocValidDecimal(decimal))
    {
        return mloc_errorInvalid;
    }
    mlocDecimal d = *decimal;
    if (d.kind == mloc_decimalFinite)
    {
        mlocResult rounded = mlocApplyPrecision(&f->precision, &d);
        if (rounded != mloc_success)
        {
            return rounded;
        }
    }
    NumberSign sign = mlocNumberSign(&d, (mlocSignDisplay)f->signDisplay);
    NumberWriter w = {.buffer = output->buffer, .capacity = output->capacity};
    mlocWriteStandard(&f->core, &d, sign, (mlocGrouping)f->grouping, f->minInteger, &w);
    output->length = w.length;
    if (output->buffer == nullptr || w.length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (output->part != nullptr)
    {
        NumberWriter parts = {.parts = output};
        mlocWriteStandard(&f->core, &d, sign, (mlocGrouping)f->grouping, f->minInteger, &parts);
        mlocEndParts(&parts);
    }
    return mloc_success;
}
