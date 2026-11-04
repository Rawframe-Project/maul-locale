// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What the decimal part's files share: a digit's power of ten is the
// exponent plus its distance from the last digit.

#ifndef MLOC_DECIMAL_H
#define MLOC_DECIMAL_H

#include "maul-locale/decimal.h"

#include <string.h>

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
MLOC_NODISCARD bool mlocValidDecimal(const mlocDecimal* d);

// The bytes of a valid decimal's plain text (mlocFormatDecimal's) from
// offset from on, at most capacity of them, written into out; returns
// how many, and gives the whole text's length in *lengthOut. A text far
// longer than any buffer, up to 10^MLOC_MAX_DECIMAL_EXPONENT, is written
// a piece at a time.
size_t mlocDecimalText(const mlocDecimal* decimal, size_t from, char* out, size_t capacity,
                       size_t* lengthOut);

// Whether two valid decimals hold the same value and show the same
// digits, compared field by field: a decimal's padding is no part of it,
// and a caller's may hold anything.
MLOC_NODISCARD static inline bool mlocSameDecimal(const mlocDecimal* a, const mlocDecimal* b)
{
    return a->kind == b->kind && a->negative == b->negative && a->count == b->count &&
           a->exponent == b->exponent && a->fractionDigits == b->fractionDigits &&
           memcmp(a->digits, b->digits, sizeof(a->digits)) == 0;
}

// The plural operands of a valid finite decimal.
void mlocFillPluralOperands(const mlocDecimal* d, mlocPluralOperands* operandsOut);

#endif
