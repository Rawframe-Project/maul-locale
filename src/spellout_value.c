// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The spell-out engine's exact arithmetic (spellout_value.h).

#include "spellout_value.h"

#include "decimal.h"
#include "number_precision.h"

#include <string.h>

void mlocSpelloutNormalize(mlocDecimal* d)
{
    if (d->kind != mloc_decimalFinite)
    {
        return;
    }
    if (d->count == 0)
    {
        d->exponent = 0;
        d->negative = 0;
    }
    d->fractionDigits = d->count > 0 && d->exponent < 0 ? -d->exponent : 0;
}

bool mlocSpelloutIsInteger(const mlocDecimal* d)
{
    return d->kind == mloc_decimalFinite && (d->count == 0 || d->exponent >= 0);
}

bool mlocSpelloutIsZero(const mlocDecimal* d)
{
    return d->kind == mloc_decimalFinite && d->count == 0;
}

// An integer decimal's magnitude as 64 bits; false past them.
bool mlocSpelloutMagnitude64(const mlocDecimal* d, uint64_t* out)
{
    uint64_t m = 0;
    if (d->count > 0 && mlocTopPower(d) > 19)
    {
        return false;
    }
    for (int64_t power = d->count > 0 ? mlocTopPower(d) : -1; power >= 0; power--)
    {
        uint64_t digit = mlocDigitAt(d, power);
        if (m > (UINT64_MAX - digit) / 10)
        {
            return false;
        }
        m = m * 10 + digit;
    }
    *out = m;
    return true;
}

// An integer decimal as int64_t (INT64_MIN aside, which ICU writes by
// its decimal format); false otherwise.
bool mlocSpelloutToInt64(const mlocDecimal* d, int64_t* out)
{
    uint64_t m = 0;
    if (!mlocSpelloutIsInteger(d) || !mlocSpelloutMagnitude64(d, &m) || m > (uint64_t)INT64_MAX)
    {
        return false;
    }
    *out = d->negative != 0 ? -(int64_t)m : (int64_t)m;
    return true;
}

mlocDecimal mlocSpelloutFromInt64(int64_t n)
{
    mlocDecimal d;
    (void)mlocMakeDecimal(n, &d);
    return d;
}

mlocDecimal mlocSpelloutFromUnsigned(uint64_t n)
{
    mlocDecimal d;
    (void)mlocMakeUnsignedDecimal(n, &d);
    return d;
}

// A decimal from digits (most significant first) and the power of the
// last; false past 40 significant digits or 10^999999.
static bool Build(mlocDecimal* out, const uint8_t* digits, int count, int64_t exponent,
                  bool negative)
{
    int start = 0;
    while (start < count && digits[start] == 0)
    {
        start++;
    }
    int end = count;
    while (end > start && digits[end - 1] == 0)
    {
        end--;
        exponent++;
    }
    *out = (mlocDecimal){.kind = mloc_decimalFinite};
    if (end == start)
    {
        return true;
    }
    if (end - start > mlocDecimalDigits || exponent < -mlocDecimalMaxExponent ||
        exponent + (end - start) - 1 > mlocDecimalMaxExponent)
    {
        return false;
    }
    memcpy(out->digits, digits + start, (size_t)(end - start));
    out->count = (uint8_t)(end - start);
    out->exponent = (int32_t)exponent;
    out->negative = negative ? 1 : 0;
    mlocSpelloutNormalize(out);
    return true;
}

// Rounds a finite decimal to an integer in a mode.
mlocDecimal mlocSpelloutRound(const mlocDecimal* x, mlocRoundingMode mode)
{
    mlocDecimal d = *x;
    if (!mlocSpelloutIsInteger(&d))
    {
        (void)mlocRoundDecimal(&d, 0, 1, mode);
    }
    mlocSpelloutNormalize(&d);
    return d;
}

mlocDecimal mlocSpelloutAbsolute(const mlocDecimal* x)
{
    mlocDecimal d = *x;
    d.negative = 0;
    return d;
}

bool mlocSpelloutShift(const mlocDecimal* x, int power, mlocDecimal* out)
{
    if (mlocSpelloutIsZero(x) || x->kind != mloc_decimalFinite)
    {
        *out = *x;
        return true;
    }
    int64_t exponent = (int64_t)x->exponent - power;
    if (exponent < -mlocDecimalMaxExponent || exponent + x->count - 1 > mlocDecimalMaxExponent)
    {
        return false;
    }
    *out = mlocShiftDecimal(x, power);
    mlocSpelloutNormalize(out);
    return true;
}

