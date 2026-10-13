// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The core every number formatter embeds (record mloc-0021): a locale's
// digits, symbols, pattern and grouping resolved from the data, and the
// writing of a rounded decimal with them.

#ifndef MLOC_NUMBER_CORE_H
#define MLOC_NUMBER_CORE_H

#include "data_view.h"
#include "number_tables.h"

#include "maul-locale/number.h"

#include <stddef.h>

// Which sign a number shows.
typedef enum NumberSign
{
    numberSignNone,
    numberSignMinus,
    numberSignPlus,
} NumberSign;

// The affixes of a pattern, in its record's order.
enum
{
    affixPositivePrefix,
    affixPositiveSuffix,
    affixNegativePrefix,
    affixNegativeSuffix,
    affixCount,
};

typedef struct NumberCore
{
    // The texts of the symbol and pattern units.
    const uint8_t* symbolText;
    const uint8_t* affixText;
    uint16_t symbolStart[numberSymbolCount];
    uint16_t symbolLength[numberSymbolCount];
    uint16_t affixStart[affixCount];
    uint16_t affixLength[affixCount];
    // The affixes parsing reads, in the same text: the pattern's, but a
    // currency pattern's alphaNextToNumber variant, which ICU's
    // DecimalFormat does not take, reads as the pattern it varies.
    uint16_t parseStart[affixCount];
    uint16_t parseLength[affixCount];
    // The ten digits, numberDigitSize bytes apart, each digitLength long.
    uint8_t digits[10 * numberDigitSize];
    uint8_t digitLength;
    uint8_t primary;
    uint8_t secondary;
    uint8_t minGrouping;
    // The decimal and group separators' text: the symbol set's, its
    // currency ones, or a currency's own; their lengths are symbolLength's.
    const uint8_t* decimalText;
    const uint8_t* groupText;
    // A currency formatter's currency as shown (its symbol, a variant of
    // it, or its code), and its code, for the placeholders of a currency
    // pattern.
    const uint8_t* currencyText;
    uint8_t currencyLength;
    char currencyCode[3];
    // The sets parsing reads (parse_tables.h); null in data without them.
    const uint8_t* parseSets;
} NumberCore;

// Where a formatter writes: the text pass copies bytes while they fit
// and counts them all; the parts pass, given the output with its part
// function, reports each run of one part kind over the text already
// written.
typedef struct NumberWriter
{
    char* buffer;
    size_t capacity;
    size_t length;
    const mlocOutput* parts;
    uint32_t kind;
    size_t start;
    // The source of the parts being written, a range's start or end, or 0
    // for both: added to each part's kind.
    uint32_t source;
} NumberWriter;

// Resolves a locale's core: its numbers unit along the main chain, the
// numbering system its -u-nu keyword or its data names, and that
// system's entry (latn's when it has none of its own), with the decimal
// pattern or, for percent and per mille, the percent one. Returns
// mloc_success, mloc_fellBack when the system asked for has no digits
// and the default stands instead, or mloc_errorNotFound for data without
// numbers or whose units disagree.
mlocResult mlocInitNumberCore(NumberCore* coreOut, const mlocDataSet* data,
                              const mlocLocale* locale, mlocNumberStyle style);

// As mlocInitNumberCore, for compact notation: also the system's compact
// table of a length (long when longLength) and the compact patterns,
// every index checked. mloc_errorNotFound for data without them.
mlocResult mlocInitCompactCore(NumberCore* coreOut, const mlocDataSet* data,
                               const mlocLocale* locale, bool longLength, const uint8_t** tableOut,
                               const uint8_t** patternsOut);

