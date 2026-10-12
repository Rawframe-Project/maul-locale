// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Exact unit conversion (unit_convert.h). A value in flight is
// ±a / b * 10^e with a and b fixed-width integers and e kept apart, so a
// decimal of any exponent scales without the integers growing; an
// offset needs the power folded in first, and fails only when the
// integers cannot hold it.

#include "unit_convert.h"

#include "bytes.h"
#include "decimal.h"
#include "unit_tables.h"

#include <string.h>

enum
{
    // The digits a conversion computes before rounding to odd.
    convertDigits = mlocDecimalDigits,
    // Past this power of ten a value is beyond every Beaufort force or
    // below the first.
    beaufortReach = 64,
};

// A value: ±a / b * 10^e, or an infinity.
typedef struct Exact
{
    Wide a;
    Wide b;
    int64_t e;
    bool negative;
    bool infinite;
} Exact;

static bool MultiplyPower(Wide* w, uint32_t base, uint32_t times)
{
    for (uint32_t i = 0; i < times; i++)
    {
        if (!mlocWideMultiplyAdd(w, base, 0))
        {
            return false;
        }
    }
    return true;
}

// Multiplies a size by a factor raised to a count: numerator by
// numerator, denominator by denominator.
static bool MultiplyBy(Wide* numerator, Wide* denominator, const Wide* n, const Wide* d,
                       uint32_t count)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (!mlocWideMultiply(numerator, numerator, n) ||
            !mlocWideMultiply(denominator, denominator, d))
        {
            return false;
        }
    }
    return true;
}

// Multiplies a size by a prefix's value raised to a count.
static bool MultiplyPrefix(Wide* numerator, Wide* denominator, const uint8_t* prefix,
                           uint32_t count)
{
    int8_t power10 = (int8_t)prefix[5];
    uint32_t times = count * (uint32_t)(power10 < 0 ? -power10 : power10);
    if (prefix[6] != 0)
    {
        return MultiplyPower(numerator, 1024, count * prefix[6]);
    }
    return power10 > 0 ? mlocWideScale10(numerator, times) : mlocWideScale10(denominator, times);
}

static bool MultiplyConstant(Wide* w, uint64_t constant)
{
    Wide c = mlocWideOf(constant);
    return mlocWideMultiply(w, w, &c);
}

// Whether a part converts with its simple unit's offset or table: alone,
// of power 1, without a prefix or constants.
static bool Alone(const UnitId* id)
{
    return id->count == 1 && id->parts[0].power == 1 && id->parts[0].prefix < 0 &&
           id->numeratorConstant == 1 && id->denominatorConstant == 1;
}

// Adds a single unit to a scale: its dimensions, its size and its
// prefix's raised to its power, and when it stands alone its offset or
// table.
static mlocResult ScalePart(const UnitTable* t, const UnitPart* part, bool alone, UnitScale* s)
{
    const uint8_t* simple = part->currency ? nullptr : mlocSimpleUnit(t, part->simple);
    if (simple == nullptr || simple[simpleBase] == unitNoBase ||
        ((simple[simpleFlags] & unitBeaufort) != 0 && !alone))
    {
        return mloc_errorInvalid;
    }
    const int8_t* dimensions = mlocBaseDimensions(t, simple[simpleBase]);
    for (uint32_t d = 0; d < unitMaxDimensions; d++)
    {
        s->dimensions[d] = (int8_t)(s->dimensions[d] + part->power * dimensions[d]);
    }
    UnitFactor f;
    mlocReadUnitFactor(t, part->simple, &f);
    bool denominator = part->power < 0;
    uint32_t count = (uint32_t)(denominator ? -part->power : part->power);
    Wide* top = denominator ? &s->denominator : &s->numerator;
    Wide* bottom = denominator ? &s->numerator : &s->denominator;
    if (!MultiplyBy(top, bottom, &f.numerator, &f.denominator, count) ||
        (part->prefix >= 0 &&
         !MultiplyPrefix(top, bottom, mlocUnitPrefix(t, (uint32_t)part->prefix), count)))
    {
        return mloc_errorCapacity;
    }
    if (alone)
    {
        s->offset = (simple[simpleFlags] & unitOffset) != 0;
        s->beaufort = (simple[simpleFlags] & unitBeaufort) != 0;
        s->offsetNumerator = f.offsetNumerator;
        s->offsetDenominator = f.offsetDenominator;
    }
    return mloc_success;
}

mlocResult mlocScaleUnit(const UnitTable* t, const UnitId* id, UnitScale* out)
{
    if (id->mixed)
    {
        return mloc_errorInvalid;
    }
    UnitScale s = {.numerator = mlocWideOf(1), .denominator = mlocWideOf(1)};
    for (uint32_t i = 0; i < id->count; i++)
    {
        mlocResult r = ScalePart(t, &id->parts[i], Alone(id), &s);
        if (r != mloc_success)
        {
            return r;
        }
    }
    if (!MultiplyConstant(&s.numerator, id->numeratorConstant) ||
        !MultiplyConstant(&s.denominator, id->denominatorConstant))
    {
        return mloc_errorCapacity;
    }
    *out = s;
    return mloc_success;
}

