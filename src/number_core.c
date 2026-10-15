// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number formatters' core (number_core.h).

#include "number_core.h"

#include "bytes.h"
#include "currency_tables.h"
#include "data_format.h"
#include "data_view.h"
#include "decimal.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "locale_keyword.h"
#include "number_compact.h"
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

bool mlocUseNumberPattern(NumberCore* core, DataSpan patterns, uint32_t index)
{
    NumberTable t = mlocReadNumberTable(patterns, numberPatternSize);
    if (index >= t.recordCount)
    {
        return false;
    }
    const uint8_t* p = t.records + (size_t)index * numberPatternSize;
    core->affixText = t.text;
    for (int i = 0; i < affixCount; i++)
    {
        TableString(t, mlocLoad16(p + 2 * i), &core->affixStart[i], &core->affixLength[i]);
    }
    memcpy(core->parseStart, core->affixStart, sizeof(core->parseStart));
    memcpy(core->parseLength, core->affixLength, sizeof(core->parseLength));
    core->primary = p[patternPrimary];
    core->secondary = p[patternSecondary];
    return true;
}

// Fills a core from a system, an entry whose symbol set is in bounds and
// a pattern index.
static bool Fill(NumberCore* core, const Units* u, uint32_t system, const uint8_t* entry,
                 uint8_t minGrouping, uint32_t pattern)
{
    NumberTable symbols = mlocReadNumberTable(u->symbols, 2 * numberSymbolCount);
    uint32_t set = mlocLoad16(entry + entrySymbols);
    if (set >= symbols.recordCount)
    {
        return false;
    }
    *core = (NumberCore){
        .symbolText = symbols.text,
        .minGrouping = minGrouping,
    };
    const uint8_t* s = symbols.records + (size_t)set * 2 * numberSymbolCount;
    for (int i = 0; i < numberSymbolCount; i++)
    {
        TableString(symbols, mlocLoad16(s + 2 * i), &core->symbolStart[i], &core->symbolLength[i]);
    }
    core->decimalText = core->symbolText + core->symbolStart[numberDecimal];
    core->groupText = core->symbolText + core->symbolStart[numberGroup];
    if (!mlocUseNumberPattern(core, u->patterns, pattern))
    {
        return false;
    }
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

bool mlocNumberSystemDigits(const mlocDataSet* data, const char* id, size_t length,
                            uint8_t digits[10 * numberDigitSize], uint8_t* lengthOut)
{
    DataSpan systems;
    if (length > 8 || !mlocFindDataSetUnit(data, kindNumberSystems, nullptr, 0, &systems))
    {
        return false;
    }
    uint32_t system = FindSystem(systems, mlocPackSystem(id, (uint32_t)length));
    if (system == numberAlgorithmic)
    {
        return false;
    }
    const uint8_t* found = System(systems, system) + 8;
    memcpy(digits, found, 10 * numberDigitSize);
    uint8_t n = 1;
    while (n < numberDigitSize && found[n] != 0)
    {
        n++;
    }
    *lengthOut = n;
    return true;
}

// Whether a pattern's prefix holds a currency placeholder.
static bool CurrencyInPrefix(DataSpan patterns, uint32_t index)
{
    NumberTable t = mlocReadNumberTable(patterns, numberPatternSize);
    uint16_t start = 0;
    uint16_t length = 0;
    TableString(t, mlocLoad16(t.records + (size_t)index * numberPatternSize), &start, &length);
    for (uint32_t i = 0; i < length; i++)
    {
        uint8_t b = t.text[start + i];
        if (b >= numberAffixCurrency && b <= numberAffixCurrencyNarrow)
        {
            return true;
        }
    }
    return false;
}

// The index of an entry's currency pattern of a style; false when the
// data has no pattern sets or the indexes leave them.
static bool CurrencyPatternOf(const DataView* view, const Units* u, const uint8_t* entry,
                              const CurrencyStyle* style, uint32_t* patternOut, uint32_t* parseOut,
                              uint32_t* compactOut, CurrencyTexts* texts)
{
    DataSpan sets;
    if (!mlocFindDataUnit(view, kindCurrencyPatterns, dataNoLocale, &sets))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(sets, currencySetSize);
    NumberTable patterns = mlocReadNumberTable(u->patterns, numberPatternSize);
    uint32_t set = mlocLoad16(entry + entryCurrency);
    if (set >= t.recordCount)
    {
        return false;
    }
    const uint8_t* r = t.records + (size_t)set * currencySetSize;
    for (int i = 0; i < currencyPatternCount; i++)
    {
        if (mlocLoad16(r + 2 * i) >= patterns.recordCount)
        {
            return false;
        }
    }
    texts->text = t.text;
    for (int i = 0; i < 7; i++)
    {
        TableString(t, mlocLoad16(r + setUnitPatterns + 2 * i), &texts->start[i],
                    &texts->length[i]);
    }
    int base = style->accounting ? currencyAccounting : currencyStandard;
    // The alphaNextToNumber pattern when the currency's character closest
    // to the digits is neither a symbol nor a space.
    uint8_t edge = CurrencyInPrefix(u->patterns, mlocLoad16(r + 2 * base)) ? currencyEndsAlpha
                                                                           : currencyStartsAlpha;
    bool alpha = (style->spacing & edge) != 0;
    int chosen = style->hidden ? base + 2 : alpha ? base + 1 : base;
    *patternOut = mlocLoad16(r + 2 * chosen);
    *parseOut = mlocLoad16(r + 2 * (style->hidden ? chosen : base));
    *compactOut = mlocLoad16(r + (alpha ? setCompactAlpha : setCompact));
    // A long name takes the decimal pattern and tables; hidden, compact
    // notation takes the short decimal table (UTS #35).
    if (style->name)
    {
        *patternOut = mlocLoad16(entry + entryDecimal);
        *parseOut = *patternOut;
        *compactOut =
            mlocLoad16(entry + (style->longLength ? entryCompactLong : entryCompactShort));
    }
    else if (style->hidden)
    {
        *compactOut = mlocLoad16(entry + entryCompactShort);
    }
    return true;
}

// Points a core's parse affixes at a pattern CurrencyPatternOf checked
// (in the same patterns unit, so the same text), unless it is the one
// the core writes, whose affixes they already are.
static void UseParsePattern(NumberCore* core, DataSpan patterns, uint32_t index, uint32_t written)
{
    if (index == written)
    {
        return;
    }
    NumberTable t = mlocReadNumberTable(patterns, numberPatternSize);
    const uint8_t* p = t.records + (size_t)index * numberPatternSize;
    for (int i = 0; i < affixCount; i++)
    {
        TableString(t, mlocLoad16(p + 2 * i), &core->parseStart[i], &core->parseLength[i]);
    }
}

// Resolves a core and, when compact is set, the entry's compact table of
// a length and the compact patterns; with a currency style, its currency
// pattern and separators.
static mlocResult InitCore(NumberCore* coreOut, const mlocDataSet* data, const mlocLocale* locale,
                           mlocNumberStyle style, const CurrencyStyle* currency,
                           const uint8_t** compact, bool longLength, const uint8_t** patternsOut,
                           CurrencyTexts* texts)
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
    // Written in place: callers drop the core when making it fails.
    NumberCore* core = coreOut;
    int field = style == mloc_numberDecimal ? entryDecimal : entryPercent;
    uint32_t pattern = entry == nullptr ? 0 : mlocLoad16(entry + field);
    uint32_t parsePattern = pattern;
    uint32_t compactIndex =
        entry == nullptr ? 0
                         : mlocLoad16(entry + (longLength ? entryCompactLong : entryCompactShort));
    if (entry == nullptr ||
        (currency != nullptr && !CurrencyPatternOf(&view, &u, entry, currency, &pattern,
                                                   &parsePattern, &compactIndex, texts)) ||
        !Fill(core, &u, system, entry, header[localeMinGrouping], pattern))
    {
        return mloc_errorNotFound;
    }
    UseParsePattern(core, u.patterns, parsePattern, pattern);
    if (currency != nullptr)
    {
        core->decimalText = core->symbolText + core->symbolStart[numberCurrencyDecimal];
        core->groupText = core->symbolText + core->symbolStart[numberCurrencyGroup];
        core->symbolLength[numberDecimal] = core->symbolLength[numberCurrencyDecimal];
        core->symbolLength[numberGroup] = core->symbolLength[numberCurrencyGroup];
    }
    if (compact != nullptr)
    {
        DataSpan tables;
        DataSpan patterns;
        if (!mlocFindDataUnit(&view, kindNumberCompact, dataNoLocale, &tables) ||
            !mlocFindDataUnit(&view, kindNumberCompactPatterns, dataNoLocale, &patterns))
        {
            return mloc_errorNotFound;
        }
        const uint8_t* table = mlocFindCompactTable(tables, compactIndex);
        if (table == nullptr ||
            !mlocValidCompactUse((CompactData){.table = table, .patterns = patterns.bytes}))
        {
            return mloc_errorNotFound;
        }
        *compact = table;
        *patternsOut = patterns.bytes;
    }
    DataSpan sets;
    core->parseSets =
        mlocFindDataUnit(&view, kindParseSets, dataNoLocale, &sets) ? sets.bytes : nullptr;
    return fellBack ? mloc_fellBack : mloc_success;
}

