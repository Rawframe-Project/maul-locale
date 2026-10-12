// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validator of the currency regions unit (currency_tables.h), for
// the kind list.

#ifndef MLOC_CURRENCY_REGION_H
#define MLOC_CURRENCY_REGION_H

#include "data_view.h"

bool mlocValidCurrencyRegions(const DataView* view, DataSpan payload);

#endif
