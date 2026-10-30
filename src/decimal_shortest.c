// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The shortest decimal that reads back as a double, by integer arithmetic
// only, so every platform gives the same digits: Ulf Adams's Ryu ("Ryu:
// fast float-to-string conversion", PLDI 2018). The interval of decimals
// that round to the double is scaled by a power of ten through 125-bit
// powers of five, and digits are removed while both ends of the interval
// still differ; of the shortest candidates the one nearest the double
// wins, ties to even.

#include "invariant.h"

#include "generated/pow5_tables.h"
#include "maul-locale/decimal.h"

#include <float.h>
#include <string.h>

#if FLT_RADIX != 2 || DBL_MANT_DIG != 53
#error "a double must be IEEE 754 binary64"
#endif
static_assert(sizeof(double) == sizeof(uint64_t), "a double has the bits of a uint64_t");

enum
{
    mantissaBits = 52,
    exponentBias = 1023,
};

// The high 64 bits of a 64-by-64-bit product, and its low ones.
static uint64_t MultiplyHigh(uint64_t a, uint64_t b, uint64_t* lowOut)
{
#if defined(__SIZEOF_INT128__)
    __extension__ unsigned __int128 product = (unsigned __int128)a * b;
    *lowOut = (uint64_t)product;
    return (uint64_t)(product >> 64);
#else
    uint64_t aLow = (uint32_t)a;
    uint64_t aHigh = a >> 32;
    uint64_t bLow = (uint32_t)b;
    uint64_t bHigh = b >> 32;
    uint64_t lowLow = aLow * bLow;
    uint64_t lowHigh = aLow * bHigh;
    uint64_t highLow = aHigh * bLow;
    uint64_t highHigh = aHigh * bHigh;
    uint64_t middle = (lowLow >> 32) + (uint32_t)lowHigh + (uint32_t)highLow;
    *lowOut = (middle << 32) | (uint32_t)lowLow;
    return highHigh + (lowHigh >> 32) + (highLow >> 32) + (middle >> 32);
#endif
}

// (m * multiplier) >> shift for a 125-bit multiplier, m below 2^55 and
// shift above 64.
static uint64_t MultiplyShift(uint64_t m, const uint64_t multiplier[2], int32_t shift)
{
    // Scale's m is four times a 53-bit mantissa, plus at most 2, and its
    // shifts are Ryu's, which leave the product's top 64 bits and more.
    MLOC_ASSERT(m >> 55 == 0 && shift > 64 && shift < 128);
    uint64_t low0 = 0;
    uint64_t high0 = MultiplyHigh(m, multiplier[0], &low0);
    uint64_t low1 = 0;
    uint64_t high1 = MultiplyHigh(m, multiplier[1], &low1);
    uint64_t sum = high0 + low1;
    high1 += sum < high0;
    int32_t distance = shift - 64;
    return (high1 << (64 - distance)) | (sum >> distance);
}

// ceil(log2(5^e)) for e >= 1, and 1 for e = 0.
static int32_t Pow5Bits(int32_t e)
{
    return (int32_t)(((uint32_t)e * 1217359) >> 19) + 1;
}

// floor(log10(2^e)) and floor(log10(5^e)) for e from 0 to 1650.
static int32_t Log10Pow2(int32_t e)
{
    return (int32_t)(((uint32_t)e * 78913) >> 18);
}

static int32_t Log10Pow5(int32_t e)
{
    return (int32_t)(((uint32_t)e * 732923) >> 20);
}

static bool MultipleOfPow5(uint64_t value, int32_t p)
{
    // Every multiple of 5 would include 0, and the loop would not end:
    // Scale asks about four times a nonzero mantissa, less 2 at most, or
    // plus 2.
    MLOC_ASSERT(value != 0);
    int32_t count = 0;
    for (; value % 5 == 0; value /= 5)
    {
        count++;
    }
    return count >= p;
}

