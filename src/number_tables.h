// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's units (record mloc-0021), shared by the library and
// the generator. All integers are little-endian.
//
// Numbering systems, a unit of no locale: a header (u32 count, u32 0),
// then per numeric system, sorted by id: the id packed (u64, the first
// character in the low byte, NUL-padded) and its ten digits, each the
// UTF-8 of one code point NUL-padded to 4 bytes, all of one length.
//
// Symbol sets and patterns, units of no locale, share one shape: a
// header (u16 string count, u16 record count, u32 text size), the string
// offsets into the text (u16 each, count + 1 of them, from 0 to the text
// size), the records, then the text. A symbol set is numberSymbolCount
// string indexes (u16); a pattern is numberPatternSize bytes (below).
// Symbol strings hold no byte below 0x20; pattern affixes hold the
// placeholder bytes numberAffixMinus to numberAffixCurrencyNarrow for
// the symbols they stand for.
//
// A locale's numbers, a unit per locale filed where the resolved data
// differs from its chain's: a header (the indexes in the numbering
// systems unit of its default, native, traditional and finance systems,
// u8 each, numberNoSystem for none and numberAlgorithmic for a system
// with no digits; u8 minimum grouping digits, u8 entry count, 2 bytes 0),
// then per numbering system whose data differs from latn's, and latn, in
// the order of the systems: its index (u8), a 0, and the indexes of its
// symbol set, its decimal and percent patterns and its short and long
// compact tables (u16 each). Scientific notation writes the decimal
// pattern's affixes, as ICU does: the locales' scientific patterns are
// not read.
//
// Compact patterns, a unit of no locale in the table shape: each record
// the string indexes of its prefix, suffix and negative prefix (the
// negative suffix is the suffix; u16 each) and its flags (u8): a pattern
// with patternFlagNoNumber writes no digits (Italian's "mille" for one
// thousand).
//
// Compact tables, a unit of no locale: a header (u32 table count, u32
// 0), the tables' offsets into the bytes that follow (u32 each, count + 1
// of them, from 0 to their size), then the tables. A table is a row for
// each power of ten from 10^3 to 10^14: the other pattern's zeros (u8; 0
// where the locale does not compact at that power, CLDR's pattern "0"),
// which give the divisor 10^(power - zeros + 1); a mask (u8) of the
// slots present, a bit per plural category from zero to other and one
// for the exact value 1 (CLDR's count="1"), the other one's bit set
// exactly when the zeros are not 0; then the compact pattern index of
// each slot present (u16), in slot order. A category absent takes the
// other one's pattern.

#ifndef MLOC_NUMBER_TABLES_H
#define MLOC_NUMBER_TABLES_H

#include <stdint.h>

enum
{
    numberSystemsHeaderSize = 8,
    numberSystemSize = 48,
    numberDigitSize = 4,
    numberTableHeaderSize = 8,
    numberPatternSize = 16,
    numberLocaleHeaderSize = 8,
    numberEntrySize = 12,
    // A compact table: rows of the powers 10^3 to 10^14, slots of the six
    // categories and the exact value 1.
    numberCompactFirstPower = 3,
    numberCompactRows = 12,
    numberCompactSlots = 7,
    numberCompactOther = 5,
    numberCompactExactOne = 6,
    numberCompactHeaderSize = 8,
    numberCompactPatternSize = 7,
    // A locale unit's system index for none, and for a system with no
    // digits (an algorithmic one, which the spellout part formats).
    numberNoSystem = 0xFF,
    numberAlgorithmic = 0xFE,
    // The largest group size and exponent digit count a pattern holds.
    numberMaxGroup = 9,
    // The largest minimum integer and fraction digit count a pattern
    // holds.
    numberMaxPatternDigits = 40,
};

// The symbols of a set, in the order of its indexes.
typedef enum NumberSymbol
{
    numberDecimal,
    numberGroup,
    numberMinus,
    numberPlus,
    numberPercent,
    numberPerMille,
    numberExponential,
    numberSuperscript,
    numberInfinity,
    numberNaN,
    numberApproximately,
    numberCurrencyDecimal,
    numberCurrencyGroup,
    // What stands between a range's ends: CLDR's range pattern without
    // its {0} and {1}.
    numberRangeSeparator,
    numberSymbolCount,
} NumberSymbol;

// Placeholder bytes in pattern affixes.
enum
{
    numberAffixMinus = 1,
    numberAffixPlus,
    numberAffixPercent,
    numberAffixPerMille,
    // The currency's symbol (¤), ISO code (¤¤), name (¤¤¤), narrow
    // symbol (¤¤¤¤¤).
    numberAffixCurrency,
    numberAffixCurrencyCode,
    numberAffixCurrencyName,
    numberAffixCurrencyNarrow,
};

// A pattern's bytes: the string indexes of the positive prefix and
// suffix and of the negative ones (u16 each), then the minimum integer
// digits, the minimum and maximum fraction digits, the primary and
// secondary group sizes (0 for no grouping), the exponent's minimum
// digits (0 for no exponent), flags, and a 0.
enum
{
    patternPositivePrefix = 0,
    patternPositiveSuffix = 2,
    patternNegativePrefix = 4,
    patternNegativeSuffix = 6,
    patternMinInteger = 8,
    patternMinFraction = 9,
    patternMaxFraction = 10,
    patternPrimary = 11,
    patternSecondary = 12,
    patternExponentDigits = 13,
    patternFlags = 14,
    // The exponent shows a plus sign when positive.
    patternFlagExponentPlus = 1,
    // The pattern writes no digits: a compact pattern for exactly 1
    // ("mille"); the only flag of a compact pattern.
    patternFlagNoNumber = 2,
};

// The fields of a locale unit's header and entries.
enum
{
    localeDefaultSystem = 0,
    localeNativeSystem = 1,
    localeTraditionalSystem = 2,
    localeFinanceSystem = 3,
    localeMinGrouping = 4,
    localeEntryCount = 5,
    entrySystem = 0,
    entrySymbols = 2,
    entryDecimal = 4,
    entryPercent = 6,
    entryCompactShort = 8,
    entryCompactLong = 10,
    compactPrefix = 0,
    compactSuffix = 2,
    compactNegativePrefix = 4,
    compactFlags = 6,
};

// A numbering system id packed, or 0 when it is not 1 to 8 lowercase
// letters and digits.
static inline uint64_t mlocPackSystem(const char* id, uint32_t length)
{
    if (length == 0 || length > 8)
    {
        return 0;
    }
    uint64_t packed = 0;
    for (uint32_t i = 0; i < length; i++)
    {
        char c = id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            return 0;
        }
        packed |= (uint64_t)(uint8_t)c << (8 * i);
    }
    return packed;
}

// Whether a packed id is one mlocPackSystem gives.
static inline bool mlocValidSystemId(uint64_t packed)
{
    bool ended = false;
    for (int i = 0; i < 8; i++)
    {
        char c = (char)(packed >> (8 * i));
        if (c == '\0')
        {
            ended = true;
        }
        else if (ended || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
        {
            return false;
        }
    }
    return packed != 0;
}

// The packed id of latn.
constexpr uint64_t numberLatn = 0x6E74616Cu;

#endif
