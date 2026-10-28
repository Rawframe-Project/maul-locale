// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Exact unit conversion (UTS #35 Part 6, Unit Conversion): a core unit's
// size in its base unit as a fraction of fixed-width integers, and a
// decimal carried through two such sizes without rounding until the end.

#ifndef MLOC_UNIT_CONVERT_H
#define MLOC_UNIT_CONVERT_H

#include "unit_data.h"
#include "unit_id.h"
#include "unit_tables.h"
#include "wide.h"

#include "maul-locale/number.h"

// A core unit's size in its base unit, its dimensions, and how it
// converts: by its size, its size and an offset (a temperature alone),
// or the Beaufort table (beaufort alone).
typedef struct UnitScale
{
    Wide numerator;
    Wide denominator;
    Wide offsetNumerator;
    Wide offsetDenominator;
    int16_t dimensions[unitMaxDimensions];
    bool offset;
    bool beaufort;
} UnitScale;

// A value in flight: ±a / b * 10^e with the power kept out of the
// integers, or an infinity.
typedef struct UnitValue
{
    Wide a;
    Wide b;
    int64_t e;
    bool negative;
    bool infinite;
} UnitValue;

// The scale of a parsed core unit. Returns mloc_success;
// mloc_errorInvalid for a mixed unit or one that does not convert (a
// currency, a unit without conversion data, beaufort in a compound);
// mloc_errorLimit when its size passes the integers' width.
mlocResult mlocScaleUnit(const UnitTable* t, const UnitId* id, UnitScale* out);

// Converts a decimal between two scales of the same dimensions, or of
// opposite ones (a reciprocal, "mile-per-gallon" to
// "liter-per-100-kilometer"). The result has 40 significant digits
// rounded to odd: truncated, then with its last digit raised from 0 or
// 5 when the value is inexact, so rounding it again to 38 significant
// digits or fewer gives what rounding the exact value would. *exactOut
// tells whether the value was exact. Returns mloc_success;
// mloc_errorInvalid for scales that do not convert; mloc_errorLimit
// when the integers or the decimal's exponent cannot hold the value.
mlocResult mlocConvertScaled(const UnitTable* t, const UnitScale* from, const UnitScale* to,
                             const mlocDecimal* value, mlocDecimal* out, bool* exactOut);

// A finite or infinite decimal's value; false when its digits do not fit.
bool mlocUnitValueOf(const mlocDecimal* d, UnitValue* out);

// A value in a scale's unit to its base unit, and back; false when the
// integers cannot hold it.
bool mlocUnitValueToBase(const UnitTable* t, const UnitScale* scale, UnitValue* x);
bool mlocUnitValueFromBase(const UnitTable* t, const UnitScale* scale, UnitValue* x);

// x = 1 / x: zero gives an infinity and an infinity zero.
void mlocInvertUnitValue(UnitValue* x);

// Whether |x| is at least n / d (d not zero).
bool mlocUnitValueAtLeast(const UnitValue* x, const Wide* n, const Wide* d);

// Takes the integer part of a finite |x| out: *integerOut receives it and
// x keeps the rest, positive. False when the power cannot be folded in.
bool mlocSplitUnitValue(UnitValue* x, Wide* integerOut);

// An integer's value.
UnitValue mlocUnitValueOfInteger(const Wide* integer);

// Writes a value as 40 digits rounded to odd (mlocConvertScaled);
// mloc_errorLimit when a decimal cannot hold it.
mlocResult mlocUnitValueDecimal(const UnitValue* x, mlocDecimal* out, bool* exactOut);

// Rounds a converted value: by a precision with a kind, else an exact one
// stays and an inexact one rounds half even to 38 significant digits.
mlocResult mlocRoundConverted(mlocDecimal* d, bool exact, const mlocPrecision* precision);

#endif
