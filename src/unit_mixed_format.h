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

// Fills an output unit of a mixed unit whose identifier out->id holds,
// parsed from the text: each part's patterns in the slots from *slots on
// (advanced past them), the pool from *used on, and the formatter's unit
// list of the width when it has none yet. Returns mloc_success;
// mloc_errorInvalid for parts that do not convert to each other alone;
// mloc_errorCapacity when the slots run out; mloc_errorNotFound for data
// without the lists; the errors of mlocLoadUnitForms.
mlocResult mlocLoadMixedOutput(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                               const char* unit, size_t length, mlocUnitWidth width,
                               uint32_t* slots, size_t* used, UnitOutput* out);

// Fills a formatter's one output with a mixed unit, the unit table taken
// from the data. Returns mloc_success; mloc_errorInvalid for a notation
// other than standard; mloc_errorNotFound for data without the unit
// table; the errors of mlocParseUnitId and mlocLoadMixedOutput.
mlocResult mlocInitMixedUnit(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                             const char* unit, size_t length, const mlocUnitOptions* options,
                             size_t* used);

// Whether a formatter's mixed outputs are as mlocLoadMixedOutput leaves
// them.
bool mlocValidMixedUnit(const UnitFormat* u);

// Rounds a mixed unit's smallest part (MixedRound): by the precision the
// context points to, else an inexact value to 38 digits.
mlocResult mlocRoundSmallest(void* context, mlocDecimal* amount, bool exact);

// Writes an output's amounts across its parts into a writer of the
// caller's: mloc_success, or mloc_errorCapacity when a part passes the
// limits.
mlocResult mlocWriteMixedTo(const UnitFormat* u, const UnitOutput* o,
                            const mlocUnitAmounts* amounts, NumberWriter* writer);

// Writes an output's amounts across its parts: mloc_success;
// mloc_errorCapacity when the text does not fit or a part passes the
// limits.
mlocResult mlocWriteMixed(const UnitFormat* u, const UnitOutput* o, const mlocUnitAmounts* amounts,
                          mlocOutput* output);

// Formats an amount of the one output's largest part across its parts:
// mloc_success; mloc_errorCapacity when the text does not fit or a part
// passes the limits.
mlocResult mlocFormatMixedUnit(const UnitFormat* u, const mlocDecimal* decimal, mlocOutput* output);

#endif
