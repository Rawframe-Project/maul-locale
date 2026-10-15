// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Temporal's rounding of integers (records mloc-0040, mloc-0041):
// GetUnsignedRoundingMode, ApplyUnsignedRoundingMode on an exact
// fraction, and RoundNumberToIncrement.

#ifndef MLOC_ROUNDING_H
#define MLOC_ROUNDING_H

#include "maul-locale/decimal.h"

#include <stdint.h>

// The unsigned rounding mode of GetUnsignedRoundingMode.
typedef enum RoundingUnsigned
{
    roundingZero,
    roundingInfinity,
    roundingHalfZero,
    roundingHalfInfinity,
    roundingHalfEven,
} RoundingUnsigned;

RoundingUnsigned mlocUnsignedRoundingMode(mlocRoundingMode mode, bool negative);

// ApplyUnsignedRoundingMode for x = r1 + (r2 - r1) * part / whole, with
// 0 <= part < whole: whether x rounds to r2. evenLow says whether r1 is
// an even multiple of the step, for half-even.
bool mlocRoundsUp(int64_t part, int64_t whole, RoundingUnsigned mode, bool evenLow);

// RoundNumberToIncrement with a mode, signed.
int64_t mlocRoundToIncrement(int64_t value, int64_t increment, mlocRoundingMode mode);

#endif
