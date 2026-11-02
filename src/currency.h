// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A currency's symbol in place in the data, for the names part.

#ifndef MLOC_CURRENCY_H
#define MLOC_CURRENCY_H

#include "maul-locale/currency.h"

#include <stddef.h>

// A currency's symbol in a locale, as mlocGetCurrencySymbol finds it for
// mloc_currencySymbol: its text in the data, however long; false when
// the code is not three letters or the locale's chain gives no symbol.
bool mlocFindCurrencySymbol(const mlocDataSet* data, const mlocLocale* locale, const char* code,
                            size_t codeLength, const char** textOut, size_t* lengthOut);

#endif