static bool MultipleOfPow2(uint64_t value, int32_t p)
{
    // Scale asks only for q below 63.
    MLOC_ASSERT(p >= 0 && p < 64);
    return (value & ((1ull << p) - 1)) == 0;
}

// The interval of a double scaled by 10^-exponent: vm, vr and vp are the
// lower bound, the value and the upper bound, and the flags say whether
// what the scaling dropped of vm and vr was all zeros.
typedef struct Scaled
{
    uint64_t vr;
    uint64_t vp;
    uint64_t vm;
    int32_t exponent;
    bool vmTrailingZeros;
    bool vrTrailingZeros;
} Scaled;

static Scaled Scale(uint64_t m2, int32_t e2, bool acceptBounds, uint32_t mmShift)
{
    Scaled s = {0};
    uint64_t mv = 4 * m2;
    if (e2 >= 0)
    {
        int32_t q = Log10Pow2(e2) - (e2 > 3);
        // e2 is at most 2046 - 1023 - 52 - 2, so q is at most 290.
        MLOC_ASSERT(q >= 0 && q < pow5InverseCount);
        int32_t shift = -e2 + q + pow5Bits + Pow5Bits(q) - 1;
        s.exponent = q;
        s.vr = MultiplyShift(mv, s_pow5Inverse[q], shift);
        s.vp = MultiplyShift(mv + 2, s_pow5Inverse[q], shift);
        s.vm = MultiplyShift(mv - 1 - mmShift, s_pow5Inverse[q], shift);
        // Only for q of 21 or less can mv be a multiple of 5^q.
        if (q <= 21)
        {
            if (mv % 5 == 0)
            {
                s.vrTrailingZeros = MultipleOfPow5(mv, q);
            }
            else if (acceptBounds)
            {
                s.vmTrailingZeros = MultipleOfPow5(mv - 1 - mmShift, q);
            }
            else
            {
                s.vp -= MultipleOfPow5(mv + 2, q);
            }
        }
        return s;
    }
    int32_t q = Log10Pow5(-e2) - (-e2 > 1);
    int32_t i = -e2 - q;
    // -e2 is at most 1023 + 52 + 2 - 1, so i is at most 325.
    MLOC_ASSERT(q >= 0 && i >= 0 && i < pow5Count);
    int32_t shift = q - (Pow5Bits(i) - pow5Bits);
    s.exponent = q + e2;
    s.vr = MultiplyShift(mv, s_pow5[i], shift);
    s.vp = MultiplyShift(mv + 2, s_pow5[i], shift);
    s.vm = MultiplyShift(mv - 1 - mmShift, s_pow5[i], shift);
    if (q <= 1)
    {
        // mv has at least q trailing zero bits, so vr's dropped part is 0.
        s.vrTrailingZeros = true;
        if (acceptBounds)
        {
            s.vmTrailingZeros = mmShift == 1;
        }
        else
        {
            s.vp--;
        }
    }
    else if (q < 63)
    {
        s.vrTrailingZeros = MultipleOfPow2(mv, q);
    }
    return s;
}

// Shorten's common case, about 99% of doubles: nothing exact to watch
// for, so digits go two at a time while they can, and the last removed
// pair or digit alone decides the rounding (the value is never exactly
// halfway).
static uint64_t ShortenInexact(Scaled* s, int32_t* removedOut)
{
    int32_t removed = 0;
    bool roundUp = false;
    while (s->vp / 100 > s->vm / 100)
    {
        roundUp = s->vr % 100 >= 50;
        s->vr /= 100;
        s->vp /= 100;
        s->vm /= 100;
        removed += 2;
    }
    if (s->vp / 10 > s->vm / 10)
    {
        roundUp = s->vr % 10 >= 5;
        s->vr /= 10;
        s->vp /= 10;
        s->vm /= 10;
        removed++;
    }
    *removedOut = removed;
    return s->vr + (s->vr == s->vm || roundUp);
}

