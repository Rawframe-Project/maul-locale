// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Mixed units split exactly (unit_mixed.h).

#include "unit_mixed.h"

#include "decimal.h"
#include "wide.h"

UnitId mlocUnitPartOf(const UnitId* mixed, uint32_t i)
{
    UnitId part = {.numeratorConstant = 1, .denominatorConstant = 1, .count = 1};
    part.parts[0] = mixed->parts[i];
    return part;
}

bool mlocSameDimensions(const UnitScale* a, const UnitScale* b, bool* reciprocalOut)
{
    bool same = true;
    bool opposite = true;
    for (uint32_t d = 0; d < unitMaxDimensions; d++)
    {
        same &= a->dimensions[d] == b->dimensions[d];
        opposite &= a->dimensions[d] == -b->dimensions[d];
    }
    *reciprocalOut = !same && opposite;
    return same || opposite;
}

bool mlocUnitValueIn(const UnitTable* t, const UnitScale* from, const UnitValue* value,
                     const UnitScale* to, UnitValue* out)
{
    bool reciprocal = false;
    if (!mlocSameDimensions(from, to, &reciprocal) ||
        (reciprocal && (from->beaufort || to->beaufort)))
    {
        return false;
    }
    *out = *value;
    if (!mlocUnitValueToBase(t, from, out))
    {
        return false;
    }
    if (reciprocal)
    {
        mlocInvertUnitValue(out);
    }
    return mlocUnitValueFromBase(t, to, out);
}

// What a split works on: the parts' scales, largest first, and the index
// each has in the order given.
typedef struct Split
{
    const UnitTable* table;
    UnitScale scales[unitMaxParts];
    uint32_t index[unitMaxParts];
    uint32_t count;
} Split;

// Whether one part is larger than another (a / b > c / d).
static bool Larger(const UnitScale* a, const UnitScale* b)
{
    Wide left;
    Wide right;
    if (!mlocWideMultiply(&left, &a->numerator, &b->denominator) ||
        !mlocWideMultiply(&right, &b->numerator, &a->denominator))
    {
        return false;
    }
    return mlocWideCompare(&left, &right) > 0;
}

// The parts' scales, sorted largest first (stable).
static mlocResult Sort(const UnitTable* t, const UnitId* mixed, Split* s)
{
    s->table = t;
    s->count = mixed->count;
    if (mixed->count < 2 || mixed->count > unitMaxParts)
    {
        return mloc_errorInvalid;
    }
    for (uint32_t i = 0; i < s->count; i++)
    {
        UnitId part = mlocUnitPartOf(mixed, i);
        UnitScale scale;
        bool reciprocal = false;
        if (mlocScaleUnit(t, &part, &scale) != mloc_success || scale.offset || scale.beaufort ||
            (i > 0 && (!mlocSameDimensions(&scale, &s->scales[0], &reciprocal) || reciprocal)))
        {
            return mloc_errorInvalid;
        }
        for (uint32_t j = 0; j < i; j++)
        {
            // Two parts of one size ("foot-and-foot") are refused, as ICU.
            if (!Larger(&scale, &s->scales[j]) && !Larger(&s->scales[j], &scale))
            {
                return mloc_errorInvalid;
            }
        }
        uint32_t at = i;
        while (at > 0 && Larger(&scale, &s->scales[at - 1]))
        {
            s->scales[at] = s->scales[at - 1];
            s->index[at] = s->index[at - 1];
            at--;
        }
        s->scales[at] = scale;
        s->index[at] = i;
    }
    return mloc_success;
}

// Whether a rounded amount of one part reaches one of the part above.
static bool Reaches(const Split* s, const mlocDecimal* amount, uint32_t part)
{
    Wide one = mlocWideOf(1);
    UnitValue unit = mlocUnitValueOfInteger(&one);
    UnitValue size;
    UnitValue value;
    if (!mlocUnitValueIn(s->table, &s->scales[part - 1], &unit, &s->scales[part], &size) ||
        !mlocUnitValueOf(amount, &value))
    {
        return false;
    }
    return mlocUnitValueAtLeast(&value, &size.a, &size.b) && size.e == 0;
}

static mlocResult DecimalOf(const UnitValue* v, mlocDecimal* out, bool* exactOut)
{
    return mlocUnitValueDecimal(v, out, exactOut);
}

