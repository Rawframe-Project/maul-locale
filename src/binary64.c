// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Exact quotients as binary64 numbers (binary64.h).

#include "binary64.h"

#include <string.h>

Wide mlocWideProduct(uint64_t a, uint64_t b)
{
    uint64_t aLow = a & 0xFFFFFFFFu;
    uint64_t aHigh = a >> 32;
    uint64_t bLow = b & 0xFFFFFFFFu;
    uint64_t bHigh = b >> 32;
    uint64_t low = aLow * bLow;
    uint64_t middle1 = aHigh * bLow;
    uint64_t middle2 = aLow * bHigh;
    uint64_t high = aHigh * bHigh;
    uint64_t carry = ((low >> 32) + (middle1 & 0xFFFFFFFFu) + (middle2 & 0xFFFFFFFFu)) >> 32;
    return (Wide){high + (middle1 >> 32) + (middle2 >> 32) + carry,
                  low + (middle1 << 32) + (middle2 << 32)};
}

Wide mlocWideSum(Wide a, Wide b)
{
    uint64_t low = a.low + b.low;
    return (Wide){a.high + b.high + (low < a.low ? 1 : 0), low};
}

Wide mlocWideScaled(Wide a, uint64_t factor)
{
    Wide low = mlocWideProduct(a.low, factor);
    return (Wide){low.high + a.high * factor, low.low};
}

static int BitLength(Wide v)
{
    uint64_t word = v.high != 0 ? v.high : v.low;
    int length = v.high != 0 ? 64 : 0;
    while (word != 0)
    {
        length += 1;
        word >>= 1;
    }
    return length;
}

static Wide ShiftLeft(Wide v, int n)
{
    if (n == 0)
    {
        return v;
    }
    if (n >= 64)
    {
        return (Wide){v.low << (n - 64), 0};
    }
    return (Wide){(v.high << n) | (v.low >> (64 - n)), v.low << n};
}

static bool Bit(Wide v, int i)
{
    return ((i >= 64 ? v.high >> (i - 64) : v.low >> i) & 1) != 0;
}

static bool AtLeast(Wide a, Wide b)
{
    return a.high != b.high ? a.high > b.high : a.low >= b.low;
}

static Wide Difference(Wide a, Wide b)
{
    return (Wide){a.high - b.high - (a.low < b.low ? 1 : 0), a.low - b.low};
}

bool mlocWideBelow(Wide a, Wide b)
{
    return !AtLeast(a, b);
}

// The quotient and remainder of n / d, by shifting and subtracting; the
// quotient fits 64 bits here.
uint64_t mlocWideQuotient(Wide n, Wide d, Wide* remainderOut)
{
    uint64_t quotient = 0;
    Wide r = {0, 0};
    for (int i = BitLength(n) - 1; i >= 0; i -= 1)
    {
        r = ShiftLeft(r, 1);
        r.low |= Bit(n, i) ? 1 : 0;
        quotient <<= 1;
        if (AtLeast(r, d))
        {
            r = Difference(r, d);
            quotient |= 1;
        }
    }
    *remainderOut = r;
    return quotient;
}

double mlocBinary64Of(bool negative, Wide numerator, Wide denominator)
{
    uint64_t bits = 0;
    double out;
    if (numerator.high != 0 || numerator.low != 0)
    {
        // Scaled by 2^shift, the quotient has 54 or 55 bits; the scaling
        // moves to the denominator when it would shrink the numerator.
        int shift = BitLength(denominator) - BitLength(numerator) + 54;
        Wide n = shift >= 0 ? ShiftLeft(numerator, shift) : numerator;
        Wide d = shift >= 0 ? denominator : ShiftLeft(denominator, -shift);
        Wide remainder;
        uint64_t q = mlocWideQuotient(n, d, &remainder);
        int drop = q >> 54 != 0 ? 2 : 1;
        uint64_t mantissa = q >> drop;
        uint64_t rest = q & ((UINT64_C(1) << drop) - 1);
        uint64_t half = UINT64_C(1) << (drop - 1);
        bool sticky = remainder.high != 0 || remainder.low != 0;
        if (rest > half || (rest == half && (sticky || (mantissa & 1) != 0)))
        {
            mantissa += 1;
        }
        int exponent = 52 + drop - shift;
        if (mantissa >> 53 != 0)
        {
            mantissa >>= 1;
            exponent += 1;
        }
        bits = (negative ? UINT64_C(1) << 63 : 0) | ((uint64_t)(exponent + 1023) << 52) |
               (mantissa & ((UINT64_C(1) << 52) - 1));
    }
    memcpy(&out, &bits, sizeof out);
    return out;
}

bool mlocWideOfDouble(double value, bool* negativeOut, Wide* magnitudeOut)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof bits);
    int biased = (int)(bits >> 52 & 0x7FF);
    uint64_t fraction = bits & ((UINT64_C(1) << 52) - 1);
    *negativeOut = bits >> 63 != 0;
    if (biased == 0)
    {
        // Zero, or a subnormal, which is no integer.
        *magnitudeOut = (Wide){0, 0};
        return fraction == 0;
    }
    int exponent = biased - 1075;
    uint64_t mantissa = fraction | UINT64_C(1) << 52;
    if (biased == 0x7FF || exponent > 74)
    {
        return false;
    }
    if (exponent < 0)
    {
        if (exponent < -52 || (mantissa & ((UINT64_C(1) << -exponent) - 1)) != 0)
        {
            return false;
        }
        *magnitudeOut = (Wide){0, mantissa >> -exponent};
        return true;
    }
    *magnitudeOut = ShiftLeft((Wide){0, mantissa}, exponent);
    return true;
}
