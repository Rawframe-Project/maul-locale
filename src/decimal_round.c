// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Rounding a decimal: to a multiple of an increment of 1, 2, 5 or 25 at a
// power of ten, in ECMA-402's nine modes. With K the digits at and above
// the position, as an integer, and r the part below it, the value is (K +
// r) units; K's last two digits decide its remainder by the increment,
// since each increment divides 100, and r is compared with half a unit
// from its first digit and whether any follow.

#include "decimal.h"
#include "invariant.h"

#include <string.h>

// How the part of a value below a unit compares with half of one.
typedef enum Part
{
    partNone,
    partBelow,
    partHalf,
    partAbove,
} Part;

static Part BelowUnit(const mlocDecimal* d, int64_t position)
{
    if (d->count == 0 || d->exponent >= position)
    {
        return partNone;
    }
    // The last digit is not 0, so digits past the first one below the
    // unit make the part more than that digit alone.
    uint8_t first = mlocDigitAt(d, position - 1);
    bool more = d->exponent < position - 1;
    if (first != 5)
    {
        return first < 5 ? partBelow : partAbove;
    }
    return more ? partAbove : partHalf;
}

// How (remainder + part) compares with half an increment, as 2 * remainder
// + 2 * part against the increment.
static Part BeyondMultiple(uint32_t remainder, Part part, uint32_t increment)
{
    uint32_t twice = 2 * remainder;
    switch (part)
    {
    case partNone:
        return twice < increment ? partBelow : twice == increment ? partHalf : partAbove;
    case partBelow:
        return twice < increment ? partBelow : partAbove;
    case partHalf:
        return twice + 1 < increment ? partBelow : twice + 1 == increment ? partHalf : partAbove;
    case partAbove:
        return twice + 1 >= increment ? partAbove : partBelow;
    }
    return partNone;
}

// Whether a value between two multiples goes to the upper one (farther
// from zero); odd says whether the lower one's quotient is odd.
static bool RoundsUp(mlocRoundingMode mode, Part beyond, bool negative, bool odd)
{
    switch (mode)
    {
    case mloc_roundCeil:
        return !negative;
    case mloc_roundFloor:
        return negative;
    case mloc_roundExpand:
        return true;
    case mloc_roundTrunc:
        return false;
    default:
        break;
    }
    if (beyond != partHalf)
    {
        return beyond == partAbove;
    }
    switch (mode)
    {
    case mloc_roundHalfExpand:
        return true;
    case mloc_roundHalfTrunc:
        return false;
    case mloc_roundHalfCeil:
        return !negative;
    case mloc_roundHalfFloor:
        return negative;
    default:
        return odd;
    }
}

// The fraction digits a rounded decimal shows.
static int32_t Shown(const mlocDecimal* before, int64_t position, const mlocDecimal* after)
{
    int64_t limit = position < 0 ? -position : 0;
    int64_t shown = before->fractionDigits < limit ? before->fractionDigits : limit;
    int64_t needed = after->count > 0 && after->exponent < 0 ? -(int64_t)after->exponent : 0;
    return (int32_t)(shown > needed ? shown : needed);
}

// Writes the digits of buffer[0, length), the last at position, into a
// decimal without leading or trailing zeros; false when they pass its
// capacity or its largest power.
static bool SetRounded(const uint8_t* buffer, int length, int64_t position, mlocDecimal* d)
{
    int first = 0;
    while (first < length && buffer[first] == 0)
    {
        first++;
    }
    int last = length - 1;
    while (last >= first && buffer[last] == 0)
    {
        last--;
    }
    memset(d->digits, 0, sizeof(d->digits));
    d->count = 0;
    d->exponent = 0;
    if (first > last)
    {
        return true;
    }
    int64_t top = position + (length - 1 - first);
    int count = last - first + 1;
    if (count > MLOC_MAX_DECIMAL_DIGITS || top > MLOC_MAX_DECIMAL_EXPONENT)
    {
        return false;
    }
    memcpy(d->digits, buffer + first, (size_t)count);
    d->count = (uint8_t)count;
    d->exponent = (int32_t)(position + (length - 1 - last));
    return true;
}

mlocResult mlocRoundDecimal(mlocDecimal* decimal, int32_t position, uint32_t increment,
                            mlocRoundingMode mode)
{
    increment = increment == 0 ? 1 : increment;
    if (decimal == nullptr || !mlocValidDecimal(decimal) ||
        (increment != 1 && increment != 2 && increment != 5 && increment != 25) ||
        (uint32_t)mode > mloc_roundTrunc)
    {
        return mloc_errorInvalid;
    }
    if (position < -MLOC_MAX_DECIMAL_EXPONENT || position > MLOC_MAX_DECIMAL_EXPONENT)
    {
        return mloc_errorLimit;
    }
    if (decimal->kind != mloc_decimalFinite)
    {
        return mloc_success;
    }
    uint32_t last2 =
        10u * mlocDigitAt(decimal, (int64_t)position + 1) + mlocDigitAt(decimal, position);
    uint32_t remainder = last2 % increment;
    Part part = BelowUnit(decimal, position);
    mlocDecimal rounded = *decimal;
    if (remainder != 0 || part != partNone)
    {
        // Inexact, so no digit lies two places above the unit or more and
        // the digits from the first to the unit, with one place for a
        // carry, are at most 42.
        bool odd = (last2 % (2 * increment)) / increment != 0;
        bool up =
            RoundsUp(mode, BeyondMultiple(remainder, part, increment), decimal->negative, odd);
        int64_t top = mlocTopPower(decimal);
        int64_t high = (top > (int64_t)position + 1 ? top : (int64_t)position + 1) + 1;
        // The digits at and above the unit, in their places from high
        // down; the places past the last digit are zeros.
        uint8_t buffer[MLOC_MAX_DECIMAL_DIGITS + 3] = {0};
        int length = (int)(high - position + 1);
        MLOC_ASSERT(high >= top && length >= 2 && length <= (int)sizeof(buffer));
        int64_t kept = top - position + 1 < decimal->count ? top - position + 1 : decimal->count;
        if (kept > 0)
        {
            memcpy(buffer + (high - top), decimal->digits, (size_t)kept);
        }
        uint32_t sum = last2 - remainder + (up ? increment : 0);
        buffer[length - 1] = (uint8_t)(sum % 10);
        buffer[length - 2] = (uint8_t)((sum / 10) % 10);
        for (int k = length - 3, carry = (int)(sum / 100); carry != 0 && k >= 0; k--)
        {
            int digit = buffer[k] + carry;
            buffer[k] = (uint8_t)(digit % 10);
            carry = digit / 10;
        }
        if (!SetRounded(buffer, length, position, &rounded))
        {
            return mloc_errorLimit;
        }
    }
    rounded.fractionDigits = Shown(decimal, position, &rounded);
    *decimal = rounded;
    return mloc_success;
}

mlocResult mlocShowFractionDigits(mlocDecimal* decimal, int32_t count)
{
    if (decimal == nullptr || !mlocValidDecimal(decimal) || count < 0)
    {
        return mloc_errorInvalid;
    }
    if (count > MLOC_MAX_DECIMAL_EXPONENT)
    {
        return mloc_errorLimit;
    }
    if (decimal->kind != mloc_decimalFinite)
    {
        return mloc_success;
    }
    int32_t needed = decimal->count > 0 && decimal->exponent < 0 ? -decimal->exponent : 0;
    decimal->fractionDigits = count > needed ? count : needed;
    return mloc_success;
}
