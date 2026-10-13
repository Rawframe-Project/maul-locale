// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number units' validators (number_data.h).

#include "number_data.h"

#include "bytes.h"
#include "currency_tables.h"
#include "number_tables.h"

// The length of the well-formed UTF-8 character at text (the Unicode
// Standard's table of well-formed byte sequences: no overlong form, no
// surrogate, nothing past U+10FFFF), or 0.
static uint32_t CharLength(const uint8_t* text, uint32_t available)
{
    uint8_t c = text[0];
    if (c < 0x80)
    {
        return 1;
    }
    uint32_t n = c >= 0xC2 && c <= 0xDF   ? 2
                 : c >= 0xE0 && c <= 0xEF ? 3
                 : c >= 0xF0 && c <= 0xF4 ? 4
                                          : 0;
    if (n == 0 || n > available)
    {
        return 0;
    }
    uint8_t low = c == 0xE0 ? 0xA0 : c == 0xF0 ? 0x90 : 0x80;
    uint8_t high = c == 0xED ? 0x9F : c == 0xF4 ? 0x8F : 0xBF;
    if (text[1] < low || text[1] > high)
    {
        return 0;
    }
    for (uint32_t i = 2; i < n; i++)
    {
        if (text[i] < 0x80 || text[i] > 0xBF)
        {
            return 0;
        }
    }
    return n;
}

// Whether text is well-formed UTF-8 whose bytes below 0x20 are at most
// the largest placeholder allowed (0 for none) and never NUL.
static bool ValidText(const uint8_t* text, uint32_t length, uint8_t maxPlaceholder)
{
    for (uint32_t at = 0; at < length;)
    {
        if (text[at] < 0x20)
        {
            if (text[at] == 0 || text[at] > maxPlaceholder)
            {
                return false;
            }
            at++;
            continue;
        }
        uint32_t n = CharLength(text + at, length - at);
        if (n == 0)
        {
            return false;
        }
        at += n;
    }
    return true;
}

bool mlocValidNumberSystems(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < numberSystemsHeaderSize || mlocLoad32(payload.bytes + 4) != 0)
    {
        return false;
    }
    uint32_t count = mlocLoad32(payload.bytes);
    if (count == 0 || count >= numberAlgorithmic ||
        payload.size != numberSystemsHeaderSize + (size_t)count * numberSystemSize)
    {
        return false;
    }
    uint64_t previous = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* entry =
            payload.bytes + numberSystemsHeaderSize + (size_t)i * numberSystemSize;
        uint64_t id = mlocLoad64(entry);
        if (!mlocValidSystemId(id) || id <= previous)
        {
            return false;
        }
        previous = id;
        uint32_t length = 0;
        for (int d = 0; d < 10; d++)
        {
            const uint8_t* digit = entry + 8 + d * numberDigitSize;
            // No digit is NUL, a control character or a space.
            uint32_t n = digit[0] <= 0x20 ? 0 : CharLength(digit, numberDigitSize);
            if (n == 0 || (d > 0 && n != length))
            {
                return false;
            }
            length = n;
            for (uint32_t k = n; k < numberDigitSize; k++)
            {
                if (digit[k] != 0)
                {
                    return false;
                }
            }
        }
    }
    return true;
}

NumberTable mlocReadNumberTable(DataSpan unit, uint32_t recordSize)
{
    NumberTable t = {
        .stringCount = mlocLoad16(unit.bytes),
        .recordCount = mlocLoad16(unit.bytes + 2),
    };
    t.offsets = unit.bytes + numberTableHeaderSize;
    t.records = t.offsets + (size_t)(t.stringCount + 1) * 2;
    t.text = t.records + (size_t)t.recordCount * recordSize;
    return t;
}

bool mlocValidNumberTable(DataSpan payload, uint32_t recordSize, uint8_t maxPlaceholder)
{
    if (payload.size < numberTableHeaderSize)
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, recordSize);
    uint32_t textSize = mlocLoad32(payload.bytes + 4);
    if (payload.size != numberTableHeaderSize + (size_t)(t.stringCount + 1) * 2 +
                            (size_t)t.recordCount * recordSize + textSize ||
        mlocLoad16(t.offsets) != 0 || mlocLoad16(t.offsets + (size_t)t.stringCount * 2) != textSize)
    {
        return false;
    }
    for (uint32_t s = 0; s < t.stringCount; s++)
    {
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        uint32_t end = mlocLoad16(t.offsets + (size_t)s * 2 + 2);
        // Each string ends within the text before any of it is read.
        if (end < start || end > textSize ||
            !ValidText(t.text + start, end - start, maxPlaceholder))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidNumberSymbols(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!mlocValidNumberTable(payload, 2 * numberSymbolCount, 0))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, 2 * numberSymbolCount);
    for (uint32_t i = 0; i < t.recordCount * numberSymbolCount; i++)
    {
        if (mlocLoad16(t.records + (size_t)i * 2) >= t.stringCount)
        {
            return false;
        }
    }
    return true;
}