// x - floor(x): the digits below the point, or for a negative value one
// less their value (ICU's number - uprv_floor(number)).
bool mlocSpelloutFraction(const mlocDecimal* x, mlocDecimal* out)
{
    if (mlocSpelloutIsInteger(x))
    {
        *out = (mlocDecimal){.kind = mloc_decimalFinite};
        return true;
    }
    if (x->negative == 0)
    {
        // The digits below the point, its zeros before them implied.
        uint8_t digits[mlocDecimalDigits];
        int count = 0;
        for (int i = 0; i < x->count; i++)
        {
            if (x->exponent + (int64_t)(x->count - 1 - i) < 0)
            {
                digits[count++] = x->digits[i];
            }
        }
        return Build(out, digits, count, x->exponent, false);
    }
    // 1 - f: 10^places - D, each place's nines complement and the last's
    // tens complement (a decimal's last digit is not 0, so nothing
    // carries), which fails past the places a decimal holds.
    int64_t places = -(int64_t)x->exponent;
    if (places > mlocDecimalDigits)
    {
        return false;
    }
    uint8_t digits[mlocDecimalDigits];
    for (int64_t i = 0; i < places; i++)
    {
        digits[i] = (uint8_t)(9 - mlocDigitAt(x, -1 - i));
    }
    digits[places - 1]++;
    return Build(out, digits, (int)places, -places, false);
}

// x mod 10^k with x's sign (ICU's uprv_fmod by a power of ten).
static mlocDecimal ModPower(const mlocDecimal* x, int power)
{
    uint8_t digits[mlocDecimalDigits];
    int count = 0;
    for (int i = 0; i < x->count; i++)
    {
        int64_t at = x->exponent + (int64_t)(x->count - 1 - i);
        if (at < power)
        {
            digits[count++] = x->digits[i];
        }
    }
    mlocDecimal d;
    (void)Build(&d, digits, count, x->exponent, x->negative != 0);
    return d;
}

// (a + b) mod m and a times b mod m for a and b below m, without
// overflow.
static uint64_t AddMod(uint64_t a, uint64_t b, uint64_t m)
{
    return a >= m - b ? a - (m - b) : a + b;
}

static uint64_t MultiplyMod(uint64_t a, uint64_t b, uint64_t m)
{
    uint64_t r = 0;
    for (; b != 0; b >>= 1)
    {
        if ((b & 1) != 0)
        {
            r = AddMod(r, a, m);
        }
        a = AddMod(a, a, m);
    }
    return r;
}

// A decimal's integer magnitude mod a 64-bit divisor, for any exponent.
uint64_t mlocSpelloutMagnitudeMod(const mlocDecimal* d, uint64_t divisor)
{
    uint64_t r = 0;
    uint64_t ten = 10 % divisor;
    for (int i = 0; i < d->count; i++)
    {
        r = AddMod(MultiplyMod(r, ten, divisor), d->digits[i] % divisor, divisor);
    }
    for (int64_t e = d->exponent; e > 0; e >>= 1)
    {
        if ((e & 1) != 0)
        {
            r = MultiplyMod(r, ten, divisor);
        }
        ten = MultiplyMod(ten, ten, divisor);
    }
    return r;
}

// x times a factor below 10^18, exact; false past the decimal's digits.
bool mlocSpelloutMultiply(const mlocDecimal* x, uint64_t factor, mlocDecimal* out)
{
    uint8_t digits[mlocDecimalDigits + 20];
    int n = (int)sizeof(digits);
    uint64_t carry = 0;
    for (int i = x->count - 1; i >= 0 || carry != 0; i--)
    {
        if (n == 0)
        {
            return false;
        }
        uint64_t value = carry + (i >= 0 ? x->digits[i] * factor : 0);
        digits[--n] = (uint8_t)(value % 10);
        carry = value / 10;
    }
    return Build(out, digits + n, (int)sizeof(digits) - n, x->exponent, x->negative != 0);
}

// floor(x / divisor) for the multiplier, exact; false when a divisor not
// a power of ten meets an integer part past int64_t.
bool mlocSpelloutDivideFloor(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out)
{
    if (divisor.power >= 0)
    {
        mlocDecimal shifted;
        if (!mlocSpelloutShift(x, divisor.power, &shifted))
        {
            return false;
        }
        *out = mlocSpelloutRound(&shifted, mloc_roundFloor);
        return true;
    }
    mlocDecimal floor = mlocSpelloutRound(x, mloc_roundFloor);
    int64_t n = 0;
    if (!mlocSpelloutToInt64(&floor, &n) || divisor.value > (uint64_t)INT64_MAX)
    {
        return false;
    }
    int64_t d = (int64_t)divisor.value;
    int64_t q = n / d;
    q -= n % d != 0 && n < 0 ? 1 : 0;
    *out = mlocSpelloutFromInt64(q);
    return true;
}

// The plural operand of a value of at least one: x / divisor truncated.
bool mlocSpelloutDivideTrunc(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out)
{
    mlocDecimal magnitude = mlocSpelloutAbsolute(x);
    if (!mlocSpelloutDivideFloor(&magnitude, divisor, out))
    {
        return false;
    }
    out->negative = mlocSpelloutIsZero(out) ? 0 : x->negative;
    return true;
}

