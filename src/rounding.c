// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Temporal's rounding of integers (rounding.h).

#include "rounding.h"

#include "invariant.h"

RoundingUnsigned mlocUnsignedRoundingMode(mlocRoundingMode mode, bool negative)
{
    // Every public call refuses a mode past mloc_roundTrunc.
    MLOC_ASSERT((uint32_t)mode <= mloc_roundTrunc);
    switch (mode)
    {
    case mloc_roundCeil:
        return negative ? roundingZero : roundingInfinity;
    case mloc_roundFloor:
        return negative ? roundingInfinity : roundingZero;
    case mloc_roundExpand:
        return roundingInfinity;
    case mloc_roundTrunc:
        return roundingZero;
    case mloc_roundHalfCeil:
        return negative ? roundingHalfZero : roundingHalfInfinity;
    case mloc_roundHalfFloor:
        return negative ? roundingHalfInfinity : roundingHalfZero;
    case mloc_roundHalfExpand:
        return roundingHalfInfinity;
    case mloc_roundHalfTrunc:
        return roundingHalfZero;
    default:
        return roundingHalfEven;
    }
}

bool mlocRoundsUp(int64_t part, int64_t whole, RoundingUnsigned mode, bool evenLow)
{
    // A part of a step or a day, which doubles without overflow: the
    // callers' steps divide a day or a second.
    MLOC_ASSERT(part >= 0 && part < whole && whole <= INT64_MAX / 2);
    if (part == 0)
    {
        return false;
    }
    if (mode == roundingZero || mode == roundingInfinity)
    {
        return mode == roundingInfinity;
    }
    int64_t twice = 2 * part;
    if (twice != whole)
    {
        return twice > whole;
    }
    return mode == roundingHalfInfinity || (mode == roundingHalfEven && !evenLow);
}

int64_t mlocRoundToIncrement(int64_t value, int64_t increment, mlocRoundingMode mode)
{
    // Its callers round within a day by a step of at most a day, so the
    // value negates and the rounded magnitude does not overflow.
    MLOC_ASSERT(increment > 0 && value > INT64_MIN);
    bool negative = value < 0;
    int64_t magnitude = negative ? -value : value;
    MLOC_ASSERT(magnitude <= INT64_MAX - increment);
    int64_t low = magnitude / increment;
    bool up = mlocRoundsUp(magnitude % increment, increment,
                           mlocUnsignedRoundingMode(mode, negative), low % 2 == 0);
    int64_t rounded = (low + (up ? 1 : 0)) * increment;
    return negative ? -rounded : rounded;
}