// How a currency formatter's core takes its pattern: the locale's
// accounting or standard one, its noCurrency variant when the currency is
// hidden, else its alphaNextToNumber variant when the currency's
// character next to the digits is neither a symbol nor a space (spacing
// flags of the text shown, currency_tables.h).
// In compact notation, the short compact currency table, or its
// alphaNextToNumber one as for the pattern; hidden, the short decimal
// compact table. Both lengths take the short tables (CLDR has no long
// currency ones). With a long name, the decimal pattern and the decimal
// compact table of the length asked for, as UTS #35 and ICU say.
typedef struct CurrencyStyle
{
    bool accounting;
    bool hidden;
    bool name;
    bool longLength;
    uint8_t spacing;
} CurrencyStyle;

// The texts of a currency pattern set: the long name's unit patterns,
// zero to other, then the ISO append pattern (currency_tables.h).
typedef struct CurrencyTexts
{
    const uint8_t* text;
    uint16_t start[7];
    uint16_t length[7];
} CurrencyTexts;

// As mlocInitNumberCore, with the system's currency pattern of a style
// and the currency separators; with tableOut, as mlocInitCompactCore,
// the style's compact table. mloc_errorNotFound for data without the
// currency pattern sets.
mlocResult mlocInitCurrencyCore(NumberCore* coreOut, const mlocDataSet* data,
                                const mlocLocale* locale, const CurrencyStyle* style,
                                const uint8_t** tableOut, const uint8_t** patternsOut,
                                CurrencyTexts* textsOut);

// Points a core's pattern at a pattern of a valid patterns unit; false
// for an index past its records.
bool mlocUseNumberPattern(NumberCore* core, DataSpan patterns, uint32_t index);

// Points a core's affixes at a compact pattern of a valid compact
// patterns unit, keeping the decimal pattern's grouping, as ICU does;
// returns whether the pattern writes no digits.
bool mlocUseCompactPattern(NumberCore* core, const uint8_t* patterns, uint32_t index);

// The sign a rounded decimal shows under a sign display.
NumberSign mlocNumberSign(const mlocDecimal* decimal, mlocSignDisplay display);

// How a rounded decimal is written.
typedef struct NumberLayout
{
    mlocGrouping grouping;
    // At least this many integer digits, zeros before.
    uint32_t minInteger;
    // The percent placeholder writes the per mille sign.
    bool perMille;
    // Whether an exponent of ten follows the digits, and which.
    bool scientific;
    int32_t exponent;
    // The affixes are a compact pattern's: their text is a compact part
    // but for the spaces at its ends.
    bool compact;
    // The pattern writes no digits (a compact pattern for exactly 1).
    bool noNumber;
    // The approximately sign goes where the sign would.
    bool approximately;
    // A range collapses the affixes its ends share: the prefix is written
    // for the start alone and the suffix for the end, each for both.
    bool skipPrefix;
    bool skipSuffix;
    bool sharedPrefix;
    bool sharedSuffix;
    // The decimal separator is written with no fraction digits shown, as
    // a pattern ending in its point ("#,##0.") writes it.
    bool point;
} NumberLayout;

// Writes a rounded decimal: the affixes for its sign, the integer digits
// grouped as the layout says, the fraction digits it shows, and the
// exponent when the layout has one.
void mlocWriteNumber(const NumberCore* core, const mlocDecimal* decimal, NumberSign sign,
                     const NumberLayout* layout, NumberWriter* writer);

// The code points the prefix and suffix of a sign write.
size_t mlocAffixCodePoints(const NumberCore* core, NumberSign sign, const NumberLayout* layout);

// Writes literal text.
void mlocWriteLiteral(NumberWriter* writer, const char* text, size_t length);

// Writes text as a part of a kind.
void mlocWritePart(NumberWriter* writer, uint32_t kind, const uint8_t* text, size_t length);

// Writes text as a part of a kind between the spaces, bidi controls and
// variation selectors at its ends, which are literal, as ICU trims its
// unit and compact fields.
void mlocWriteTrimmed(NumberWriter* writer, uint32_t kind, const uint8_t* text, size_t length);

// Ends a writer's last part.
void mlocEndParts(NumberWriter* writer);

#endif
