// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number formatters' core (number_core.h).

#include "number_core.h"

#include "bytes.h"
#include "data_format.h"
#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"
#include "number_data.h"

#include <string.h>

// The units a locale's numbers index, all of one source.
typedef struct Units
{
    DataSpan locale;
    DataSpan systems;
    DataSpan symbols;
    DataSpan patterns;
} Units;

static uint32_t SystemCount(DataSpan systems)
{
    return mlocLoad32(systems.bytes);
}

static const uint8_t* System(DataSpan systems, uint32_t index)
{
    return systems.bytes + numberSystemsHeaderSize + (size_t)index * numberSystemSize;
}

// The index of a system by its packed id, or numberAlgorithmic when the
// table has none (a system without digits, or no system at all).
static uint32_t FindSystem(DataSpan systems, uint64_t id)
{
    uint32_t low = 0;
    uint32_t high = SystemCount(systems);
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint64_t at = mlocLoad64(System(systems, middle));
        if (at == id)
        {
            return middle;
        }
        if (at < id)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return numberAlgorithmic;
}

static bool Is(const char* value, size_t length, const char* text)
{
    return length == strlen(text) && memcmp(value, text, length) == 0;
}

// The system the locale asks for: its -u-nu keyword, "native",
// "traditio" and "finance" as its data names them (UTS #35: traditional
// falls back to native, finance to the default), else its default.
// Sets *fellBack when that system has no digits and the default stands
// instead.
static uint32_t ChooseSystem(const mlocLocale* locale, const uint8_t* header, DataSpan systems,
                             bool* fellBack)
{
    const char* value = nullptr;
    size_t length = mlocFindKeyword(locale, "nu", &value);
    uint32_t system = header[localeDefaultSystem];
    if (length == 0)
    {
        return system;
    }
    if (Is(value, length, "native"))
    {
        system = header[localeNativeSystem];
    }
    else if (Is(value, length, "traditio"))
    {
        system = header[localeTraditionalSystem] == numberNoSystem
                     ? header[localeNativeSystem]
                     : header[localeTraditionalSystem];
    }
    else if (Is(value, length, "finance"))
    {
        system = header[localeFinanceSystem] == numberNoSystem ? header[localeDefaultSystem]
                                                               : header[localeFinanceSystem];
    }
    else
    {
        system = FindSystem(systems, mlocPackSystem(value, (uint32_t)length));
    }
    if (system == numberAlgorithmic)
    {
        *fellBack = true;
        system = header[localeDefaultSystem];
    }
    return system;
}

// The locale's entry for a system, else latn's; null when it has
// neither.
static const uint8_t* FindEntry(DataSpan locale, DataSpan systems, uint32_t system)
{
    uint32_t latn = FindSystem(systems, numberLatn);
    const uint8_t* fallback = nullptr;
    for (uint32_t i = 0; i < locale.bytes[localeEntryCount]; i++)
    {
        const uint8_t* e = locale.bytes + numberLocaleHeaderSize + (size_t)i * numberEntrySize;
        if (e[entrySystem] == system)
        {
            return e;
        }
        if (e[entrySystem] == latn)
        {
            fallback = e;
        }
    }
    return fallback;
}

static void TableString(NumberTable t, uint32_t index, uint16_t* startOut, uint16_t* lengthOut)
{
    uint16_t start = mlocLoad16(t.offsets + (size_t)index * 2);
    *startOut = start;
    *lengthOut = (uint16_t)(mlocLoad16(t.offsets + (size_t)index * 2 + 2) - start);
}

// Fills a core from a system and an entry whose indexes are in bounds.
static bool Fill(NumberCore* core, const Units* u, uint32_t system, const uint8_t* entry,
                 uint8_t minGrouping, int patternField)
{
    NumberTable symbols = mlocReadNumberTable(u->symbols, 2 * numberSymbolCount);
    NumberTable patterns = mlocReadNumberTable(u->patterns, numberPatternSize);
    uint32_t set = mlocLoad16(entry + entrySymbols);
    uint32_t pattern = mlocLoad16(entry + patternField);
    if (set >= symbols.recordCount || mlocLoad16(entry + entryDecimal) >= patterns.recordCount ||
        mlocLoad16(entry + entryPercent) >= patterns.recordCount)
    {
        return false;
    }
    *core = (NumberCore){
        .symbolText = symbols.text,
        .affixText = patterns.text,
        .minGrouping = minGrouping,
    };
    const uint8_t* s = symbols.records + (size_t)set * 2 * numberSymbolCount;
    for (int i = 0; i < numberSymbolCount; i++)
    {
        TableString(symbols, mlocLoad16(s + 2 * i), &core->symbolStart[i], &core->symbolLength[i]);
    }
    const uint8_t* p = patterns.records + (size_t)pattern * numberPatternSize;
    for (int i = 0; i < affixCount; i++)
    {
        TableString(patterns, mlocLoad16(p + 2 * i), &core->affixStart[i], &core->affixLength[i]);
    }
    core->primary = p[patternPrimary];
    core->secondary = p[patternSecondary];
    const uint8_t* digits = System(u->systems, system) + 8;
    memcpy(core->digits, digits, sizeof(core->digits));
    uint8_t length = 1;
    while (length < numberDigitSize && digits[length] != 0)
    {
        length++;
    }
    core->digitLength = length;
    return true;
}

