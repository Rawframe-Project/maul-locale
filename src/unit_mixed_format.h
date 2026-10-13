// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Mixed units formatted ("5 ft, 3 in"), as ICU's
// MixedUnitLongNameHandler formats them.

#ifndef MLOC_UNIT_MIXED_FORMAT_H
#define MLOC_UNIT_MIXED_FORMAT_H

#include "unit_patterns.h"

#include <stddef.h>

// Whether an identifier names a mixed unit (its parts joined by "-and-").
bool mlocIsMixedUnit(const char* unit, size_t length);

// Fills a formatter's mixed unit: the identifier parsed, each part's
// patterns from used on in the pool, the unit list of the width.
// Returns mloc_success; mloc_errorInvalid for a notation other than
// standard, or parts that do not convert to each other alone;
// mloc_errorNotFound for data without the unit table, a part's names or
// the lists; the errors of mlocParseUnitId and mlocLoadUnitForms.
mlocResult mlocInitMixedUnit(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                             const char* unit, size_t length, const mlocUnitOptions* options,
                             size_t* used);

// Whether a formatter's mixed unit is as mlocInitMixedUnit leaves it.
bool mlocValidMixedUnit(const UnitFormat* u);

// Formats an amount of the largest part across the parts: mloc_success;
// mloc_errorCapacity when the text does not fit or a part passes the
// limits; mloc_errorInvalid for a null pointer or an invalid decimal.
mlocResult mlocFormatMixedUnit(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output);

#endif