// The decimal's value: its digits as an integer, its last digit's power.
static bool ExactOf(const mlocDecimal* d, Exact* out)
{
    *out = (Exact){.b = mlocWideOf(1),
                   .e = d->exponent,
                   .negative = d->negative != 0,
                   .infinite = d->kind == mloc_decimalInfinity};
    for (uint32_t i = 0; i < d->count; i++)
    {
        if (!mlocWideMultiplyAdd(&out->a, 10, d->digits[i]))
        {
            return false;
        }
    }
    return true;
}

// Folds the power of ten into the integers.
static bool Fold(Exact* x)
{
    if (x->e > mlocDecimalMaxExponent || x->e < -mlocDecimalMaxExponent)
    {
        return false;
    }
    bool ok =
        x->e > 0 ? mlocWideScale10(&x->a, (uint32_t)x->e) : mlocWideScale10(&x->b, (uint32_t)-x->e);
    x->e = 0;
    return ok;
}

// x = x + sign * n / d, the power folded in first.
static bool AddFraction(Exact* x, const Wide* n, const Wide* d, bool negative)
{
    if (x->infinite || mlocWideIsZero(n))
    {
        return true;
    }
    Wide left;
    Wide right;
    if (!Fold(x) || !mlocWideMultiply(&left, &x->a, d) || !mlocWideMultiply(&right, n, &x->b) ||
        !mlocWideMultiply(&x->b, &x->b, d))
    {
        return false;
    }
    if (x->negative == negative)
    {
        return mlocWideAdd(&x->a, &left, &right);
    }
    bool rightLarger = mlocWideCompare(&right, &left) > 0;
    mlocWideSubtract(&x->a, rightLarger ? &right : &left, rightLarger ? &left : &right);
    x->negative = rightLarger ? negative : x->negative;
    return true;
}

// The Beaufort force's middle speed in its base unit: |x| rounded to a
// force, half up, within the table (as ICU's scaleToBase).
static bool FromBeaufort(const UnitTable* t, Exact* x)
{
    uint32_t last = t->beaufortCount - 2;
    uint32_t force = last;
    if (!x->infinite)
    {
        // force = floor(|x| + 1/2) = floor((2a + b) / 2b) * 10^e folded.
        Exact y = *x;
        Wide twice;
        Wide sum;
        Wide divisor;
        // Far past the table either way, the force is its last or 0.
        if (y.e > beaufortReach || y.e < -beaufortReach)
        {
            y = (Exact){.a = mlocWideOf(y.e > 0 ? last : 0), .b = mlocWideOf(1)};
        }
        if (!Fold(&y) || !mlocWideAdd(&twice, &y.a, &y.a) || !mlocWideAdd(&sum, &twice, &y.b) ||
            !mlocWideAdd(&divisor, &y.b, &y.b))
        {
            return false;
        }
        Wide quotient;
        mlocWideDivide(&sum, &divisor, &quotient, nullptr);
        Wide cap = mlocWideOf(last);
        force = mlocWideCompare(&quotient, &cap) > 0 ? last
                                                     : (quotient.count == 0 ? 0 : quotient.limb[0]);
    }
    uint32_t low = mlocLoad16(t->beaufort + 2 * force);
    uint32_t high = mlocLoad16(t->beaufort + 2 * (force + 1));
    *x = (Exact){.a = mlocWideOf(low + high), .b = mlocWideOf(20)};
    return true;
}

// The Beaufort force whose range holds |x| (as ICU's baseToScale).
static bool ToBeaufort(const UnitTable* t, Exact* x)
{
    uint32_t last = t->beaufortCount - 2;
    uint32_t force = 0;
    bool huge = x->infinite || x->e > beaufortReach;
    bool tiny = !huge && (mlocWideIsZero(&x->a) || x->e < -beaufortReach);
    if (huge)
    {
        force = last;
    }
    else if (!tiny)
    {
        Exact y = *x;
        if (!Fold(&y) || !mlocWideMultiplyAdd(&y.a, 10, 0))
        {
            return false;
        }
        // The tenths of a meter per second: force i while 10|x| >= its least.
        for (uint32_t i = 1; i <= last; i++)
        {
            Wide least = mlocWideOf(mlocLoad16(t->beaufort + 2 * i));
            Wide scaled;
            if (!mlocWideMultiply(&scaled, &least, &y.b))
            {
                return false;
            }
            force = mlocWideCompare(&y.a, &scaled) >= 0 ? i : force;
        }
    }
    *x = (Exact){.a = mlocWideOf(force), .b = mlocWideOf(1)};
    return true;
}

