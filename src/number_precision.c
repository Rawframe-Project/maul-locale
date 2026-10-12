// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A number formatter's rounding (number_precision.h).

#include "number_precision.h"

#include "decimal.h"

enum
{
    maxFractionDigits = 100,
    maxSignificantDigits = 21,
};

static bool UsesFraction(mlocPrecisionKind kind)
{
    return kind == mloc_precisionFraction || kind == mloc_precisionMorePrecision ||
           kind == mloc_precisionLessPrecision;
}

static bool UsesSignificant(mlocPrecisionKind kind)
{
    return kind == mloc_precisionSignificant || kind == mloc_precisionMorePrecision ||
           kind == mloc_precisionLessPrecision;
}

// An ECMA-402 increment as a mlocRoundDecimal increment (1, 2, 5 or 25)
// times a power of ten; false for another.
static bool SplitIncrement(uint32_t increment, uint32_t* unitOut, int32_t* powerOut)
{
    int32_t power = 0;
    while (increment >= 10 && increment % 10 == 0)
    {
        increment /= 10;
        power++;
    }
    *unitOut = increment;
    *powerOut = power;
    return increment == 1 || increment == 2 || increment == 5 || increment == 25;
}

bool mlocValidPrecision(const mlocPrecision* p)
{
    if ((uint32_t)p->kind > mloc_precisionLessPrecision || (uint32_t)p->mode > mloc_roundTrunc ||
        (uint32_t)p->trailingZeros > mloc_trailingZerosStripIfInteger)
    {
        return false;
    }
    bool fraction = UsesFraction(p->kind) ? p->minFractionDigits <= p->maxFractionDigits &&
                                                p->maxFractionDigits <= maxFractionDigits
                                          : p->minFractionDigits == 0 && p->maxFractionDigits == 0;
    bool significant = UsesSignificant(p->kind)
                           ? p->minSignificantDigits >= 1 &&
                                 p->minSignificantDigits <= p->maxSignificantDigits &&
                                 p->maxSignificantDigits <= maxSignificantDigits
                           : p->minSignificantDigits == 0 && p->maxSignificantDigits == 0;
    uint32_t unit = 1;
    int32_t power = 0;
    // ECMA-402 takes the increments of 1 to 5000 its list names, which
    // are those of 1, 2, 5 and 25 times a power of ten up to 5000.
    bool increment =
        p->increment <= 1 ||
        (p->increment <= 5000 && SplitIncrement(p->increment, &unit, &power) &&
         p->kind == mloc_precisionFraction && p->minFractionDigits == p->maxFractionDigits);
    return fraction && significant && increment;
}

// The fraction digits a decimal's digits need.
static int32_t Needed(const mlocDecimal* d)
{
    return d->count > 0 && d->exponent < 0 ? -d->exponent : 0;
}

// The power of the first digit, 0 for zero, as ECMA-402's e.
static int64_t Magnitude(const mlocDecimal* d)
{
    return d->count == 0 ? 0 : mlocTopPower(d);
}

// Rounds at a position, unless every digit lies at or above it, which
// rounding leaves alone. Positions stay within the decimal's limits: a
// fixed one lies from -100 to 3, and a significant one below -999999
// lies under every digit.
static mlocResult RoundAt(mlocDecimal* d, int64_t position, uint32_t unit, mlocRoundingMode mode)
{
    if (unit == 1 && (d->count == 0 || position <= d->exponent))
    {
        return mloc_success;
    }
    return mlocRoundDecimal(d, (int32_t)position, unit, mode);
}

// ECMA-402's ToRawFixed: rounds at the maximum fraction digits (in steps
// of the increment) and shows at least the minimum.
static mlocResult Fixed(const mlocPrecision* p, mlocDecimal* d)
{
    uint32_t unit = 1;
    int32_t power = 0;
    if (p->increment > 1)
    {
        (void)SplitIncrement(p->increment, &unit, &power);
    }
    mlocResult r = RoundAt(d, (int64_t)power - p->maxFractionDigits, unit, p->mode);
    if (r == mloc_success)
    {
        int32_t needed = Needed(d);
        d->fractionDigits = needed > p->minFractionDigits ? needed : p->minFractionDigits;
    }
    return r;
}

// ECMA-402's ToRawPrecision: rounds to the maximum significant digits and
// shows at least the minimum. Sets the rounding magnitude, that of the
// last digit kept, after rounding.
static mlocResult Precise(const mlocPrecision* p, mlocDecimal* d, int64_t* magnitudeOut)
{
    mlocResult r = RoundAt(d, Magnitude(d) - p->maxSignificantDigits + 1, 1, p->mode);
    if (r == mloc_success)
    {
        int64_t top = Magnitude(d);
        int64_t shown = (int64_t)p->minSignificantDigits - top - 1;
        int32_t needed = Needed(d);
        d->fractionDigits = shown > needed ? (int32_t)shown : needed;
        *magnitudeOut = top - p->maxSignificantDigits + 1;
    }
    return r;
}

mlocResult mlocApplyPrecision(const mlocPrecision* p, mlocDecimal* decimal)
{
    mlocDecimal d = *decimal;
    mlocResult r = mloc_success;
    int64_t magnitude = 0;
    switch (p->kind)
    {
    case mloc_precisionFraction:
        r = Fixed(p, &d);
        break;
    case mloc_precisionSignificant:
        r = Precise(p, &d, &magnitude);
        break;
    case mloc_precisionMorePrecision:
    case mloc_precisionLessPrecision:
    {
        mlocDecimal fixed = d;
        r = Precise(p, &d, &magnitude);
        if (r == mloc_success)
        {
            r = Fixed(p, &fixed);
        }
        // The fixed result is the more precise when the significant one
        // stops at a higher power.
        bool fixedMore = magnitude > -(int64_t)p->maxFractionDigits;
        if (r == mloc_success && fixedMore == (p->kind == mloc_precisionMorePrecision))
        {
            d = fixed;
        }
        break;
    }
    default:
        break;
    }
    if (r != mloc_success)
    {
        return r;
    }
    if (p->trailingZeros == mloc_trailingZerosStripIfInteger && Needed(&d) == 0)
    {
        d.fractionDigits = 0;
    }
    *decimal = d;
    return mloc_success;
}