static bool ValidPattern(const uint8_t* p, uint32_t stringCount)
{
    for (int a = 0; a < 4; a++)
    {
        if (mlocLoad16(p + 2 * a) >= stringCount)
        {
            return false;
        }
    }
    return p[patternMinInteger] <= numberMaxPatternDigits &&
           p[patternMinFraction] <= p[patternMaxFraction] &&
           p[patternMaxFraction] <= numberMaxPatternDigits && p[patternPrimary] <= numberMaxGroup &&
           p[patternSecondary] <= numberMaxGroup &&
           (p[patternPrimary] == 0) == (p[patternSecondary] == 0) &&
           p[patternExponentDigits] <= numberMaxGroup &&
           p[patternFlags] <= patternFlagExponentPlus && p[15] == 0;
}

bool mlocValidNumberPatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!mlocValidNumberTable(payload, numberPatternSize, numberAffixCurrencyNarrow))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, numberPatternSize);
    for (uint32_t i = 0; i < t.recordCount; i++)
    {
        if (!ValidPattern(t.records + (size_t)i * numberPatternSize, t.stringCount))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidNumbers(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < numberLocaleHeaderSize)
    {
        return false;
    }
    const uint8_t* h = payload.bytes;
    uint32_t count = h[localeEntryCount];
    if (count == 0 || payload.size != numberLocaleHeaderSize + count * numberEntrySize ||
        h[6] != 0 || h[7] != 0 || h[localeMinGrouping] < 1 || h[localeMinGrouping] > 4 ||
        h[localeDefaultSystem] >= numberAlgorithmic || h[localeNativeSystem] == numberNoSystem)
    {
        return false;
    }
    int previous = -1;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* e = h + numberLocaleHeaderSize + (size_t)i * numberEntrySize;
        if ((int)e[entrySystem] <= previous || e[entrySystem] >= numberAlgorithmic || e[1] != 0)
        {
            return false;
        }
        previous = e[entrySystem];
    }
    return true;
}

bool mlocValidNumberCompactPatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!mlocValidNumberTable(payload, numberCompactPatternSize, numberAffixCurrencyNarrow))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, numberCompactPatternSize);
    for (uint32_t i = 0; i < t.recordCount; i++)
    {
        const uint8_t* p = t.records + (size_t)i * numberCompactPatternSize;
        if (mlocLoad16(p + compactPrefix) >= t.stringCount ||
            mlocLoad16(p + compactSuffix) >= t.stringCount ||
            mlocLoad16(p + compactNegativePrefix) >= t.stringCount ||
            (p[compactFlags] & ~patternFlagNoNumber) != 0)
        {
            return false;
        }
    }
    return true;
}

// The size of a valid table at bytes with room for available bytes, or
// 0 when it is not one.
static uint32_t CompactTableSize(const uint8_t* bytes, uint32_t available)
{
    uint32_t at = 0;
    for (uint32_t r = 0; r < numberCompactRows; r++)
    {
        if (available - at < 2)
        {
            return 0;
        }
        uint32_t zeros = bytes[at];
        uint32_t mask = bytes[at + 1];
        uint32_t power = numberCompactFirstPower + r;
        uint32_t other = 1u << numberCompactOther;
        bool valid = zeros == 0 ? mask == 0
                                : zeros <= power + 1 && (mask & other) != 0 &&
                                      mask < 1u << numberCompactSlots;
        uint32_t slots = 0;
        for (uint32_t m = mask; m != 0; m &= m - 1)
        {
            slots++;
        }
        if (!valid || available - at - 2 < 2 * slots)
        {
            return 0;
        }
        at += 2 + 2 * slots;
    }
    return at;
}

bool mlocValidNumberCompact(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < numberCompactHeaderSize || mlocLoad32(payload.bytes + 4) != 0)
    {
        return false;
    }
    uint32_t count = mlocLoad32(payload.bytes);
    if ((payload.size - numberCompactHeaderSize) / 4 <= count)
    {
        return false;
    }
    const uint8_t* offsets = payload.bytes + numberCompactHeaderSize;
    const uint8_t* tables = offsets + 4 * ((size_t)count + 1);
    uint32_t size = payload.size - numberCompactHeaderSize - 4 * (count + 1);
    if (mlocLoad32(offsets) != 0 || mlocLoad32(offsets + 4 * (size_t)count) != size)
    {
        return false;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(offsets + 4 * (size_t)i + 4);
        // Each table ends within the bytes before any of it is read, and
        // none is empty: a table holds all its rows.
        if (end <= start || end > size ||
            CompactTableSize(tables + start, end - start) != end - start)
        {
            return false;
        }
    }
    return true;
}

