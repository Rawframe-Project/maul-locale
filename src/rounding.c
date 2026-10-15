// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Temporal's rounding of integers (rounding.h).

#include "rounding.h"

RoundingUnsigned mlocUnsignedRoundingMode(mlocRoundingMode mode, bool negative)
{
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
    bool negative = value < 0;
    int64_t magnitude = negative ? -value : value;
    int64_t low = magnitude / increment;
    bool up = mlocRoundsUp(magnitude % increment, increment,
                           mlocUnsignedRoundingMode(mode, negative), low % 2 == 0);
    int64_t rounded = (low + (up ? 1 : 0)) * increment;
    return negative ? -rounded : rounded;
}
