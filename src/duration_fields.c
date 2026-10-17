// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration's fields as exact decimals (duration_fields.h). A valid
// duration's fields are below 2^84 (nanoseconds under 2^53 seconds); a
// field scaled by 10^9 with its fractions stays below 2^128.

#include "duration_fields.h"

#include "binary64.h"

#include <string.h>

enum
{
    // The digits of 10^19, the largest power of ten below 2^64, less one.
    chunkDigits = 19,
};

static Wide Magnitude(const mlocDurationValues* d, uint32_t field, bool* negative)
{
    const double given[mloc_durationFieldCount] = {
        d->years,   d->months,  d->weeks,        d->days,         d->hours,
        d->minutes, d->seconds, d->milliseconds, d->microseconds, d->nanoseconds};
    Wide magnitude = {0, 0};
    *negative = false;
    (void)mlocWideOfDouble(given[field], negative, &magnitude);
    return magnitude;
}

bool mlocDurationNegative(const mlocDurationValues* d)
{
    for (uint32_t i = 0; i < mloc_durationFieldCount; i++)
    {
        bool negative = false;
        Wide m = Magnitude(d, i, &negative);
        if (negative && (m.high != 0 || m.low != 0))
        {
            return true;
        }
    }
    return false;
}

// Writes a value's digits before end, least significant first, at least
// width of them (zeros before); the first digit written.
static uint8_t* Digits(uint64_t value, uint32_t width, uint8_t* end)
{
    uint8_t* at = end;
    for (uint32_t n = 0; value != 0 || n < width; n++)
    {
        *--at = (uint8_t)(value % 10);
        value /= 10;
    }
    return at;
}

// The decimal of magnitude / 10^places: the low 19 digits and, past
// 2^64, the rest by one division.
static void DecimalOf(Wide magnitude, uint32_t places, bool negative, mlocDecimal* out)
{
    static const Wide s_chunk = {0, UINT64_C(10000000000000000000)};
    uint8_t digits[2 * chunkDigits];
    uint8_t* end = digits + sizeof(digits);
    uint8_t* first = nullptr;
    if (magnitude.high == 0)
    {
        first = Digits(magnitude.low, 0, end);
    }
    else
    {
        Wide rest;
        uint64_t high = mlocWideQuotient(magnitude, s_chunk, &rest);
        first = Digits(high, 0, Digits(rest.low, chunkDigits, end));
    }
    int32_t exponent = -(int32_t)places;
    while (end > first && end[-1] == 0)
    {
        end--;
        exponent++;
    }
    memset(out, 0, sizeof(*out));
    out->negative = negative ? 1 : 0;
    if (first == end)
    {
        return;
    }
    out->count = (uint8_t)(end - first);
    memcpy(out->digits, first, out->count);
    out->exponent = exponent;
    out->fractionDigits = exponent < 0 ? -exponent : 0;
}

void mlocDurationValue(const mlocDurationValues* d, uint32_t field, uint32_t fractions,
                       bool negative, mlocDecimal* out)
{
    bool ignored = false;
    Wide total = Magnitude(d, field, &ignored);
    for (uint32_t i = 1; i <= fractions; i++)
    {
        total = mlocWideSum(mlocWideScaled(total, 1000), Magnitude(d, field + i, &ignored));
    }
    DecimalOf(total, 3 * fractions, negative, out);
}
