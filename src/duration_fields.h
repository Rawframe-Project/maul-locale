// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration's fields as exact decimals (record mloc-0044): each double's
// value, and a field with the fractions after it added, in 128 bits.

#ifndef MLOC_DURATION_FIELDS_H
#define MLOC_DURATION_FIELDS_H

#include "maul-locale/duration.h"

// Whether a duration mlocMakeDuration takes is negative: a field below 0.
bool mlocDurationNegative(const mlocDurationValues* duration);

// The value of a field plus each of the next fractions fields over 1000
// times the one before (ECMA-402's ComputeFractionalDigits), negative
// (-0 for 0) when negative. The duration is one mlocMakeDuration takes.
void mlocDurationValue(const mlocDurationValues* duration, uint32_t field, uint32_t fractions,
                       bool negative, mlocDecimal* out);

#endif