// Removes digits while the interval's ends still differ, and returns the
// digits left, rounded to the nearest, ties to even; *removedOut counts
// what went.
static uint64_t Shorten(Scaled* s, bool acceptBounds, int32_t* removedOut)
{
    if (!s->vmTrailingZeros && !s->vrTrailingZeros)
    {
        return ShortenInexact(s, removedOut);
    }
    int32_t removed = 0;
    uint64_t lastRemoved = 0;
    while (s->vp / 10 > s->vm / 10)
    {
        s->vmTrailingZeros &= s->vm % 10 == 0;
        s->vrTrailingZeros &= lastRemoved == 0;
        lastRemoved = s->vr % 10;
        s->vr /= 10;
        s->vp /= 10;
        s->vm /= 10;
        removed++;
    }
    if (s->vmTrailingZeros)
    {
        // The lower bound itself is a decimal of fewer digits.
        while (s->vm % 10 == 0)
        {
            s->vrTrailingZeros &= lastRemoved == 0;
            lastRemoved = s->vr % 10;
            s->vr /= 10;
            s->vp /= 10;
            s->vm /= 10;
            removed++;
        }
    }
    if (s->vrTrailingZeros && lastRemoved == 5 && s->vr % 2 == 0)
    {
        // Exactly halfway: round to even.
        lastRemoved = 4;
    }
    *removedOut = removed;
    bool outside = s->vr == s->vm && (!acceptBounds || !s->vmTrailingZeros);
    return s->vr + (outside || lastRemoved >= 5);
}

// Fills a decimal with the digits of an integer below 10^17 times
// 10^exponent, its trailing zeros moved into the exponent.
static void SetDigits(uint64_t digits, int32_t exponent, mlocDecimal* d)
{
    // A double's shortest digits are at most 17.
    MLOC_ASSERT(digits < UINT64_C(100000000000000000));
    while (digits != 0 && digits % 10 == 0)
    {
        digits /= 10;
        exponent++;
    }
    int n = 0;
    for (uint64_t rest = digits; rest != 0; rest /= 10)
    {
        n++;
    }
    for (int k = n - 1; k >= 0; k--)
    {
        d->digits[k] = (uint8_t)(digits % 10);
        digits /= 10;
    }
    d->count = (uint8_t)n;
    d->exponent = n == 0 ? 0 : exponent;
    d->fractionDigits = n > 0 && exponent < 0 ? -exponent : 0;
}

mlocResult mlocMakeShortestDecimal(double value, mlocDecimal* decimalOut)
{
    if (decimalOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    uint64_t mantissa = bits & ((1ull << mantissaBits) - 1);
    uint32_t biased = (uint32_t)(bits >> mantissaBits) & 0x7FF;
    mlocDecimal d = {.negative = (uint8_t)(bits >> 63)};
    if (biased == 0x7FF)
    {
        d.kind = mantissa == 0 ? mloc_decimalInfinity : mloc_decimalNaN;
        d.negative = mantissa == 0 ? d.negative : 0;
        *decimalOut = d;
        return mloc_success;
    }
    if (biased == 0 && mantissa == 0)
    {
        *decimalOut = d;
        return mloc_success;
    }
    // The double is m2 * 2^e2, with two more bits for the interval's ends.
    int32_t e2 = (biased == 0 ? 1 : (int32_t)biased) - exponentBias - mantissaBits - 2;
    uint64_t m2 = biased == 0 ? mantissa : (1ull << mantissaBits) | mantissa;
    bool even = (m2 & 1) == 0;
    // The lower end is nearer when the mantissa is a power of two.
    uint32_t mmShift = mantissa != 0 || biased <= 1;
    Scaled s = Scale(m2, e2, even, mmShift);
    int32_t removed = 0;
    uint64_t digits = Shorten(&s, even, &removed);
    SetDigits(digits, s.exponent + removed, &d);
    *decimalOut = d;
    return mloc_success;
}