mlocResult mlocInitNumberCore(NumberCore* coreOut, const mlocDataSet* data,
                              const mlocLocale* locale, mlocNumberStyle style)
{
    return InitCore(coreOut, data, locale, style, nullptr, nullptr, false, nullptr, nullptr);
}

mlocResult mlocInitCurrencyCore(NumberCore* coreOut, const mlocDataSet* data,
                                const mlocLocale* locale, const CurrencyStyle* style,
                                const uint8_t** tableOut, const uint8_t** patternsOut,
                                CurrencyTexts* textsOut)
{
    return InitCore(coreOut, data, locale, mloc_numberDecimal, style, tableOut, false, patternsOut,
                    textsOut);
}

mlocResult mlocInitCompactCore(NumberCore* coreOut, const mlocDataSet* data,
                               const mlocLocale* locale, bool longLength, const uint8_t** tableOut,
                               const uint8_t** patternsOut)
{
    return InitCore(coreOut, data, locale, mloc_numberDecimal, nullptr, tableOut, longLength,
                    patternsOut, nullptr);
}

bool mlocUseCompactPattern(NumberCore* core, const uint8_t* patterns, uint32_t index)
{
    NumberTable t = mlocReadNumberTable((DataSpan){patterns, 0}, numberCompactPatternSize);
    const uint8_t* p = t.records + (size_t)index * numberCompactPatternSize;
    const int fields[affixCount] = {compactPrefix, compactSuffix, compactNegativePrefix,
                                    compactSuffix};
    core->affixText = t.text;
    for (int i = 0; i < affixCount; i++)
    {
        TableString(t, mlocLoad16(p + fields[i]), &core->affixStart[i], &core->affixLength[i]);
    }
    return (p[compactFlags] & patternFlagNoNumber) != 0;
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
    kind |= w->source;
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
    else if (length > 0 && length <= w->capacity && w->length <= w->capacity - length)
    {
        // A sign or a separator, a few bytes, costs less copied by hand
        // than a call.
        if (length <= 4)
        {
            const uint8_t* from = bytes;
            for (size_t i = 0; i < length; i++)
            {
                w->buffer[w->length + i] = (char)from[i];
            }
        }
        else
        {
            memcpy(w->buffer + w->length, bytes, length);
        }
    }
    w->length += length;
}

