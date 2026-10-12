// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What the decimal part's files share: a digit's power of ten is the
// exponent plus its distance from the last digit (record mloc-0019).

#ifndef MLOC_DECIMAL_H
#define MLOC_DECIMAL_H

#include "maul-locale/decimal.h"

// The power of ten of the first digit (that of the last for zero).
static inline int64_t mlocTopPower(const mlocDecimal* d)
{
    return d->count == 0 ? d->exponent : (int64_t)d->exponent + d->count - 1;
}

// The digit at a power of ten.
static inline uint8_t mlocDigitAt(const mlocDecimal* d, int64_t power)
{
    if (d->count == 0 || power < d->exponent || power > mlocTopPower(d))
    {
        return 0;
    }
    return d->digits[(int64_t)d->count - 1 - (power - d->exponent)];
}

// Whether every field holds as the library writes it.
bool mlocValidDecimal(const mlocDecimal* d);

// The plural operands of a valid finite decimal.
void mlocFillPluralOperands(const mlocDecimal* d, mlocPluralOperands* operandsOut);

#endif
