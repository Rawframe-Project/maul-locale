// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Exact quotients as binary64 numbers (records mloc-0001, mloc-0041):
// Temporal's totals are float64 values of exact rationals, which the
// library finds by integer arithmetic alone, correctly rounded (ties to
// even), the double assembled from its bits. Numerators and denominators
// are unsigned integers of 128 bits.

#ifndef MLOC_BINARY64_H
#define MLOC_BINARY64_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Wide
{
    uint64_t high;
    uint64_t low;
} Wide;

// a times b, exactly.
Wide mlocWideProduct(uint64_t a, uint64_t b);

// a plus b, where the sum stays below 2^128.
Wide mlocWideSum(Wide a, Wide b);

// a times a small factor, where the product stays below 2^128.
Wide mlocWideScaled(Wide a, uint64_t factor);

// The quotient of n / d, which must fit 64 bits, and the remainder.
uint64_t mlocWideQuotient(Wide n, Wide d, Wide* remainderOut);

// Whether a below b.
bool mlocWideBelow(Wide a, Wide b);

// A double's value as a sign and a magnitude, when it is a finite integer
// below 2^128.
bool mlocWideOfDouble(double value, bool* negativeOut, Wide* magnitudeOut);

// The binary64 nearest numerator / denominator, negated when negative;
// +0 for a zero numerator. The denominator is positive and below 2^74,
// and the quotient lies within binary64's normal range.
double mlocBinary64Of(bool negative, Wide numerator, Wide denominator);

#endif