// x / divisor, exact: a divisor of only twos and fives makes a decimal
// (x times 10^k / divisor, then shifted); any other fails.
bool mlocSpelloutDivideExact(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out)
{
    if (divisor.power >= 0)
    {
        return mlocSpelloutShift(x, divisor.power, out);
    }
    uint64_t rest = divisor.value;
    int twos = 0;
    int fives = 0;
    for (; rest % 2 == 0; rest /= 2)
    {
        twos++;
    }
    for (; rest % 5 == 0; rest /= 5)
    {
        fives++;
    }
    int k = twos > fives ? twos : fives;
    uint64_t factor = 1;
    for (int i = 0; i < k - twos && k <= 25; i++)
    {
        factor *= 2;
    }
    for (int i = 0; i < k - fives && k <= 25; i++)
    {
        factor *= 5;
    }
    mlocDecimal product;
    if (rest != 1 || k > 25 || !mlocSpelloutMultiply(x, factor, &product))
    {
        return false;
    }
    return mlocSpelloutShift(&product, k, out);
}

// fmod(x, divisor) with x's sign.
bool mlocSpelloutModulo(const mlocDecimal* x, SpelloutDivisor divisor, mlocDecimal* out)
{
    if (divisor.power >= 0)
    {
        *out = ModPower(x, divisor.power);
        return true;
    }
    mlocDecimal magnitude = mlocSpelloutAbsolute(x);
    mlocDecimal integer = mlocSpelloutRound(&magnitude, mloc_roundFloor);
    mlocDecimal fraction;
    uint64_t m = 0;
    if (!mlocSpelloutMagnitude64(&integer, &m) || !mlocSpelloutFraction(&magnitude, &fraction))
    {
        return false;
    }
    // The remainder's integer digits, then the fraction's.
    mlocDecimal remainder = mlocSpelloutFromUnsigned(m % divisor.value);
    int64_t top = remainder.count > 0 ? mlocTopPower(&remainder) : 0;
    int64_t last = mlocSpelloutIsZero(&fraction) ? 0 : fraction.exponent;
    uint8_t digits[mlocDecimalDigits + 24];
    if (top - last + 1 > (int64_t)sizeof(digits))
    {
        return false;
    }
    int count = (int)(top - last + 1);
    for (int i = 0; i < count; i++)
    {
        int64_t power = top - i;
        digits[i] = power >= 0 ? mlocDigitAt(&remainder, power) : mlocDigitAt(&fraction, power);
    }
    return Build(out, digits, count, last, x->negative != 0);
}

static int SignOf(const mlocDecimal* d)
{
    return mlocSpelloutIsZero(d) ? 0 : d->negative != 0 ? -1 : 1;
}

// The order of two finite decimals.
int mlocSpelloutCompare(const mlocDecimal* a, const mlocDecimal* b)
{
    int sa = SignOf(a);
    int sb = SignOf(b);
    if (sa != sb || sa == 0)
    {
        return sa < sb ? -1 : sa > sb ? 1 : 0;
    }
    int64_t top = mlocTopPower(a);
    int64_t other = mlocTopPower(b);
    if (top != other)
    {
        return top > other ? sa : -sa;
    }
    int64_t low = a->exponent < b->exponent ? a->exponent : b->exponent;
    for (int64_t power = top; power >= low; power--)
    {
        int da = mlocDigitAt(a, power);
        int db = mlocDigitAt(b, power);
        if (da != db)
        {
            return da > db ? sa : -sa;
        }
    }
    return 0;
}

bool mlocSpelloutAdd(const mlocDecimal* a, const mlocDecimal* b, mlocDecimal* out)
{
    if (mlocSpelloutIsZero(b))
    {
        *out = *a;
        return true;
    }
    if (mlocSpelloutIsZero(a))
    {
        *out = *b;
        return true;
    }
    int64_t topA = mlocTopPower(a);
    int64_t topB = mlocTopPower(b);
    int64_t top = (topA > topB ? topA : topB) + 1;
    int64_t low = a->exponent < b->exponent ? a->exponent : b->exponent;
    // Digits past a decimal's 40 are a sum it cannot hold.
    uint8_t digits[2 * mlocDecimalDigits + 2];
    if (top - low + 1 > (int64_t)sizeof(digits))
    {
        return false;
    }
    int count = (int)(top - low + 1);
    // The larger magnitude first; unlike signs subtract the smaller.
    mlocDecimal magnitudeA = mlocSpelloutAbsolute(a);
    mlocDecimal magnitudeB = mlocSpelloutAbsolute(b);
    bool swap = mlocSpelloutCompare(&magnitudeA, &magnitudeB) < 0;
    const mlocDecimal* big = swap ? b : a;
    const mlocDecimal* small = swap ? a : b;
    int sign = a->negative == b->negative ? 1 : -1;
    int carry = 0;
    for (int i = count - 1; i >= 0; i--)
    {
        int64_t power = top - i;
        int value = mlocDigitAt(big, power) + sign * mlocDigitAt(small, power) + carry;
        carry = value < 0 ? -1 : value / 10;
        digits[i] = (uint8_t)(value - carry * 10);
    }
    return Build(out, digits, count, low, big->negative != 0);
}
