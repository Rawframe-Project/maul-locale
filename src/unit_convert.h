// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Exact unit conversion (UTS #35 Part 6, Unit Conversion; record
// mloc-0023): a core unit's size in its base unit as a fraction of
// fixed-width integers, and a decimal carried through two such sizes
// without rounding until the end.

#ifndef MLOC_UNIT_CONVERT_H
#define MLOC_UNIT_CONVERT_H

#include "unit_data.h"
#include "unit_id.h"
#include "unit_tables.h"
#include "wide.h"

#include "maul-locale/decimal.h"

// A core unit's size in its base unit, its dimensions, and how it
// converts: by its size, its size and an offset (a temperature alone),
// or the Beaufort table (beaufort alone).
typedef struct UnitScale
{
    Wide numerator;
    Wide denominator;
    Wide offsetNumerator;
    Wide offsetDenominator;
    int8_t dimensions[unitMaxDimensions];
    bool offset;
    bool beaufort;
} UnitScale;

// The scale of a parsed core unit. Returns mloc_success;
// mloc_errorInvalid for a mixed unit or one that does not convert (a
// currency, a unit without conversion data, beaufort in a compound);
// mloc_errorCapacity when its size passes the integers' width.
mlocResult mlocScaleUnit(const UnitTable* t, const UnitId* id, UnitScale* out);

// Converts a decimal between two scales of the same dimensions, or of
// opposite ones (a reciprocal, "mile-per-gallon" to
// "liter-per-100-kilometer"). The result has 40 significant digits
// rounded to odd: truncated, then with its last digit raised from 0 or
// 5 when the value is inexact, so rounding it again to 38 significant
// digits or fewer gives what rounding the exact value would. *exactOut
// tells whether the value was exact. Returns mloc_success;
// mloc_errorInvalid for scales that do not convert; mloc_errorCapacity
// when the integers or the decimal's exponent cannot hold the value.
mlocResult mlocConvertScaled(const UnitTable* t, const UnitScale* from, const UnitScale* to,
                             const mlocDecimal* value, mlocDecimal* out, bool* exactOut);

#endif