mlocResult mlocInitNumberCore(NumberCore* coreOut, const mlocDataSet* data,
                              const mlocLocale* locale, mlocNumberStyle style)
{
    Units u;
    // Number data is filed only where it differs from the chain's, so
    // whether the unit was the locale's own says nothing; only a
    // numbering system not taken is reported.
    bool inherited = false;
    bool fellBack = false;
    DataView view;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kindNumbers, locale, &u.locale, &inherited,
                            &view) ||
        !mlocFindDataUnit(&view, kindNumberSystems, dataNoLocale, &u.systems) ||
        !mlocFindDataUnit(&view, kindNumberSymbols, dataNoLocale, &u.symbols) ||
        !mlocFindDataUnit(&view, kindNumberPatterns, dataNoLocale, &u.patterns))
    {
        return mloc_errorNotFound;
    }
    const uint8_t* header = u.locale.bytes;
    uint32_t count = SystemCount(u.systems);
    // The units of one source agree when every system index the locale
    // unit holds is in the systems table.
    for (int i = localeDefaultSystem; i <= localeFinanceSystem; i++)
    {
        if (header[i] >= count && header[i] != numberNoSystem && header[i] != numberAlgorithmic)
        {
            return mloc_errorNotFound;
        }
    }
    uint32_t system = ChooseSystem(locale, header, u.systems, &fellBack);
    const uint8_t* entry = system < count ? FindEntry(u.locale, u.systems, system) : nullptr;
    NumberCore core;
    int patternField = style == mloc_numberDecimal ? entryDecimal : entryPercent;
    if (entry == nullptr ||
        !Fill(&core, &u, system, entry, header[localeMinGrouping], patternField))
    {
        return mloc_errorNotFound;
    }
    *coreOut = core;
    return fellBack ? mloc_fellBack : mloc_success;
}

NumberSign mlocNumberSign(const mlocDecimal* d, mlocSignDisplay display)
{
    if (d->kind == mloc_decimalNaN)
    {
        return numberSignNone;
    }
    bool zero = d->kind == mloc_decimalFinite && d->count == 0;
    NumberSign sign = d->negative ? numberSignMinus : numberSignPlus;
    switch (display)
    {
    case mloc_signAuto:
        return d->negative ? numberSignMinus : numberSignNone;
    case mloc_signAlways:
        return sign;
    case mloc_signExceptZero:
        return zero ? numberSignNone : sign;
    case mloc_signNegative:
        return d->negative && !zero ? numberSignMinus : numberSignNone;
    default:
        return numberSignNone;
    }
}

static void Put(NumberWriter* w, uint32_t kind, const void* bytes, size_t length)
{
    if (w->parts != nullptr)
    {
        if (kind != w->kind && w->length > w->start)
        {
            w->parts->part(w->parts->context, w->kind, w->start, w->length - w->start);
        }
        if (kind != w->kind || w->length == w->start)
        {
            w->kind = kind;
            w->start = w->length;
        }
    }
    else if (length <= w->capacity && w->length <= w->capacity - length)
    {
        memcpy(w->buffer + w->length, bytes, length);
    }
    w->length += length;
}

void mlocEndParts(NumberWriter* w)
{
    if (w->parts != nullptr && w->length > w->start)
    {
        w->parts->part(w->parts->context, w->kind, w->start, w->length - w->start);
        w->start = w->length;
    }
}

static void PutSymbol(NumberWriter* w, const NumberCore* c, NumberSymbol symbol, uint32_t kind)
{
    Put(w, kind, c->symbolText + c->symbolStart[symbol], c->symbolLength[symbol]);
}

// Writes an affix: its literal text, and the symbols its placeholders
// stand for; with plus set, the minus sign's place takes the plus sign.
static void PutAffix(NumberWriter* w, const NumberCore* c, int affix, bool plus, bool perMille)
{
    const uint8_t* text = c->affixText + c->affixStart[affix];
    size_t length = c->affixLength[affix];
    size_t literal = 0;
    for (size_t i = 0; i < length; i++)
    {
        uint8_t b = text[i];
        if (b >= 0x20)
        {
            continue;
        }
        if (i > literal)
        {
            Put(w, mloc_numberPartLiteral, text + literal, i - literal);
        }
        literal = i + 1;
        switch (b)
        {
        case numberAffixMinus:
            PutSymbol(w, c, plus ? numberPlus : numberMinus,
                      plus ? mloc_numberPartPlusSign : mloc_numberPartMinusSign);
            break;
        case numberAffixPlus:
            PutSymbol(w, c, numberPlus, mloc_numberPartPlusSign);
            break;
        case numberAffixPercent:
            PutSymbol(w, c, perMille ? numberPerMille : numberPercent,
                      perMille ? mloc_numberPartPerMilleSign : mloc_numberPartPercentSign);
            break;
        case numberAffixPerMille:
            PutSymbol(w, c, numberPerMille, mloc_numberPartPerMilleSign);
            break;
        default:
            // Currency placeholders belong to the currency formatter.
            break;
        }
    }
    if (length > literal)
    {
        Put(w, mloc_numberPartLiteral, text + literal, length - literal);
    }
}

