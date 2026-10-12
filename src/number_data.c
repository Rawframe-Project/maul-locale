// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number units' validators (number_data.h).

#include "number_data.h"

#include "bytes.h"
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

// Whether a table unit's sizes and string offsets hold, and each string
// is valid text.
static bool ValidTable(DataSpan payload, uint32_t recordSize, uint8_t maxPlaceholder)
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
        if (end < start || !ValidText(t.text + start, end - start, maxPlaceholder))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidNumberSymbols(const DataView* view, DataSpan payload)
{
    (void)view;
    if (!ValidTable(payload, 2 * numberSymbolCount, 0))
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
    if (!ValidTable(payload, numberPatternSize, numberAffixCurrencyNarrow))
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