void mlocWriteLiteral(NumberWriter* w, const char* text, size_t length)
{
    Put(w, mloc_numberPartLiteral, text, length);
}

void mlocWritePart(NumberWriter* w, uint32_t kind, const uint8_t* text, size_t length)
{
    Put(w, kind, text, length);
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
    const uint8_t* text = symbol == numberDecimal ? c->decimalText
                          : symbol == numberGroup ? c->groupText
                                                  : c->symbolText + c->symbolStart[symbol];
    Put(w, kind, text, c->symbolLength[symbol]);
}

// Whether a code point is one ICU trims from the ends of a unit or
// compact field: [[:Zs:][\u0009][:Bidi_Control:][:Variation_Selector:]]
// (its default ignorables).
static bool Ignorable(uint32_t c)
{
    return c == 0x09 || c == 0x20 || c == 0xA0 || c == 0x061C || c == 0x1680 ||
           (c >= 0x180B && c <= 0x180F && c != 0x180E) || (c >= 0x2000 && c <= 0x200A) ||
           c == 0x200E || c == 0x200F || (c >= 0x202A && c <= 0x202F) || c == 0x205F ||
           (c >= 0x2066 && c <= 0x2069) || c == 0x3000 || (c >= 0xFE00 && c <= 0xFE0F) ||
           (c >= 0xE0100 && c <= 0xE01EF);
}

