// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency parsing (record mloc-0033): ICU4C 79's CombinedCurrencyMatcher,
// which reads a currency formatter's currency (its symbol, its code, its
// long names) or, for any currency, the symbols, codes and names the
// locale's chain gives, as uprv_parseCurrency reads them.

#ifndef MLOC_PARSE_CURRENCY_H
#define MLOC_PARSE_CURRENCY_H

#include "data_view.h"
#include "parse_match.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // The units of a kind a chain holds; a locale's chain has a few.
    currencyChainUnits = 8,
    // The outer patterns of a currency: one per plural category at most.
    parseMaxOuters = 6,
};

// The currency units along a locale's chain, each with its step (0 for
// the locale's own key), in the chain's order.
typedef struct CurrencyChain
{
    DataSpan symbols[currencyChainUnits];
    DataSpan names[currencyChainUnits];
    uint8_t symbolSteps[currencyChainUnits];
    uint8_t nameSteps[currencyChainUnits];
    uint8_t symbolCount;
    uint8_t nameCount;
    // Every unit of the chain fit; any currency is read only then.
    bool complete;
    // The locale uppercases the Turkic way (tr, az): i to dotted I.
    bool turkic;
} CurrencyChain;

struct ParseCurrency
{
    char code[3];
    // The currency's symbol (its code when the chain gives none), and the
    // text the formatter shows when it is another (a narrow symbol).
    const uint8_t* symbol;
    uint16_t symbolLength;
    const uint8_t* shown;
    uint16_t shownLength;
    // The long name of each plural category, zero to other, as ICU's
    // getPluralName gives it; null without the names data.
    const uint8_t* names[6];
    uint16_t nameLengths[6];
    // When the formatter writes the currency outside its pattern (a long
    // name, the ISO append), that pattern's text before the number and
    // after it as affix patterns, ICU's currency plural patterns: the
    // currency's place is the name placeholder, or the code the ISO
    // append writes. A length past parseMaxAffixText does not fit. One
    // pair per distinct pattern of the plural categories (Romanian writes
    // "6,00 euro" and "20 de euro"), other's first.
    uint8_t before[parseMaxOuters][parseMaxAffixText + 1];
    uint8_t after[parseMaxOuters][parseMaxAffixText + 1];
    uint8_t beforeLength[parseMaxOuters];
    uint8_t afterLength[parseMaxOuters];
    uint8_t outerCount;
    bool outer;
    // For any currency, the chain; null for the formatter's alone.
    const CurrencyChain* chain;
};

// The currency matcher: the currency spacing NBSP after a number, the
// currency, then the spacing before a number.
void mlocMatchCurrency(const Parser* parser, Segment* segment, Parsed* parsed);

#endif
