// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number formatting: the formatter is the core and its options
// (number_format.h).

#include "maul-locale/number.h"

#include "number_core.h"
#include "number_format.h"
#include "number_parse.h"
#include "plural.h"

#include <string.h>

static_assert(sizeof(Formatter) <= sizeof(mlocNumberFormatter),
              "a formatter fits its opaque value");

mlocResult mlocInitNumberFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                   const mlocNumberOptions* options,
                                   mlocNumberFormatter* formatterOut)
{
    static const mlocNumberOptions s_defaults = {0};
    const mlocNumberOptions* o = options == nullptr ? &s_defaults : options;
    if (formatterOut == nullptr || data == nullptr || locale == nullptr ||
        !mlocValidFormatOptions(o))
    {
        return mloc_errorInvalid;
    }
    bool compact =
        o->notation == mloc_notationCompactShort || o->notation == mloc_notationCompactLong;
    Formatter f;
    memset(&f, 0, sizeof(f));
    mlocResult result = compact ? mlocInitCompactCore(&f.core, data, locale,
                                                      o->notation == mloc_notationCompactLong,
                                                      &f.compactTable, &f.compactPatterns)
                                : mlocInitNumberCore(&f.core, data, locale, o->style);
    if (result >= 0 && compact)
    {
        mlocResult rule = mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f.pluralRule);
        result = rule < 0 ? rule : result;
    }
    if (result < 0)
    {
        return result;
    }
    mlocSetFormatOptions(&f, o, numberFormatterStamp);
    memcpy(formatterOut, &f, sizeof(f));
    memset((uint8_t*)formatterOut + sizeof(f), 0, sizeof(*formatterOut) - sizeof(f));
    return result;
}

mlocResult mlocNumberFormatter_Format(const mlocNumberFormatter* formatter,
                                      const mlocDecimal* decimal, mlocOutput* textOut)
{
    Formatter f;
    if (!mlocReadFormatter(formatter, sizeof(*formatter), numberFormatterStamp, &f))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatWith(&f, nullptr, decimal, textOut);
}

mlocResult mlocNumberFormatter_FormatRange(const mlocNumberFormatter* formatter,
                                           const mlocDecimal* start, const mlocDecimal* end,
                                           mlocOutput* textOut)
{
    Formatter f;
    if (!mlocReadFormatter(formatter, sizeof(*formatter), numberFormatterStamp, &f))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatRangeWith(&f, nullptr, start, end, textOut);
}

mlocResult mlocNumberFormatter_Parse(const mlocNumberFormatter* formatter, const char* text,
                                     size_t length, mlocParseMode mode, mlocDecimal* decimalOut,
                                     size_t* endOut)
{
    Formatter f;
    if (!mlocReadFormatter(formatter, sizeof(*formatter), numberFormatterStamp, &f) ||
        (text == nullptr && length > 0) || decimalOut == nullptr || endOut == nullptr ||
        (uint32_t)mode > mloc_parseStrict || f.compactTable != nullptr)
    {
        return mloc_errorInvalid;
    }
    return mlocParseWith(&f, nullptr, text, length, mode, decimalOut, nullptr, endOut);
}