// The code point of the well-formed UTF-8 character at text, given its
// length (1 to 4).
static uint32_t Decode(const uint8_t* text, size_t length)
{
    static const uint8_t s_leadMasks[5] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t c = text[0] & s_leadMasks[length];
    for (size_t i = 1; i < length; i++)
    {
        c = c << 6 | (text[i] & 0x3F);
    }
    return c;
}

// The length of the character at text, from its lead byte.
static size_t CharAt(const uint8_t* text, size_t length)
{
    size_t n = text[0] < 0x80 ? 1 : text[0] < 0xE0 ? 2 : text[0] < 0xF0 ? 3 : 4;
    return n <= length ? n : 0;
}

// The length of the ignorable at text, or 0.
static size_t IgnorableAt(const uint8_t* text, size_t length)
{
    size_t n = length == 0 ? 0 : CharAt(text, length);
    return n != 0 && Ignorable(Decode(text, n)) ? n : 0;
}

// The length of the ignorable ending text, or 0.
static size_t IgnorableBefore(const uint8_t* text, size_t length)
{
    size_t n = 1;
    while (n < 4 && n < length && (text[length - n] & 0xC0) == 0x80)
    {
        n++;
    }
    return n <= length && IgnorableAt(text + length - n, n) == n ? n : 0;
}

void mlocWriteTrimmed(NumberWriter* w, uint32_t kind, const uint8_t* text, size_t length)
{
    // The ends matter only to the parts; the text is the same.
    if (w->parts == nullptr)
    {
        Put(w, kind, text, length);
        return;
    }
    size_t lead = 0;
    for (size_t n = IgnorableAt(text, length); n != 0; n = IgnorableAt(text + lead, length - lead))
    {
        lead += n;
    }
    size_t trail = 0;
    for (size_t n = IgnorableBefore(text + lead, length - lead); n != 0;
         n = IgnorableBefore(text + lead, length - lead - trail))
    {
        trail += n;
    }
    Put(w, mloc_numberPartLiteral, text, lead);
    Put(w, kind, text + lead, length - lead - trail);
    Put(w, mloc_numberPartLiteral, text + length - trail, trail);
}

// Writes a run of an affix's text: literal, or in a compact pattern a
// compact part between the spaces at its ends.
static void PutText(NumberWriter* w, const uint8_t* text, size_t length, bool compact)
{
    if (compact)
    {
        mlocWriteTrimmed(w, mloc_numberPartCompact, text, length);
    }
    else
    {
        Put(w, mloc_numberPartLiteral, text, length);
    }
}

// What the minus sign's place in an affix writes: the minus or plus
// sign, the approximately sign alone or before either.
typedef enum SignSlot
{
    slotMinus,
    slotPlus,
    slotApproximately,
    slotApproximatelyMinus,
    slotApproximatelyPlus,
} SignSlot;

static void PutSlot(NumberWriter* w, const NumberCore* c, SignSlot slot)
{
    if (slot >= slotApproximately)
    {
        PutSymbol(w, c, numberApproximately, mloc_numberPartApproximatelySign);
    }
    if (slot == slotMinus || slot == slotApproximatelyMinus)
    {
        PutSymbol(w, c, numberMinus, mloc_numberPartMinusSign);
    }
    else if (slot == slotPlus || slot == slotApproximatelyPlus)
    {
        PutSymbol(w, c, numberPlus, mloc_numberPartPlusSign);
    }
}