const uint8_t* mlocFindCompactTable(DataSpan unit, uint32_t index)
{
    uint32_t count = mlocLoad32(unit.bytes);
    if (index >= count)
    {
        return nullptr;
    }
    const uint8_t* offsets = unit.bytes + numberCompactHeaderSize;
    return offsets + 4 * ((size_t)count + 1) + mlocLoad32(offsets + 4 * (size_t)index);
}

bool mlocValidCurrencyFractions(const DataView* view, DataSpan payload)
{
    (void)view;
    if (payload.size < currencyFractionsHeaderSize || mlocLoad32(payload.bytes + 4) != 0)
    {
        return false;
    }
    uint32_t count = mlocLoad32(payload.bytes);
    if (count == 0 ||
        (payload.size - currencyFractionsHeaderSize) / currencyFractionSize != count ||
        (payload.size - currencyFractionsHeaderSize) % currencyFractionSize != 0)
    {
        return false;
    }
    int32_t previous = -1;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* e =
            payload.bytes + currencyFractionsHeaderSize + (size_t)i * currencyFractionSize;
        int32_t code = mlocLoad16(e + fractionCode);
        // The default entry, code 0, comes first.
        if (code <= previous || (i == 0) != (code == 0) || code > currencyMaxCode ||
            e[fractionDigits] > currencyMaxDigits || e[fractionCashDigits] > currencyMaxDigits ||
            !mlocValidCurrencyRounding(e[fractionRounding]) ||
            !mlocValidCurrencyRounding(e[fractionCashRounding]) || e[6] != 0 || e[7] != 0)
        {
            return false;
        }
        previous = code;
    }
    return true;
}

CurrencyFraction mlocFindCurrencyFraction(DataSpan unit, uint16_t code)
{
    uint32_t low = 0;
    uint32_t high = mlocLoad32(unit.bytes);
    const uint8_t* entries = unit.bytes + currencyFractionsHeaderSize;
    // The default entry stands for a code the table lacks.
    const uint8_t* found = entries;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const uint8_t* e = entries + (size_t)middle * currencyFractionSize;
        uint16_t at = mlocLoad16(e + fractionCode);
        if (at == code)
        {
            found = e;
            break;
        }
        if (at < code)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return (CurrencyFraction){
        .digits = found[fractionDigits],
        .rounding = found[fractionRounding],
        .cashDigits = found[fractionCashDigits],
        .cashRounding = found[fractionCashRounding],
    };
}

bool mlocValidCurrencyPatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!mlocValidNumberTable(payload, currencySetSize, currencyTextOther))
    {
        return false;
    }
    NumberTable t = mlocReadNumberTable(payload, currencySetSize);
    for (uint32_t i = 0; i < t.recordCount; i++)
    {
        const uint8_t* r = t.records + (size_t)i * currencySetSize;
        for (uint32_t at = setUnitPatterns; at <= setAppend; at += 2)
        {
            if (mlocLoad16(r + at) >= t.stringCount)
            {
                return false;
            }
        }
    }
    // Each text has its number's place and its text's place once.
    for (uint32_t s = 0; s < t.stringCount; s++)
    {
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        uint32_t end = mlocLoad16(t.offsets + (size_t)s * 2 + 2);
        uint32_t places[3] = {0};
        for (uint32_t at = start; at < end; at++)
        {
            places[t.text[at] <= currencyTextOther ? t.text[at] : 0]++;
        }
        if (places[currencyTextNumber] != 1 || places[currencyTextOther] != 1)
        {
            return false;
        }
    }
    return true;
}

SparseTable mlocReadSparseTable(DataSpan unit)
{
    SparseTable t = {
        .stringCount = mlocLoad16(unit.bytes),
        .entryCount = mlocLoad16(unit.bytes + 2),
        .textSize = mlocLoad32(unit.bytes + 4),
        .rowSize = mlocLoad32(unit.bytes + 8),
    };
    t.offsets = unit.bytes + sparseHeaderSize;
    t.codes = t.offsets + 2 * ((size_t)t.stringCount + 1);
    t.rowOffsets = t.codes + 2 * (size_t)t.entryCount;
    t.rows = t.rowOffsets + 2 * ((size_t)t.entryCount + 1);
    t.text = t.rows + t.rowSize;
    return t;
}

