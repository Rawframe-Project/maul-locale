// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency formatting (record mloc-0022): a number formatter's core with
// the locale's currency pattern, the currency's symbol and separators
// gathered field by field along the locale's chain, and its digits.

#include "maul-locale/currency.h"

#include "bytes.h"
#include "currency_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"
#include "number_core.h"
#include "number_data.h"
#include "number_format.h"
#include "plural.h"

#include <string.h>

enum
{
    // Marks a currency formatter the library made.
    currencyStamp = 0x43,
};

static_assert(sizeof(Formatter) <= sizeof(mlocCurrencyFormatter),
              "a formatter fits its opaque value");

// A currency's fields, each from the first key along the chain that
// gives it.
typedef struct Fields
{
    uint16_t code;
    uint32_t found;
    const uint8_t* text[currencyFieldCount];
    uint16_t length[currencyFieldCount];
    uint8_t spacing[currencyFieldCount];
    // The own pattern's index, in the patterns unit of its source.
    uint16_t pattern;
    DataSpan patterns;
    bool broken;
} Fields;

static bool Gather(void* context, DataSpan unit, const DataView* view, int depth)
{
    (void)depth;
    Fields* f = context;
    SparseTable t = mlocReadSparseTable(unit);
    const uint8_t* row = mlocFindSparseRow(&t, f->code);
    for (uint32_t field = 0; row != nullptr && field < currencyFieldCount; field++)
    {
        uint16_t value = 0;
        if ((f->found & 1u << field) != 0 || !mlocSparseField(row, field, &value))
        {
            continue;
        }
        f->found |= 1u << field;
        if (field == currencyOwnPattern)
        {
            // The index names a pattern of the unit's own source.
            f->pattern = value;
            f->broken |= !mlocFindDataUnit(view, kindNumberPatterns, dataNoLocale, &f->patterns);
            continue;
        }
        uint16_t start = mlocLoad16(t.offsets + (size_t)value * 2);
        f->text[field] = t.text + start;
        f->length[field] = (uint16_t)(mlocLoad16(t.offsets + (size_t)value * 2 + 2) - start);
        f->spacing[field] = t.text[t.textSize + value];
    }
    return f->found == (1u << currencyFieldCount) - 1;
}

// The field a display shows, falling back to the symbol; currencyFieldCount
// for the code, which a currency without the field or a symbol shows.
static uint32_t ShownField(const Fields* f, mlocCurrencyDisplay display)
{
    uint32_t field = display == mloc_currencyNarrowSymbol    ? currencyNarrow
                     : display == mloc_currencyFormalSymbol  ? currencyFormal
                     : display == mloc_currencyVariantSymbol ? currencyVariant
                                                             : currencySymbol;
    if (display == mloc_currencyCode)
    {
        return currencyFieldCount;
    }
    field = (f->found & 1u << field) != 0 ? field : currencySymbol;
    return (f->found & 1u << field) != 0 ? field : currencyFieldCount;
}

static bool ValidOptions(const mlocCurrencyOptions* o)
{
    const mlocNumberOptions* n = &o->number;
    return mlocValidFormatOptions(n) && n->style == mloc_numberDecimal &&
           (uint32_t)o->display <= mloc_currencyHidden &&
           (uint32_t)o->usage <= mloc_currencyUsageCash;
}

// The zero precision's: the currency's digits and rounding increment.
static bool CurrencyPrecision(const mlocDataSet* data, uint16_t code, mlocCurrencyUsage usage,
                              mlocPrecision* precision)
{
    DataSpan fractions;
    if (!mlocFindDataSetUnit(data, kindCurrencyFractions, nullptr, 0, &fractions))
    {
        return false;
    }
    CurrencyFraction f = mlocFindCurrencyFraction(fractions, code);
    bool cash = usage == mloc_currencyUsageCash;
    uint8_t digits = cash ? f.cashDigits : f.digits;
    uint8_t rounding = cash ? f.cashRounding : f.rounding;
    precision->kind = mloc_precisionFraction;
    precision->minFractionDigits = digits;
    precision->maxFractionDigits = digits;
    precision->increment = rounding > 1 ? rounding : 0;
    return true;
}

// Whether the locale asks for accounting: -u-cf-account.
static bool Accounting(const mlocLocale* locale)
{
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "cf", &value);
    return length == 7 && memcmp(value, "account", 7) == 0;
}

