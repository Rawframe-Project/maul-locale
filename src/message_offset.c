// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_offset.h"

#include "invariant.h"

#include "maul-locale/decimal.h"

#include <string.h>

enum
{
    // The digits an offset sum is worked in: a decimal's 40 and room for
    // a carry; a sum needing more is refused when read back.
    offsetDigits = 48,
};

// Adds b to a (little-endian digit arrays of offsetDigits, the top one
// of each zero, so nothing carries out).
static void AddDigits(uint8_t* a, const uint8_t* b)
{
    unsigned carry = 0;
    for (int i = 0; i < offsetDigits; i++)
    {
        unsigned sum = a[i] + b[i] + carry;
        a[i] = (uint8_t)(sum % 10);
        carry = sum / 10;
    }
}

// Subtracts b from a, a not less than b.
static void SubtractDigits(uint8_t* a, const uint8_t* b)
{
    int borrow = 0;
    for (int i = 0; i < offsetDigits; i++)
    {
        int difference = a[i] - b[i] - borrow;
        borrow = difference < 0;
        a[i] = (uint8_t)(difference + (borrow ? 10 : 0));
    }
}

static int CompareDigits(const uint8_t* a, const uint8_t* b)
{
    for (int i = offsetDigits - 1; i >= 0; i--)
    {
        if (a[i] != b[i])
        {
            return a[i] < b[i] ? -1 : 1;
        }
    }
    return 0;
}

// The value's digits into a and the delta's magnitude into b, each from
// the power low up. Each digit and the delta's two lie at most at top,
// below the room and its spare top digit.
static void Place(const mlocDecimal* d, int32_t delta, int64_t low, int64_t top, uint8_t* a,
                  uint8_t* b)
{
    for (int i = 0; i < d->count; i++)
    {
        int64_t at = d->exponent + d->count - 1 - i - low;
        MLOC_ASSERT(at >= 0 && at <= top - low);
        a[at] = d->digits[i];
    }
    uint32_t magnitude = (uint32_t)(delta < 0 ? -(int64_t)delta : delta);
    for (int64_t at = -low; magnitude > 0; at++, magnitude /= 10)
    {
        MLOC_ASSERT(at <= top - low);
        b[at] = (uint8_t)(magnitude % 10);
    }
}

bool mlocOffsetNumber(NumberValue* value, int32_t delta)
{
    mlocDecimal* d = &value->decimal;
    // The add or subtract option's digit size (mlocDigitSize).
    MLOC_ASSERT(delta >= -99 && delta <= 99);
    if (d->kind != mloc_decimalFinite || delta == 0)
    {
        return true;
    }
    // The value as an integer of digits from its last power (or the
    // units, if lower) up: a sum needs every power between them.
    int64_t low = d->exponent < 0 ? d->exponent : 0;
    // The delta, at most 99, reaches the tens.
    int64_t top = d->count > 0 ? (int64_t)d->exponent + d->count - 1 : 0;
    top = top > 1 ? top : 1;
    if (top - low + 2 > offsetDigits)
    {
        return false;
    }
    uint8_t a[offsetDigits] = {0};
    uint8_t b[offsetDigits] = {0};
    Place(d, delta, low, top, a, b);
    bool negative = d->negative != 0 && d->count > 0;
    bool subtract = negative != (delta < 0);
    if (!subtract)
    {
        AddDigits(a, b);
    }
    else if (CompareDigits(a, b) >= 0)
    {
        SubtractDigits(a, b);
    }
    else
    {
        SubtractDigits(b, a);
        memcpy(a, b, sizeof a);
        negative = !negative;
    }
    // Written plainly and read back, so the decimal stays the library's.
    char text[offsetDigits + 4];
    size_t length = 0;
    int first = offsetDigits - 1;
    while (first > -low && a[first] == 0)
    {
        first--;
    }
    if (negative)
    {
        text[length++] = '-';
    }
    for (int i = first; i >= 0; i--)
    {
        if (i == -low - 1)
        {
            text[length++] = '.';
        }
        text[length++] = (char)('0' + a[i]);
    }
    mlocDecimal sum;
    if (mlocParseDecimal(text, length, &sum).status != mloc_success)
    {
        return false;
    }
    // A zero sum keeps no sign; the fraction digits shown stay.
    sum.negative = sum.count == 0 ? 0 : sum.negative;
    if (mlocShowFractionDigits(&sum, d->fractionDigits) != mloc_success)
    {
        return false;
    }
    *d = sum;
    return true;
}