// Whether a row of a sparse unit, its length given, has a mask of the
// unit's fields and their values, each string field's in range.
static bool ValidRow(const uint8_t* row, uint32_t length, uint32_t fieldCount,
                     uint32_t stringFields, uint32_t stringCount)
{
    uint32_t mask = row[0];
    uint32_t present = 0;
    for (uint32_t m = mask; m != 0; m &= m - 1)
    {
        present++;
    }
    if (mask == 0 || mask >= 1u << fieldCount || length != 1 + 2 * present)
    {
        return false;
    }
    const uint8_t* value = row + 1;
    for (uint32_t f = 0; f < fieldCount; f++)
    {
        if ((mask & 1u << f) == 0)
        {
            continue;
        }
        if ((stringFields & 1u << f) != 0 && mlocLoad16(value) >= stringCount)
        {
            return false;
        }
        value += 2;
    }
    return true;
}

bool mlocValidSparse(DataSpan payload, const SparseShape* shape)
{
    bool flags = shape->flags;
    if (payload.size < sparseHeaderSize)
    {
        return false;
    }
    // The size in 64 bits, so that the two u32 sizes cannot wrap on a
    // 32-bit target, and checked before any part's address is taken.
    uint64_t strings = mlocLoad16(payload.bytes);
    uint64_t entries = mlocLoad16(payload.bytes + 2);
    uint64_t size = sparseHeaderSize + 2 * (strings + 1) + 2 * entries + 2 * (entries + 1) +
                    mlocLoad32(payload.bytes + 4) + mlocLoad32(payload.bytes + 8) +
                    (flags ? strings : 0);
    if (payload.size != size)
    {
        return false;
    }
    SparseTable t = mlocReadSparseTable(payload);
    if (mlocLoad16(t.offsets) != 0 ||
        mlocLoad16(t.offsets + 2 * (size_t)t.stringCount) != t.textSize ||
        mlocLoad16(t.rowOffsets) != 0 ||
        mlocLoad16(t.rowOffsets + 2 * (size_t)t.entryCount) != t.rowSize)
    {
        return false;
    }
    for (uint32_t s = 0; s < t.stringCount; s++)
    {
        uint32_t start = mlocLoad16(t.offsets + (size_t)s * 2);
        uint32_t end = mlocLoad16(t.offsets + (size_t)s * 2 + 2);
        if (end < start + (shape->empty ? 0 : 1) || end > t.textSize ||
            !ValidText(t.text + start, end - start, 0) ||
            (flags && t.text[t.textSize + s] > (currencyStartsAlpha | currencyEndsAlpha)))
        {
            return false;
        }
    }
    uint32_t previous = 0;
    for (uint32_t e = 0; e < t.entryCount; e++)
    {
        uint32_t code = mlocLoad16(t.codes + (size_t)e * 2);
        uint32_t start = mlocLoad16(t.rowOffsets + (size_t)e * 2);
        uint32_t end = mlocLoad16(t.rowOffsets + (size_t)e * 2 + 2);
        // Each row ends within the rows before any of it is read.
        if ((e > 0 && code <= previous) || code < shape->minCode || code > shape->maxCode ||
            end <= start || end > t.rowSize ||
            !ValidRow(t.rows + start, end - start, shape->fieldCount, shape->stringFields,
                      t.stringCount))
        {
            return false;
        }
        previous = code;
    }
    return true;
}

bool mlocValidCurrencySymbols(const DataView* view, DataSpan payload)
{
    (void)view;
    static const SparseShape s_shape = {
        .fieldCount = currencyFieldCount,
        .stringFields = (1u << currencyFieldCount) - 1 - (1u << currencyOwnPattern),
        .minCode = 1,
        .maxCode = currencyMaxCode,
        .flags = true,
    };
    return mlocValidSparse(payload, &s_shape);
}

bool mlocValidCurrencyNames(const DataView* view, DataSpan payload)
{
    (void)view;
    static const SparseShape s_shape = {
        .fieldCount = currencyNameFieldCount,
        .stringFields = (1u << currencyNameFieldCount) - 1,
        .minCode = 1,
        .maxCode = currencyMaxCode,
    };
    return mlocValidSparse(payload, &s_shape);
}

const uint8_t* mlocFindSparseRow(const SparseTable* t, uint16_t code)
{
    uint32_t low = 0;
    uint32_t high = t->entryCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        uint16_t at = mlocLoad16(t->codes + (size_t)middle * 2);
        if (at == code)
        {
            return t->rows + mlocLoad16(t->rowOffsets + (size_t)middle * 2);
        }
        if (at < code)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return nullptr;
}

bool mlocSparseField(const uint8_t* row, uint32_t field, uint16_t* valueOut)
{
    uint32_t mask = row[0];
    if ((mask & 1u << field) == 0)
    {
        return false;
    }
    uint32_t before = 0;
    for (uint32_t m = mask & ((1u << field) - 1); m != 0; m &= m - 1)
    {
        before++;
    }
    *valueOut = mlocLoad16(row + 1 + 2 * before);
    return true;
}