// Sets the currency a core shows and the separators it gives of its own.
static bool ShowCurrency(NumberCore* core, const Fields* f, mlocCurrencyDisplay display,
                         bool ownPattern)
{
    mlocUnpackCurrency(f->code, core->currencyCode);
    uint32_t field = ShownField(f, display);
    if (field != currencyFieldCount)
    {
        if (f->length[field] > UINT8_MAX)
        {
            return false;
        }
        core->currencyText = f->text[field];
        core->currencyLength = (uint8_t)f->length[field];
    }
    if ((f->found & 1u << currencyOwnDecimal) != 0)
    {
        core->decimalText = f->text[currencyOwnDecimal];
        core->symbolLength[numberDecimal] = f->length[currencyOwnDecimal];
    }
    if ((f->found & 1u << currencyOwnGroup) != 0)
    {
        core->groupText = f->text[currencyOwnGroup];
        core->symbolLength[numberGroup] = f->length[currencyOwnGroup];
    }
    return !ownPattern || mlocUseNumberPattern(core, f->patterns, f->pattern);
}

mlocResult mlocInitCurrencyFormatter(mlocCurrencyFormatter* formatterOut, const mlocDataSet* data,
                                     const mlocLocale* locale, const char* code, size_t codeLength,
                                     const mlocCurrencyOptions* options)
{
    static const mlocCurrencyOptions s_defaults = {0};
    const mlocCurrencyOptions* o = options == nullptr ? &s_defaults : options;
    uint16_t packed = code == nullptr || codeLength != 3 ? 0 : mlocPackCurrency(code, 3);
    if (formatterOut == nullptr || data == nullptr || locale == nullptr || packed == 0 ||
        !ValidOptions(o))
    {
        return mloc_errorInvalid;
    }
    Fields fields = {.code = packed};
    if (!mlocVisitLocaleUnits(data, mloc_componentMain, kindCurrencySymbols, locale, Gather,
                              &fields) ||
        fields.broken)
    {
        return mloc_errorNotFound;
    }
    bool hidden = o->display == mloc_currencyHidden;
    // A code shows as three letters.
    uint32_t shown = ShownField(&fields, o->display);
    bool compact = o->number.notation == mloc_notationCompactShort ||
                   o->number.notation == mloc_notationCompactLong;
    CurrencyStyle style = {
        .accounting = Accounting(locale),
        .hidden = hidden,
        .spacing = shown != currencyFieldCount ? fields.spacing[shown]
                                               : currencyStartsAlpha | currencyEndsAlpha,
        .longLength = o->number.notation == mloc_notationCompactLong,
    };
    // A currency's own pattern replaces the standard one alone.
    bool ownPattern =
        (fields.found & 1u << currencyOwnPattern) != 0 && !style.accounting && !hidden;
    Formatter f;
    memset(&f, 0, sizeof(f));
    mlocResult result =
        mlocInitCurrencyCore(&f.core, data, locale, &style, compact ? &f.compactTable : nullptr,
                             compact ? &f.compactPatterns : nullptr);
    if (result >= 0 && compact)
    {
        mlocResult rule = mlocFindPluralRule(data, locale, mloc_pluralCardinal, &f.pluralRule);
        result = rule < 0 ? rule : result;
    }
    if (result < 0)
    {
        return result;
    }
    mlocSetFormatOptions(&f, &o->number, currencyStamp);
    // Compact notation rounds by its own rule unless the options say
    // otherwise, as ICU does.
    if (!ShowCurrency(&f.core, &fields, o->display, ownPattern) ||
        (o->number.precision.kind == mloc_precisionNone && !compact &&
         !CurrencyPrecision(data, packed, o->usage, &f.precision)))
    {
        return mloc_errorNotFound;
    }
    memset(formatterOut, 0, sizeof(*formatterOut));
    memcpy(formatterOut, &f, sizeof(f));
    return result;
}

mlocResult mlocFormatCurrency(const mlocCurrencyFormatter* formatter, const mlocDecimal* decimal,
                              mlocOutput* output)
{
    Formatter f;
    if (!mlocReadFormatter(formatter, sizeof(*formatter), currencyStamp, &f))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatWith(&f, decimal, output);
}

mlocResult mlocFormatCurrencyRange(const mlocCurrencyFormatter* formatter, const mlocDecimal* start,
                                   const mlocDecimal* end, mlocOutput* output)
{
    Formatter f;
    if (!mlocReadFormatter(formatter, sizeof(*formatter), currencyStamp, &f))
    {
        return mloc_errorInvalid;
    }
    return mlocFormatRangeWith(&f, start, end, output);
}
