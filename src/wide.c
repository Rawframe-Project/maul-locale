// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Fixed-width unsigned integers (wide.h). Division is Knuth's algorithm D
// (The Art of Computer Programming, volume 2, 4.3.1): the divisor is
// shifted until its top limb's high bit is set, so each estimated
// quotient limb is at most two too large.

#include "wide.h"

#include <string.h>

static void Trim(Wide* a)
{
    while (a->count > 0 && a->limb[a->count - 1] == 0)
    {
        a->count--;
    }
}

Wide mlocWideOf(uint64_t value)
{
    Wide a = {0};
    a.limb[0] = (uint32_t)value;
    a.limb[1] = (uint32_t)(value >> 32);
    a.count = 2;
    Trim(&a);
    return a;
}

bool mlocWideIsZero(const Wide* a)
{
    return a->count == 0;
}

int mlocWideCompare(const Wide* a, const Wide* b)
{
    if (a->count != b->count)
    {
        return a->count < b->count ? -1 : 1;
    }
    for (uint32_t i = a->count; i-- > 0;)
    {
        if (a->limb[i] != b->limb[i])
        {
            return a->limb[i] < b->limb[i] ? -1 : 1;
        }
    }
    return 0;
}

bool mlocWideMultiply(Wide* out, const Wide* a, const Wide* b)
{
    if (a->count + b->count > wideLimbs + 1)
    {
        return false;
    }
    uint32_t product[2 * wideLimbs] = {0};
    for (uint32_t i = 0; i < a->count; i++)
    {
        uint64_t carry = 0;
        for (uint32_t j = 0; j < b->count; j++)
        {
            uint64_t t = (uint64_t)a->limb[i] * b->limb[j] + product[i + j] + carry;
            product[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
        product[i + b->count] = (uint32_t)carry;
    }
    uint32_t count = a->count + b->count;
    while (count > 0 && product[count - 1] == 0)
    {
        count--;
    }
    if (count > wideLimbs)
    {
        return false;
    }
    Wide r = {0};
    memcpy(r.limb, product, (size_t)count * sizeof(uint32_t));
    r.count = count;
    *out = r;
    return true;
}

bool mlocWideMultiplyAdd(Wide* a, uint32_t factor, uint32_t addend)
{
    uint64_t carry = addend;
    for (uint32_t i = 0; i < a->count; i++)
    {
        uint64_t t = (uint64_t)a->limb[i] * factor + carry;
        a->limb[i] = (uint32_t)t;
        carry = t >> 32;
    }
    if (carry != 0)
    {
        if (a->count == wideLimbs)
        {
            return false;
        }
        a->limb[a->count++] = (uint32_t)carry;
    }
    Trim(a);
    return true;
}

bool mlocWideAdd(Wide* out, const Wide* a, const Wide* b)
{
    uint32_t count = a->count > b->count ? a->count : b->count;
    Wide r = {0};
    uint64_t carry = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        uint64_t t = (uint64_t)a->limb[i] + b->limb[i] + carry;
        r.limb[i] = (uint32_t)t;
        carry = t >> 32;
    }
    r.count = count;
    if (carry != 0)
    {
        if (count == wideLimbs)
        {
            return false;
        }
        r.limb[r.count++] = 1;
    }
    *out = r;
    return true;
}

void mlocWideSubtract(Wide* out, const Wide* a, const Wide* b)
{
    Wide r = *a;
    int64_t borrow = 0;
    for (uint32_t i = 0; i < a->count; i++)
    {
        int64_t t = (int64_t)a->limb[i] - b->limb[i] - borrow;
        borrow = t < 0;
        r.limb[i] = (uint32_t)(t + (borrow << 32));
    }
    Trim(&r);
    *out = r;
}

bool mlocWideScale10(Wide* a, uint32_t power)
{
    for (; power >= 9; power -= 9)
    {
        if (!mlocWideMultiplyAdd(a, 1000000000u, 0))
        {
            return false;
        }
    }
    static const uint32_t s_powers[9] = {1,      10,      100,      1000,     10000,
                                         100000, 1000000, 10000000, 100000000};
    return mlocWideMultiplyAdd(a, s_powers[power], 0);
}

uint32_t mlocWideDivideSmall(Wide* a, uint32_t divisor)
{
    uint64_t remainder = 0;
    for (uint32_t i = a->count; i-- > 0;)
    {
        uint64_t t = remainder << 32 | a->limb[i];
        a->limb[i] = (uint32_t)(t / divisor);
        remainder = t % divisor;
    }
    Trim(a);
    return (uint32_t)remainder;
}

// The leading zero bits of a nonzero limb.
static uint32_t LeadingZeros(uint32_t x)
{
    uint32_t n = 0;
    for (; (x & 0x80000000u) == 0; x <<= 1)
    {
        n++;
    }
    return n;
}

// Shifts limbs left by bits (under 32) into out, giving count + 1 limbs.
static void ShiftLeft(const uint32_t* in, uint32_t count, uint32_t bits, uint32_t* out)
{
    out[count] = bits == 0 ? 0 : in[count - 1] >> (32 - bits);
    for (uint32_t i = count - 1; i > 0; i--)
    {
        out[i] = bits == 0 ? in[i] : in[i] << bits | in[i - 1] >> (32 - bits);
    }
    out[0] = in[0] << bits;
}

// Subtracts q * v (n limbs) from u (n + 1 limbs); adds v back when that
// went below zero, giving the corrected q.
static uint32_t SubtractMultiple(uint32_t* u, const uint32_t* v, uint32_t n, uint64_t q)
{
    int64_t borrow = 0;
    uint64_t carry = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint64_t p = q * v[i] + carry;
        carry = p >> 32;
        int64_t t = (int64_t)u[i] - (int64_t)(uint32_t)p - borrow;
        borrow = t < 0;
        u[i] = (uint32_t)(t + (borrow << 32));
    }
    int64_t t = (int64_t)u[n] - (int64_t)carry - borrow;
    u[n] = (uint32_t)t;
    if (t >= 0)
    {
        return (uint32_t)q;
    }
    uint64_t c = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint64_t s = (uint64_t)u[i] + v[i] + c;
        u[i] = (uint32_t)s;
        c = s >> 32;
    }
    u[n] += (uint32_t)c;
    return (uint32_t)(q - 1);
}

