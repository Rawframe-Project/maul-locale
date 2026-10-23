// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unsigned integers of fixed width for exact unit conversion: 2,048 bits
// in 32-bit limbs, least significant first, on the caller's stack. An
// operation whose result does not fit reports it and leaves its output
// unspecified. The generator shares this file.

#ifndef MLOC_WIDE_H
#define MLOC_WIDE_H

#include <stdint.h>

enum
{
    wideLimbs = 64,
};

// A value and the limbs it uses: limbs past count are 0, and the top
// used limb is not 0 (count is 0 for zero).
typedef struct Wide
{
    uint32_t limb[wideLimbs];
    uint32_t count;
} Wide;

// The value of a 64-bit integer.
Wide mlocWideOf(uint64_t value);

bool mlocWideIsZero(const Wide* a);

// -1, 0 or 1 as a is less than, equal to or greater than b.
int mlocWideCompare(const Wide* a, const Wide* b);

// out = a * b; false when it does not fit. out may be a or b.
bool mlocWideMultiply(Wide* out, const Wide* a, const Wide* b);

// a = a * factor + addend; false when it does not fit.
bool mlocWideMultiplyAdd(Wide* a, uint32_t factor, uint32_t addend);

// out = a + b; false when it does not fit. out may be a or b.
bool mlocWideAdd(Wide* out, const Wide* a, const Wide* b);

// out = a - b for a at least b. out may be a or b.
void mlocWideSubtract(Wide* out, const Wide* a, const Wide* b);

// a = a * 10^power; false when it does not fit.
bool mlocWideScale10(Wide* a, uint32_t power);

// a = a / divisor, giving the remainder; divisor is not 0.
uint32_t mlocWideDivideSmall(Wide* a, uint32_t divisor);

// quotient = numerator / divisor and remainder the rest, divisor not
// zero. Either output may be null; neither may alias an input.
void mlocWideDivide(const Wide* numerator, const Wide* divisor, Wide* quotient, Wide* remainder);

// The decimal digits of a value, 1 for zero.
uint32_t mlocWideDigitCount(const Wide* a);

#endif