// Writes an affix: its literal text, and the symbols its placeholders
// stand for, the minus sign's place as the slot says.
static void PutAffix(NumberWriter* w, const NumberCore* c, int affix, SignSlot slot,
                     const NumberLayout* layout)
{
    bool perMille = layout->perMille;
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
            PutText(w, text + literal, i - literal, layout->compact);
        }
        literal = i + 1;
        switch (b)
        {
        case numberAffixMinus:
            PutSlot(w, c, slot);
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
        case numberAffixCurrencyCode:
            Put(w, mloc_numberPartCurrency, c->currencyCode, sizeof(c->currencyCode));
            break;
        default:
            // The currency as the formatter shows it, its code when it has
            // no symbol to show.
            if (c->currencyText == nullptr)
            {
                Put(w, mloc_numberPartCurrency, c->currencyCode, sizeof(c->currencyCode));
            }
            else
            {
                Put(w, mloc_numberPartCurrency, c->currencyText, c->currencyLength);
            }
            break;
        }
    }
    if (length > literal)
    {
        PutText(w, text + literal, length - literal, layout->compact);
    }
}

// What a sign's place writes: the sign, after the approximately sign
// when there is one, or the approximately sign alone.
static SignSlot SlotOf(NumberSign sign, bool approximately)
{
    if (sign == numberSignNone)
    {
        return slotApproximately;
    }
    if (sign == numberSignMinus)
    {
        return approximately ? slotApproximatelyMinus : slotMinus;
    }
    return approximately ? slotApproximatelyPlus : slotPlus;
}

static bool HasMinus(const NumberCore* c, int affix)
{
    return memchr(c->affixText + c->affixStart[affix], numberAffixMinus, c->affixLength[affix]) !=
           nullptr;
}

