// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A currency formatter's fields (currency.c), which a white-box test
// forges.

#ifndef MLOC_CURRENCY_FORMAT_H
#define MLOC_CURRENCY_FORMAT_H

#include "number_format.h"
#include "parse_currency.h"

#include "maul-locale/currency.h"

#include <stdbool.h>

enum
{
    // Marks a currency formatter the library made.
    currencyStamp = 0x43,
};

// The opaque value's contents: the formatter, the pattern around its
// number when it has one, and the currency units of its locale's chain,
// which parsing reads.
typedef struct Currency
{
    Formatter formatter;
    Outer outer;
    bool hasOuter;
    // The outer pattern is the ISO append's.
    bool appended;
    CurrencyChain chain;
} Currency;

static_assert(sizeof(Currency) <= sizeof(mlocCurrencyFormatter),
              "a currency formatter fits its opaque value");

#endif
