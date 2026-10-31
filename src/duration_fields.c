// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration's fields as exact decimals (duration_fields.h). A valid
// duration's fields are below 2^84 (nanoseconds under 2^53 seconds); a
// field scaled by 10^9 with its fractions stays below 2^128.

#include "duration_fields.h"

#include "duration_values.h"
#include "invariant.h"

#include <string.h>

enum
{
    // The digits of 10^19, the largest power of ten below 2^64, less one.
    chunkDigits = 19,
};

static_assert(2 * chunkDigits <= MLOC_MAX_DECIMAL_DIGITS, "a decimal holds a field's digits");

bool mlocReadDurationFields(const mlocDurationValues* d, DurationFields* out)
{
    Wide magnitudes[mloc_durationFieldCount];
    if (!mlocReadValidDuration(d, magnitudes, &out->negative))
    {
        return false;
    }
    for (uint32_t i = 0; i < mloc_durationFieldCount; i++)
    {
        out->high[i] = magnitudes[i].high;
        out->low[i] = magnitudes[i].low;
    }
    return true;
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
        // A field, with the fractions after it, is at most the time's
        // total, below 2^53 seconds in nanoseconds: far within 10^38.
        MLOC_ASSERT(high < s_chunk.low);
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

void mlocDurationValue(const DurationFields* f, uint32_t field, uint32_t fractions,
                       mlocDecimal* out)
{
    // The fractions taken are the fields after this one.
    MLOC_ASSERT(field + fractions < mloc_durationFieldCount);
    Wide total = {f->high[field], f->low[field]};
    for (uint32_t i = 1; i <= fractions; i++)
    {
        Wide next = {f->high[field + i], f->low[field + i]};
        total = mlocWideSum(mlocWideScaled(total, 1000), next);
    }
    DecimalOf(total, 3 * fractions, f->negative, out);
}
