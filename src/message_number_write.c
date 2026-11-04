// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number functions of messages: preparing a value's formatter and
// writing the value with it.

#include "invariant.h"
#include "message_number.h"

#include "maul-locale/currency.h"
#include "maul-locale/plural.h"

#include <stddef.h>
#include <string.h>

// Keeps a formatter made in a call (a unit formatter is 3.5 KB) in its
// own frame, not in every write's.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

bool mlocNumberFits(const PreparedNumber* prepared, const NumberValue* value)
{
    if (prepared == NULL)
    {
        return false;
    }
    // The options alone, the bytes after the decimal: a key holds no
    // decimal, and a decimal's padding is no part of it.
    NumberValue key = mlocNumberKey(value);
    size_t options = offsetof(NumberValue, origin);
    return memcmp((const uint8_t*)&key + options, (const uint8_t*)&prepared->key + options,
                  sizeof key - options) == 0;
}

// A currency formatter's options for a currency value.
static mlocCurrencyOptions CurrencyOptions(const NumberValue* value)
{
    mlocCurrencyOptions options;
    memset(&options, 0, sizeof options);
    options.number = mlocNumberFormatOptions(value);
    options.display = (value->given & numberGivenDisplay) != 0 ? (mlocCurrencyDisplay)value->display
                                                               : mloc_currencySymbol;
    options.sign = (value->given & numberGivenCurrencySign) != 0
                       ? (mlocCurrencySign)value->currencySign
                       : mloc_currencySignLocale;
    return options;
}

// A unit formatter's options for a unit value.
static mlocUnitOptions UnitOptions(const NumberValue* value)
{
    mlocUnitOptions options;
    memset(&options, 0, sizeof options);
    options.number = mlocNumberFormatOptions(value);
    options.width =
        (value->given & numberGivenWidth) != 0 ? (mlocUnitWidth)value->width : mloc_unitWidthShort;
    if ((value->given & numberGivenUsage) != 0)
    {
        options.usage = value->usage;
        options.usageLength = value->usageLength;
    }
    return options;
}

// Writes a unit value with a unit formatter made here: 3.5 KB, off the
// resolver's frames.
MLOC_NOINLINE static mlocResult WriteUnit(const NumberValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output)
{
    mlocUnitOptions options = UnitOptions(value);
    mlocUnitFormatter formatter;
    mlocResult made =
        mlocInitUnitFormatter(data, locale, value->unit, value->unitLength, &options, &formatter);
    return made < 0 ? made : mlocUnitFormatter_Format(&formatter, &value->decimal, output);
}

MLOC_NOINLINE static mlocResult WriteCurrency(const NumberValue* value, const mlocDataSet* data,
                                              const mlocLocale* locale, mlocOutput* output)
{
    mlocCurrencyOptions options = CurrencyOptions(value);
    mlocCurrencyFormatter formatter;
    mlocResult made = mlocInitCurrencyFormatter(data, locale, value->code, 3, &options, &formatter);
    return made < 0 ? made : mlocCurrencyFormatter_Format(&formatter, &value->decimal, output);
}

size_t mlocPreparedSize(PreparedKind kind)
{
    static const size_t sizes[] = {sizeof(mlocNumberFormatter), sizeof(mlocCurrencyFormatter),
                                   sizeof(mlocUnitFormatter), sizeof(mlocSpelloutFormatter),
                                   sizeof(mlocRelativeFormatter)};
    MLOC_ASSERT((uint32_t)kind <= preparedRelative);
    return offsetof(PreparedNumber, formatter) + sizes[kind];
}

bool mlocPrepareNumber(const NumberValue* value, const mlocDataSet* data, const mlocLocale* locale,
                       PreparedNumber* out)
{
    memset(out, 0, offsetof(PreparedNumber, formatter));
    out->key = mlocNumberKey(value);
    out->kind = value->function == numberFunctionCurrency ? preparedCurrency
                : value->function == numberFunctionUnit   ? preparedUnit
                                                          : preparedNumber;
    if (value->selectable && value->select != numberSelectExact)
    {
        mlocPluralType type =
            value->select == numberSelectOrdinal ? mloc_pluralOrdinal : mloc_pluralCardinal;
        out->rulesType =
            mlocInitPluralRules(data, locale, type, &out->rules) < 0 ? 0 : (uint8_t)(1 + type);
    }
    if (out->kind == preparedUnit)
    {
        mlocUnitOptions options = UnitOptions(value);
        return mlocInitUnitFormatter(data, locale, value->unit, value->unitLength, &options,
                                     &out->formatter.unit) >= 0;
    }
    if (out->kind == preparedCurrency)
    {
        mlocCurrencyOptions options = CurrencyOptions(value);
        return mlocInitCurrencyFormatter(data, locale, value->code, 3, &options,
                                         &out->formatter.currency) >= 0;
    }
    mlocNumberOptions options = mlocNumberFormatOptions(value);
    return mlocInitNumberFormatter(data, locale, &options, &out->formatter.number) >= 0;
}

mlocMessageFaultKind mlocWriteNumberValue(const NumberValue* value, const PreparedNumber* prepared,
                                          const mlocDataSet* data, const mlocLocale* locale,
                                          mlocOutput* output)
{
    mlocResult result;
    if (mlocNumberFits(prepared, value))
    {
        result =
            prepared->kind == preparedUnit
                ? mlocUnitFormatter_Format(&prepared->formatter.unit, &value->decimal, output)
            : prepared->kind == preparedCurrency
                ? mlocCurrencyFormatter_Format(&prepared->formatter.currency, &value->decimal,
                                               output)
                : mlocNumberFormatter_Format(&prepared->formatter.number, &value->decimal, output);
    }
    else if (value->function == numberFunctionUnit)
    {
        result = WriteUnit(value, data, locale, output);
    }
    else if (value->function == numberFunctionCurrency)
    {
        result = WriteCurrency(value, data, locale, output);
    }
    else
    {
        mlocNumberOptions options = mlocNumberFormatOptions(value);
        mlocNumberFormatter formatter;
        result = mlocInitNumberFormatter(data, locale, &options, &formatter);
        if (result >= 0)
        {
            result = mlocNumberFormatter_Format(&formatter, &value->decimal, output);
        }
    }
    // A text past the room reports the length it needs.
    return result == mloc_success || (result == mloc_errorCapacity && output->length > 0)
               ? mloc_messageFaultNone
               : mloc_messageFaultUnsupportedOperation;
}
