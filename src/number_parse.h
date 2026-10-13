// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number parsing (record mloc-0032): a formatter's text read back as a
// person types it, as ICU4C's DecimalFormat::parse reads it, into an
// exact decimal and the offset where the number ended.

#ifndef MLOC_NUMBER_PARSE_H
#define MLOC_NUMBER_PARSE_H

#include "data_view.h"
#include "number_format.h"

#include "maul-locale/number.h"

#include <stddef.h>

// Whether a parse sets unit is as parse_tables.h lays it out.
bool mlocValidParseSets(const DataView* view, DataSpan payload);

// Parses text with a formatter of standard or scientific notation, as
// mlocParseNumber does.
mlocResult mlocParseWith(const Formatter* formatter, const char* text, size_t length,
                         mlocParseMode mode, mlocDecimal* decimalOut, size_t* endOut);

#endif