// A smallest part that rounds up to the size of the one above carries,
// and so on up.
static mlocResult Carry(const Split* s, Wide* integers, mlocDecimal* last)
{
    uint32_t smallest = s->count - 1;
    for (uint32_t i = smallest; i > 0 && last->kind == mloc_decimalFinite; i--)
    {
        bool carries = false;
        if (i == smallest)
        {
            carries = Reaches(s, last, i);
        }
        else
        {
            mlocDecimal whole;
            bool exact = true;
            UnitValue w = mlocUnitValueOfInteger(&integers[i]);
            carries = DecimalOf(&w, &whole, &exact) == mloc_success && Reaches(s, &whole, i);
        }
        if (!carries)
        {
            break;
        }
        if (i == smallest)
        {
            *last = (mlocDecimal){0};
        }
        else
        {
            integers[i] = mlocWideOf(0);
        }
        if (!mlocWideMultiplyAdd(&integers[i - 1], 1, 1))
        {
            return mloc_errorCapacity;
        }
    }
    return mloc_success;
}

// The whole parts above the smallest, and the rest in it.
static mlocResult Divide(const Split* s, UnitValue* v, Wide* integers)
{
    for (uint32_t i = 0; i + 1 < s->count && !v->infinite; i++)
    {
        UnitValue rest;
        if (!mlocSplitUnitValue(v, &integers[i]) ||
            !mlocUnitValueIn(s->table, &s->scales[i], v, &s->scales[i + 1], &rest))
        {
            return mloc_errorCapacity;
        }
        *v = rest;
    }
    return mloc_success;
}

mlocResult mlocCheckMixed(const UnitTable* t, const UnitId* mixed, uint32_t* smallestOut)
{
    Split s;
    mlocResult r = Sort(t, mixed, &s);
    *smallestOut = r == mloc_success ? s.index[s.count - 1] : 0;
    return r;
}

mlocResult mlocSplitMixed(const UnitTable* t, const UnitId* mixed, const UnitScale* input,
                          const UnitValue* value, bool nan, MixedRound round, void* context,
                          mlocUnitAmounts* out)
{
    Split s;
    Wide integers[unitMaxParts] = {0};
    mlocResult r = Sort(t, mixed, &s);
    if (r != mloc_success)
    {
        return r;
    }
    UnitValue v = *value;
    if (input != nullptr && !mlocUnitValueIn(t, input, value, &s.scales[0], &v))
    {
        return mloc_errorCapacity;
    }
    bool negative = v.negative;
    uint32_t smallest = s.count - 1;
    mlocDecimal last = {.kind = mloc_decimalNaN};
    if (!nan)
    {
        bool exact = true;
        r = Divide(&s, &v, integers);
        r = r == mloc_success ? DecimalOf(&v, &last, &exact) : r;
        r = r == mloc_success ? round(context, &last, exact) : r;
        r = r == mloc_success ? Carry(&s, integers, &last) : r;
    }
    if (r != mloc_success)
    {
        return r;
    }
    *out = (mlocUnitAmounts){.count = (uint8_t)s.count};
    for (uint32_t i = 0; i < smallest; i++)
    {
        bool exact = true;
        UnitValue w = mlocUnitValueOfInteger(&integers[i]);
        r = v.infinite || nan ? mlocMakeDecimal(0, &out->amounts[s.index[i]])
                              : DecimalOf(&w, &out->amounts[s.index[i]], &exact);
        if (r != mloc_success)
        {
            return r;
        }
    }
    out->amounts[s.index[smallest]] = last;
    if (v.infinite || nan)
    {
        // Not finite: the largest part takes it, with its sign; the others
        // are 0.
        out->amounts[s.index[0]] = last;
        (void)mlocMakeDecimal(0, &out->amounts[s.index[smallest]]);
        return mloc_success;
    }
    out->amounts[0].negative = negative;
    return mloc_success;
}

mlocResult mlocReachesThreshold(const UnitTable* t, const UnitScale* input, const UnitValue* value,
                                const UnitId* output, uint32_t threshold, uint8_t places,
                                bool* reachesOut)
{
    UnitId first = output->mixed ? mlocUnitPartOf(output, 0) : *output;
    UnitScale scale;
    bool reciprocal = false;
    if (mlocScaleUnit(t, &first, &scale) != mloc_success ||
        !mlocSameDimensions(input, &scale, &reciprocal))
    {
        return mloc_errorData;
    }
    UnitValue v;
    if (!mlocUnitValueIn(t, input, value, &scale, &v))
    {
        return mloc_errorCapacity;
    }
    Wide n = mlocWideOf(threshold);
    Wide d = mlocWideOf(1);
    (void)mlocWideScale10(&d, places);
    *reachesOut = mlocUnitValueAtLeast(&v, &n, &d);
    return mloc_success;
}