// Writes x as 40 digits rounded to odd.
static mlocResult ToDecimal(const Exact* x, mlocDecimal* out, bool* exactOut)
{
    mlocDecimal d = {.negative = x->negative};
    *exactOut = true;
    if (x->infinite)
    {
        d.kind = mloc_decimalInfinity;
        *out = d;
        return mloc_success;
    }
    if (mlocWideIsZero(&x->a))
    {
        *out = d;
        return mloc_success;
    }
    // a * 10^k / b has 41 or 42 digits.
    int64_t k = (int64_t)convertDigits + 1 -
                ((int64_t)mlocWideDigitCount(&x->a) - mlocWideDigitCount(&x->b));
    Wide a = x->a;
    Wide b = x->b;
    if (!(k >= 0 ? mlocWideScale10(&a, (uint32_t)k) : mlocWideScale10(&b, (uint32_t)-k)))
    {
        return mloc_errorCapacity;
    }
    Wide q;
    Wide r;
    mlocWideDivide(&a, &b, &q, &r);
    bool sticky = !mlocWideIsZero(&r);
    int64_t exponent = x->e - k;
    for (uint32_t n = mlocWideDigitCount(&q); n > convertDigits; n--, exponent++)
    {
        sticky |= mlocWideDivideSmall(&q, 10) != 0;
    }
    uint8_t digits[convertDigits];
    for (uint32_t i = convertDigits; i-- > 0;)
    {
        digits[i] = (uint8_t)mlocWideDivideSmall(&q, 10);
    }
    if (sticky && (digits[convertDigits - 1] == 0 || digits[convertDigits - 1] == 5))
    {
        digits[convertDigits - 1]++;
    }
    uint32_t count = convertDigits;
    while (digits[count - 1] == 0)
    {
        count--;
        exponent++;
    }
    if (exponent < -mlocDecimalMaxExponent || exponent + count - 1 > mlocDecimalMaxExponent)
    {
        return mloc_errorCapacity;
    }
    memcpy(d.digits, digits, count);
    d.count = (uint8_t)count;
    d.exponent = (int32_t)exponent;
    d.fractionDigits = exponent < 0 ? (int32_t)-exponent : 0;
    *exactOut = !sticky;
    *out = d;
    return mloc_success;
}

static bool Opposite(const UnitScale* a, const UnitScale* b)
{
    for (uint32_t d = 0; d < unitMaxDimensions; d++)
    {
        if (a->dimensions[d] != -b->dimensions[d])
        {
            return false;
        }
    }
    return true;
}

// x = x * n / d.
static bool Scale(Exact* x, const Wide* n, const Wide* d)
{
    return mlocWideMultiply(&x->a, &x->a, n) && mlocWideMultiply(&x->b, &x->b, d);
}

// x = 1 / x: zero gives an infinity and an infinity zero.
static void Invert(Exact* x)
{
    if (x->infinite || mlocWideIsZero(&x->a))
    {
        bool infinite = !x->infinite;
        *x = (Exact){.b = mlocWideOf(1), .negative = x->negative, .infinite = infinite};
        return;
    }
    Wide a = x->a;
    x->a = x->b;
    x->b = a;
    x->e = -x->e;
}

mlocResult mlocConvertScaled(const UnitTable* t, const UnitScale* from, const UnitScale* to,
                             const mlocDecimal* value, mlocDecimal* out, bool* exactOut)
{
    bool same = memcmp(from->dimensions, to->dimensions, sizeof(from->dimensions)) == 0;
    bool reciprocal = !same && Opposite(from, to);
    if ((!same && !reciprocal) || (reciprocal && (from->beaufort || to->beaufort)))
    {
        return mloc_errorInvalid;
    }
    if (value->kind == mloc_decimalNaN)
    {
        *out = *value;
        *exactOut = true;
        return mloc_success;
    }
    Exact x;
    if (!ExactOf(value, &x))
    {
        return mloc_errorCapacity;
    }
    // To the base unit.
    bool ok = from->beaufort ? FromBeaufort(t, &x)
                             : Scale(&x, &from->numerator, &from->denominator) &&
                                   (!from->offset || AddFraction(&x, &from->offsetNumerator,
                                                                 &from->offsetDenominator, false));
    if (ok && reciprocal)
    {
        Invert(&x);
    }
    // From the base unit to the target.
    if (ok && to->beaufort)
    {
        ok = ToBeaufort(t, &x);
    }
    else if (ok)
    {
        ok = (!to->offset || AddFraction(&x, &to->offsetNumerator, &to->offsetDenominator, true)) &&
             Scale(&x, &to->denominator, &to->numerator);
    }
    return ok ? ToDecimal(&x, out, exactOut) : mloc_errorCapacity;
}