void mlocWideDivide(const Wide* numerator, const Wide* divisor, Wide* quotient, Wide* remainder)
{
    Wide q = {0};
    Wide r = *numerator;
    if (mlocWideCompare(numerator, divisor) < 0)
    {
        r = *numerator;
    }
    else if (divisor->count == 1)
    {
        q = *numerator;
        r = mlocWideOf(mlocWideDivideSmall(&q, divisor->limb[0]));
    }
    else
    {
        uint32_t n = divisor->count;
        uint32_t m = numerator->count - n;
        uint32_t shift = LeadingZeros(divisor->limb[n - 1]);
        uint32_t v[wideLimbs + 1];
        uint32_t u[wideLimbs + 2] = {0};
        ShiftLeft(divisor->limb, n, shift, v);
        ShiftLeft(numerator->limb, numerator->count, shift, u);
        for (uint32_t j = m + 1; j-- > 0;)
        {
            uint64_t top = (uint64_t)u[j + n] << 32 | u[j + n - 1];
            uint64_t qhat = top / v[n - 1];
            uint64_t rhat = top % v[n - 1];
            while (qhat > 0xFFFFFFFFu || qhat * v[n - 2] > (rhat << 32 | u[j + n - 2]))
            {
                qhat--;
                rhat += v[n - 1];
                if (rhat > 0xFFFFFFFFu)
                {
                    break;
                }
            }
            q.limb[j] = SubtractMultiple(u + j, v, n, qhat);
        }
        q.count = m + 1;
        Trim(&q);
        r = (Wide){0};
        for (uint32_t i = 0; i < n; i++)
        {
            r.limb[i] = shift == 0 ? u[i] : u[i] >> shift | u[i + 1] << (32 - shift);
        }
        r.count = n;
        Trim(&r);
    }
    if (quotient != nullptr)
    {
        *quotient = q;
    }
    if (remainder != nullptr)
    {
        *remainder = r;
    }
}

uint32_t mlocWideDigitCount(const Wide* a)
{
    Wide t = *a;
    uint32_t digits = 0;
    while (t.count > 1)
    {
        (void)mlocWideDivideSmall(&t, 1000000000u);
        digits += 9;
    }
    uint32_t low = t.count == 0 ? 0 : t.limb[0];
    do
    {
        digits++;
        low /= 10;
    } while (low != 0);
    return digits;
}