// The prefix (or suffix) for a sign: a sign, or the approximately sign,
// takes the negative affixes in the minus sign's place. Negative affixes
// without that place (accounting's parentheses) stand for the minus sign
// themselves; a plus sign, or the approximately sign, then goes before
// the prefix.
static void PutSigned(NumberWriter* w, const NumberCore* c, NumberSign sign, bool prefix,
                      const NumberLayout* layout)
{
    int positive = prefix ? affixPositivePrefix : affixPositiveSuffix;
    int negative = prefix ? affixNegativePrefix : affixNegativeSuffix;
    bool approximately = layout->approximately;
    if (sign == numberSignNone && !approximately)
    {
        // Most patterns have an empty prefix or suffix: no call for it.
        if (c->affixLength[positive] > 0)
        {
            PutAffix(w, c, positive, slotMinus, layout);
        }
        return;
    }
    if (HasMinus(c, affixNegativePrefix) || HasMinus(c, affixNegativeSuffix))
    {
        PutAffix(w, c, negative, SlotOf(sign, approximately), layout);
        return;
    }
    if (prefix && (sign != numberSignMinus || approximately))
    {
        PutSlot(w, c, sign == numberSignMinus ? slotApproximately : SlotOf(sign, approximately));
    }
    PutAffix(w, c, sign == numberSignMinus ? negative : positive, slotMinus, layout);
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

// Text gathered for one copy into the writer, where a call for each
// digit of a number would cost more than the digit.
enum
{
    chunkSize = 256,
    // The room kept free before a digit: the digit's slot, and a short
    // separator copied by hand.
    chunkReserve = numberDigitSize + 16,
};

// Appends a separator to a chunk; returns the chunk's length.
static size_t Gather(NumberWriter* w, char* chunk, size_t n, const uint8_t* text, size_t length)
{
    if (length <= 16)
    {
        // Room was kept for it; a short copy by hand costs less than a
        // call.
        for (size_t i = 0; i < length; i++)
        {
            chunk[n + i] = (char)text[i];
        }
        return n + length;
    }
    Put(w, mloc_numberPartInteger, chunk, n);
    Put(w, mloc_numberPartInteger, text, length);
    return 0;
}

// The digit at an index of a decimal's digits, 0 outside them.
static inline uint8_t DigitIndexed(const mlocDecimal* d, int64_t index)
{
    return (uint64_t)index < d->count ? d->digits[index] : 0;
}

// PutDigits without a part function: the digits read in place and
// copied a slot at a time (numberDigitSize bytes, the slot's length
// kept) into chunks, the next group separator's power known ahead.
static void PutDigitsPlain(NumberWriter* w, const NumberCore* c, const mlocDecimal* d,
                           int64_t integers, bool grouped, bool point)
{
    char chunk[chunkSize];
    size_t n = 0;
    size_t digitLength = c->digitLength;
    int64_t index = mlocTopPower(d) - (integers - 1);
    // The highest power a separator follows, or -1 for none: the primary
    // size, then every secondary size above it.
    int64_t separator = -1;
    if (grouped && integers - 1 >= c->primary)
    {
        separator = c->primary + (integers - 1 - c->primary) / c->secondary * c->secondary;
    }
    for (int64_t power = integers - 1; power >= 0; power--, index++)
    {
        if (n > chunkSize - chunkReserve)
        {
            Put(w, mloc_numberPartInteger, chunk, n);
            n = 0;
        }
        memcpy(chunk + n, c->digits + (size_t)DigitIndexed(d, index) * numberDigitSize,
               numberDigitSize);
        n += digitLength;
        if (power == separator && power > 0)
        {
            n = Gather(w, chunk, n, c->groupText, c->symbolLength[numberGroup]);
            separator = separator == c->primary ? -1 : separator - c->secondary;
        }
    }
    if (d->fractionDigits > 0 || point)
    {
        if (n > chunkSize - chunkReserve)
        {
            Put(w, mloc_numberPartInteger, chunk, n);
            n = 0;
        }
        n = Gather(w, chunk, n, c->decimalText, c->symbolLength[numberDecimal]);
        for (int32_t i = 0; i < d->fractionDigits; i++, index++)
        {
            if (n > chunkSize - chunkReserve)
            {
                Put(w, mloc_numberPartInteger, chunk, n);
                n = 0;
            }
            memcpy(chunk + n, c->digits + (size_t)DigitIndexed(d, index) * numberDigitSize,
                   numberDigitSize);
            n += digitLength;
        }
    }
    Put(w, mloc_numberPartInteger, chunk, n);
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
    if (w->parts == nullptr)
    {
        PutDigitsPlain(w, c, d, integers, grouped, layout->point);
        return;
    }
    for (int64_t power = integers - 1; power >= 0; power--)
    {
        PutDigit(w, c, mlocDigitAt(d, power), mloc_numberPartInteger);
        if (grouped && power > 0 && GroupAfter(c, power))
        {
            PutSymbol(w, c, numberGroup, mloc_numberPartGroup);
        }
    }
    if (d->fractionDigits > 0 || layout->point)
    {
        PutSymbol(w, c, numberDecimal, mloc_numberPartDecimal);
        for (int64_t power = -1; power >= -(int64_t)d->fractionDigits; power--)
        {
            PutDigit(w, c, mlocDigitAt(d, power), mloc_numberPartFraction);
        }
    }
}

// Writes a prefix or suffix unless the layout skips it, for both ends of
// a range where it is shared.
static void PutEnd(NumberWriter* w, const NumberCore* c, NumberSign sign, bool prefix,
                   const NumberLayout* layout)
{
    if (prefix ? layout->skipPrefix : layout->skipSuffix)
    {
        return;
    }
    uint32_t source = w->source;
    w->source = (prefix ? layout->sharedPrefix : layout->sharedSuffix) ? 0 : source;
    PutSigned(w, c, sign, prefix, layout);
    w->source = source;
}

void mlocWriteNumber(const NumberCore* c, const mlocDecimal* d, NumberSign sign,
                     const NumberLayout* layout, NumberWriter* w)
{
    PutEnd(w, c, sign, true, layout);
    if (d->kind == mloc_decimalNaN)
    {
        PutSymbol(w, c, numberNaN, mloc_numberPartNaN);
    }
    else if (d->kind == mloc_decimalInfinity)
    {
        PutSymbol(w, c, numberInfinity, mloc_numberPartInfinity);
    }
    else if (!layout->noNumber)
    {
        PutDigits(w, c, d, layout);
        if (layout->scientific)
        {
            PutExponent(w, c, layout->exponent);
        }
    }
    PutEnd(w, c, sign, false, layout);
}

size_t mlocAffixCodePoints(const NumberCore* c, NumberSign sign, const NumberLayout* layout)
{
    char text[256] = {0};
    NumberWriter w = {.buffer = text, .capacity = sizeof(text)};
    PutSigned(&w, c, sign, true, layout);
    PutSigned(&w, c, sign, false, layout);
    size_t points = 0;
    for (size_t i = 0; i < w.length && i < sizeof(text); i++)
    {
        points += ((uint8_t)text[i] & 0xC0) != 0x80;
    }
    return points;
}
