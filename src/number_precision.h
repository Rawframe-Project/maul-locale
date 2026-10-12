// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a decimal by a number formatter's precision (record
// mloc-0021), as ECMA-402's FormatNumericToString does: fraction digits
// (ToRawFixed), significant digits (ToRawPrecision), or both with a
// priority, then the zeros shown cut to the minimum.

#ifndef MLOC_NUMBER_PRECISION_H
#define MLOC_NUMBER_PRECISION_H

#include "maul-locale/number.h"

// Whether a precision's fields are in range and agree with its kind.
bool mlocValidPrecision(const mlocPrecision* precision);

// Rounds a finite decimal by a valid precision; the zero precision leaves
// it as it is. Returns mloc_errorCapacity when the result passes the
// decimal's limits, the decimal then unchanged.
mlocResult mlocApplyPrecision(const mlocPrecision* precision, mlocDecimal* decimal);

// Multiplies a finite decimal by 10^power (2 for percent, 3 for per
// mille), its fraction digits shown moving with the point. Returns
// mloc_errorCapacity when the result passes 10^999999.
mlocResult mlocScaleDecimal(mlocDecimal* decimal, int32_t power);

// Turns a finite decimal into the mantissa of scientific notation (one
// integer digit) or engineering notation (one to three, the exponent a
// multiple of three), rounded by the precision as ECMA-402's
// ComputeExponent does: when rounding carries the mantissa to the next
// power, the exponent is chosen again. Fraction digits the decimal shows
// stay shown. Zero has the exponent 0.
mlocResult mlocApplyScientific(const mlocPrecision* precision, bool engineering,
                               mlocDecimal* decimal, int32_t* exponentOut);

#endif