static bool HasMinus(const NumberCore* c, int affix)
{
    return memchr(c->affixText + c->affixStart[affix], numberAffixMinus, c->affixLength[affix]) !=
           nullptr;
}

// The prefix (or suffix) for a sign: a plus sign takes the negative
// affixes with the plus sign in the minus sign's place, or, when they
// have none, goes before the positive prefix.
static void PutSigned(NumberWriter* w, const NumberCore* c, NumberSign sign, bool prefix,
                      bool perMille)
{
    int positive = prefix ? affixPositivePrefix : affixPositiveSuffix;
    int negative = prefix ? affixNegativePrefix : affixNegativeSuffix;
    if (sign == numberSignNone)
    {
        PutAffix(w, c, positive, false, perMille);
    }
    else if (sign == numberSignMinus)
    {
        PutAffix(w, c, negative, false, perMille);
    }
    else if (HasMinus(c, affixNegativePrefix) || HasMinus(c, affixNegativeSuffix))
    {
        PutAffix(w, c, negative, true, perMille);
    }
    else
    {
        if (prefix)
        {
            PutSymbol(w, c, numberPlus, mloc_numberPartPlusSign);
        }
        PutAffix(w, c, positive, false, perMille);
    }
}

static void PutDigit(NumberWriter* w, const NumberCore* c, uint8_t digit, uint32_t kind)
{
    Put(w, kind, c->digits + (size_t)digit * numberDigitSize, c->digitLength);
}

// Whether a group separator follows the digit at a power: at the primary
// size, then every secondary size.
static bool GroupAfter(const NumberCore* c, int64_t power)
{
    return power == c->primary || (power > c->primary && (power - c->primary) % c->secondary == 0);
}

// Writes an exponent: the exponential symbol, the minus sign when it is
// negative, and its digits.
static void PutExponent(NumberWriter* w, const NumberCore* c, int32_t exponent)
{
    PutSymbol(w, c, numberExponential, mloc_numberPartExponentSeparator);
    uint32_t magnitude = exponent < 0 ? 0u - (uint32_t)exponent : (uint32_t)exponent;
    if (exponent < 0)
    {
        PutSymbol(w, c, numberMinus, mloc_numberPartExponentMinusSign);
    }
    uint8_t digits[10];
    int count = 0;
    do
    {
        digits[count++] = (uint8_t)(magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);
    while (count > 0)
    {
        PutDigit(w, c, digits[--count], mloc_numberPartExponentInteger);
    }
}

// Writes a finite decimal's integer digits, grouped as the layout says,
// and the fraction digits it shows.
static void PutDigits(NumberWriter* w, const NumberCore* c, const mlocDecimal* d,
                      const NumberLayout* layout)
{
    int64_t top = mlocTopPower(d);
    int64_t integers = top >= 0 ? top + 1 : 0;
    integers = integers > (int64_t)layout->minInteger ? integers : (int64_t)layout->minInteger;
    uint32_t least = layout->grouping == mloc_groupingAlways ? 1
                     : layout->grouping == mloc_groupingMin2 ? 2
                                                             : c->minGrouping;
    bool grouped =
        layout->grouping != mloc_groupingOff && c->primary > 0 && integers >= c->primary + least;
    for (int64_t power = integers - 1; power >= 0; power--)
    {
        PutDigit(w, c, mlocDigitAt(d, power), mloc_numberPartInteger);
        if (grouped && power > 0 && GroupAfter(c, power))
        {
            PutSymbol(w, c, numberGroup, mloc_numberPartGroup);
        }
    }
    if (d->fractionDigits > 0)
    {
        PutSymbol(w, c, numberDecimal, mloc_numberPartDecimal);
        for (int64_t power = -1; power >= -(int64_t)d->fractionDigits; power--)
        {
            PutDigit(w, c, mlocDigitAt(d, power), mloc_numberPartFraction);
        }
    }
}

void mlocWriteNumber(const NumberCore* c, const mlocDecimal* d, NumberSign sign,
                     const NumberLayout* layout, NumberWriter* w)
{
    PutSigned(w, c, sign, true, layout->perMille);
    if (d->kind == mloc_decimalNaN)
    {
        PutSymbol(w, c, numberNaN, mloc_numberPartNaN);
    }
    else if (d->kind == mloc_decimalInfinity)
    {
        PutSymbol(w, c, numberInfinity, mloc_numberPartInfinity);
    }
    else
    {
        PutDigits(w, c, d, layout);
        if (layout->scientific)
        {
            PutExponent(w, c, layout->exponent);
        }
    }
    PutSigned(w, c, sign, false, layout->perMille);
}
