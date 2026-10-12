// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's currency units (record mloc-0022), shared by the
// library and the generator. All integers are little-endian.
//
// Currency fractions, a unit of no locale (supplementalData.xml's
// currencyData/fractions): a header (u32 entry count, u32 0), then the
// entries sorted by code, each currencyFractionSize bytes: the code
// packed (u16; 0 for the default entry, which comes first), the digits,
// the rounding, the cash digits and the cash rounding (u8 each), and 2
// bytes 0. A rounding counts units of the last digit, 0 meaning none.
//
// Currency pattern sets, a unit of no locale in the table shape of
// number_tables.h, with no strings: each record the indexes in the
// patterns unit of the standard pattern, its alphaNextToNumber and its
// noCurrency variants, then the accounting pattern's three (u16 each).
// A locale entry of the numbers unit names its set.
//
// Currency symbols, a unit per locale key that gives any of its own, in
// the sparse shape below: per currency the fields the key itself gives
// (its symbol, its narrow, variant and formal symbols as strings, its
// own pattern as an index in the patterns unit, its own decimal and group
// separators as strings), a field it does not give coming from its
// chain; then the spacing flags of each string (u8 each).
//
// The sparse shape: a header (u16 string count, u16 entry count, u32 text
// size, u32 row size), the string offsets into the text (u16 each, count
// + 1, from 0 to the text size), the entries' codes packed (u16 each,
// strictly ascending), the entries' row offsets into the rows (u16 each,
// count + 1, from 0 to the row size), the rows, then the text. A row is
// a mask (u8) of the fields present, a bit per field in field order, and
// a u16 for each field present, in field order. Strings hold no byte
// below 0x20.

#ifndef MLOC_CURRENCY_TABLES_H
#define MLOC_CURRENCY_TABLES_H

#include <stdint.h>

enum
{
    currencyFractionsHeaderSize = 8,
    currencyFractionSize = 8,
    currencySetSize = 12,
    sparseHeaderSize = 12,
    // The largest digit count a fraction entry holds.
    currencyMaxDigits = 6,
    // The largest packed code.
    currencyMaxCode = 26 * 26 * 26,
};

// The fields of a fraction entry.
enum
{
    fractionCode = 0,
    fractionDigits = 2,
    fractionRounding = 3,
    fractionCashDigits = 4,
    fractionCashRounding = 5,
};

// The patterns of a set, in its record's order.
typedef enum CurrencyPattern
{
    currencyStandard,
    currencyStandardAlpha,
    currencyStandardNone,
    currencyAccounting,
    currencyAccountingAlpha,
    currencyAccountingNone,
    currencyPatternCount,
} CurrencyPattern;

// The fields of a currency symbols entry, in its row's order.
typedef enum CurrencyField
{
    currencySymbol,
    currencyNarrow,
    currencyVariant,
    currencyFormal,
    currencyOwnPattern,
    currencyOwnDecimal,
    currencyOwnGroup,
    currencyFieldCount,
} CurrencyField;

// A string's spacing flags: whether its first character, and its last,
// is neither a symbol nor a space (General_Category S or Z): a letter, a
// digit, a mark, punctuation ("Fr.") or a format character. Such a
// character meets the digits through the alphaNextToNumber pattern; UTS
// #35 names letters, and the rest is root's currencySpacing, which ICU
// applies ("Fr. 1,234.50").
enum
{
    currencyStartsAlpha = 1,
    currencyEndsAlpha = 2,
};

// Whether a rounding is one the precision's increments can say: none, or
// 1, 2, 5, 10, 20, 25 or 50 units of the last digit.
static inline bool mlocValidCurrencyRounding(uint32_t rounding)
{
    return rounding == 0 || rounding == 1 || rounding == 2 || rounding == 5 || rounding == 10 ||
           rounding == 20 || rounding == 25 || rounding == 50;
}

// A currency code packed: three ASCII letters, any case, as 1 to 17576
// in alphabetical order; 0 when the code is not three letters.
static inline uint16_t mlocPackCurrency(const char* code, uint32_t length)
{
    if (length != 3)
    {
        return 0;
    }
    uint32_t packed = 0;
    for (uint32_t i = 0; i < 3; i++)
    {
        char c = code[i];
        c = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
        if (c < 'A' || c > 'Z')
        {
            return 0;
        }
        packed = packed * 26 + (uint32_t)(c - 'A');
    }
    return (uint16_t)(packed + 1);
}

// The three letters of a packed code (1 to 17576).
static inline void mlocUnpackCurrency(uint16_t packed, char* out)
{
    uint32_t value = (uint32_t)packed - 1;
    out[2] = (char)('A' + value % 26);
    out[1] = (char)('A' + value / 26 % 26);
    out[0] = (char)('A' + value / 676);
}

#endif
