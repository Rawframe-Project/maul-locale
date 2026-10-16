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

// The decimal of magnitude / 10^places.
static void DecimalOf(Wide magnitude, uint32_t places, bool negative, mlocDecimal* out)
{
    static const Wide s_chunk = {0, UINT64_C(10000000000000000000)};
    uint8_t digits[2 * chunkDigits];
    Wide rest;
    uint64_t parts[2] = {mlocWideQuotient(magnitude, s_chunk, &rest), 0};
    parts[1] = rest.low;
    for (uint32_t p = 0; p < 2; p++)
    {
        for (uint32_t i = chunkDigits; i > 0; i--)
        {
            digits[p * chunkDigits + i - 1] = (uint8_t)(parts[p] % 10);
            parts[p] /= 10;
        }
    }
    uint32_t first = 0;
    uint32_t end = 2 * chunkDigits;
    while (first < end && digits[first] == 0)
    {
        first++;
    }
    while (end > first && digits[end - 1] == 0)
    {
        end--;
    }
    memset(out, 0, sizeof(*out));
    out->negative = negative ? 1 : 0;
    if (first == end)
    {
        return;
    }
    out->count = (uint8_t)(end - first);
    memcpy(out->digits, digits + first, out->count);
    out->exponent = (int32_t)(2 * chunkDigits - end) - (int32_t)places;
    out->fractionDigits = out->exponent < 0 ? -out->exponent : 0;
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
