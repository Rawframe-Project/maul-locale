// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units formatted for a usage, as ICU's UsagePrefsHandler formats them:
// the output unit chosen by the value from the locale's preferences.

#ifndef MLOC_UNIT_USAGE_H
#define MLOC_UNIT_USAGE_H

#include "unit_patterns.h"

#include <stddef.h>

// Fills a formatter's outputs from an input unit's route for the
// options' usage: each output's identifier, threshold and increment, its
// patterns in the slots. Returns mloc_success; mloc_errorSyntax for a
// usage that is not well-formed, and the errors of mlocParseUnitId;
// mloc_errorInvalid for a notation other than standard, or an input
// that is mixed or does not convert; mloc_errorNotFound for data
// without the unit table, the preferences or an output's names;
// mloc_errorCapacity when the slots or the pool run out.
mlocResult mlocInitUsage(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                         const char* unit, size_t length, const mlocUnitOptions* options,
                         size_t* used);

// Whether a formatter's usage outputs are as mlocInitUsage leaves them.
bool mlocValidUsageOutputs(const UnitFormat* u);

// Formats an amount of the input unit in the output its value chooses:
// mloc_success; mloc_errorCapacity when the text does not fit or the
// value passes the limits.
mlocResult mlocFormatUsage(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output);

// Formats a range of two amounts of the input unit: in one output, a
// single unit, as a unit's range ("30–70 ft"); else each end whole in
// its output, joined by the spaced range separator ("30 ft – 1.2 mi").
// mloc_success; mloc_errorCapacity when the text does not fit or a value
// passes the limits.
mlocResult mlocFormatUsageRange(const UnitFormat* u, const mlocDecimal* start,
                                const mlocDecimal* end, mlocOutput* output);

#endif
