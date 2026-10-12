// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decimal values (record mloc-0019): made from integers, parsed and
// written as text, and the plural operands of their digits. A digit's
// power of ten is the exponent plus its distance from the last digit.

#include "decimal.h"

#include "bytes.h"

#include <string.h>

// 10^0 to 10^18.
static const uint64_t s_pow10[19] = {1u,
                                     10u,
                                     100u,
                                     1000u,
                                     10000u,
                                     100000u,
                                     1000000u,
                                     10000000u,
                                     100000000u,
                                     1000000000u,
                                     10000000000u,
                                     100000000000u,
                                     1000000000000u,
                                     10000000000000u,
                                     100000000000000u,
                                     1000000000000000u,
                                     10000000000000000u,
                                     100000000000000000u,
                                     1000000000000000000u};

static_assert(mlocDecimalDigits % 8 == 0, "the digits are checked eight at a time");

// Whether each digit held is at most 9 and each unused byte 0, eight
// bytes at a time: a byte below 0x80 plus 0x7F less its limit has its top
// bit set only past the limit, and no such sum carries into the next byte.
static bool ValidDigits(const mlocDecimal* d)
{
    const uint64_t nines = 0x0909090909090909u;
    uint64_t bad = 0;
    for (int k = 0; k < mlocDecimalDigits; k += 8)
    {
        uint64_t word = mlocLoad64(d->digits + k);
        int used = d->count - k;
        uint64_t limits = used >= 8 ? nines : used <= 0 ? 0 : nines >> (64 - 8 * used);
        bad |= word | (word + 0x7F7F7F7F7F7F7F7Fu - limits);
    }
    return (bad & 0x8080808080808080u) == 0;
}

bool mlocValidDecimal(const mlocDecimal* d)
{
    if (d->kind > mloc_decimalNaN || d->negative > 1 || d->reserved != 0 ||
        d->count > mlocDecimalDigits || !ValidDigits(d))
    {
        return false;
    }
    if (d->kind != mloc_decimalFinite)
    {
        return d->count == 0 && d->exponent == 0 && d->fractionDigits == 0 &&
               (d->kind == mloc_decimalInfinity || d->negative == 0);
    }
    if (d->count == 0)
    {
        return d->exponent == 0 && d->fractionDigits >= 0 &&
               d->fractionDigits <= mlocDecimalMaxExponent;
    }
    int64_t needed = d->exponent < 0 ? -(int64_t)d->exponent : 0;
    return d->digits[0] != 0 && d->digits[d->count - 1] != 0 &&
           d->exponent >= -mlocDecimalMaxExponent && mlocTopPower(d) <= mlocDecimalMaxExponent &&
           d->fractionDigits >= needed && d->fractionDigits <= mlocDecimalMaxExponent;
}

mlocResult mlocMakeUnsignedDecimal(uint64_t value, mlocDecimal* decimalOut)
{
    if (decimalOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    mlocDecimal d = {0};
    while (value != 0 && value % 10 == 0)
    {
        value /= 10;
        d.exponent++;
    }
    // The digits from the last, then moved to the front.
    uint8_t buffer[20];
    int n = (int)sizeof(buffer);
    for (; value != 0; value /= 10)
    {
        buffer[--n] = (uint8_t)(value % 10);
    }
    d.count = (uint8_t)(sizeof(buffer) - (size_t)n);
    memcpy(d.digits, buffer + n, d.count);
    *decimalOut = d;
    return mloc_success;
}

mlocResult mlocMakeDecimal(int64_t value, mlocDecimal* decimalOut)
{
    // The magnitude in unsigned arithmetic, which INT64_MIN needs.
    uint64_t magnitude = value < 0 ? 0u - (uint64_t)value : (uint64_t)value;
    mlocResult result = mlocMakeUnsignedDecimal(magnitude, decimalOut);
    if (result == mloc_success)
    {
        decimalOut->negative = value < 0;
    }
    return result;
}

static bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static mlocTextResult Result(mlocResult status, size_t offset)
{
    return (mlocTextResult){status, offset};
}

// The mantissa as the text writes it: its significant digits, where the
// first and last nonzero ones stand among all its digits, and the digits
// before and after the point.
typedef struct Mantissa
{
    uint8_t digits[mlocDecimalDigits];
    int count;
    bool tooMany;
    int64_t first;
    int64_t last;
    int64_t integerDigits;
    int64_t fractionDigits;
} Mantissa;

// Reads digits with at most one point from *at; false when there is no
// digit.
static bool ReadMantissa(const char* text, size_t length, size_t* at, Mantissa* m)
{
    *m = (Mantissa){.first = -1, .last = -1};
    bool point = false;
    int64_t index = 0;
    int64_t zeros = 0;
    for (; *at < length; (*at)++)
    {
        char c = text[*at];
        if (c == '.' && !point)
        {
            point = true;
            continue;
        }
        if (!IsDigit(c))
        {
            break;
        }
        if (c != '0')
        {
            // The zeros since the last nonzero digit are significant now.
            if (m->first < 0)
            {
                m->first = index;
                zeros = 0;
            }
            if (m->count + zeros + 1 > mlocDecimalDigits)
            {
                m->tooMany = true;
            }
            else
            {
                for (int64_t z = 0; z < zeros; z++)
                {
                    m->digits[m->count++] = 0;
                }
                m->digits[m->count++] = (uint8_t)(c - '0');
            }
            m->last = index;
            zeros = 0;
        }
        else
        {
            zeros++;
        }
        if (point)
        {
            m->fractionDigits++;
        }
        else
        {
            m->integerDigits++;
        }
        index++;
    }
    return index > 0;
}

// Reads an exponent after its "e" from *at; its magnitude stops growing
// past any bound a decimal has, so it never overflows.
static bool ReadExponent(const char* text, size_t length, size_t* at, int64_t* exponentOut)
{
    bool negative = false;
    if (*at < length && (text[*at] == '+' || text[*at] == '-'))
    {
        negative = text[*at] == '-';
        (*at)++;
    }
    size_t start = *at;
    int64_t value = 0;
    for (; *at < length && IsDigit(text[*at]); (*at)++)
    {
        if (value <= 100000000)
        {
            value = value * 10 + (text[*at] - '0');
        }
    }
    *exponentOut = negative ? -value : value;
    return *at > start;
}

mlocTextResult mlocParseDecimal(const char* text, size_t length, mlocDecimal* decimalOut)
{
    if ((text == nullptr && length > 0) || decimalOut == nullptr)
    {
        return Result(mloc_errorInvalid, 0);
    }
    size_t at = 0;
    bool negative = at < length && text[at] == '-';
    if (at < length && (text[at] == '-' || text[at] == '+'))
    {
        at++;
    }
    Mantissa m;
    size_t mantissaStart = at;
    if (!ReadMantissa(text, length, &at, &m))
    {
        // A point alone is no number: the fault is past it.
        return Result(mloc_errorSyntax, at > mantissaStart ? at : mantissaStart);
    }
    int64_t exponent = 0;
    if (at < length && (text[at] == 'e' || text[at] == 'E'))
    {
        at++;
        if (!ReadExponent(text, length, &at, &exponent))
        {
            return Result(mloc_errorSyntax, at);
        }
    }
    if (at < length)
    {
        return Result(mloc_errorSyntax, at);
    }
    if (m.tooMany)
    {
        return Result(mloc_errorCapacity, 0);
    }
    int64_t shown = m.fractionDigits - exponent;
    mlocDecimal d = {.negative = negative};
    d.fractionDigits = (int32_t)(shown > 0 ? (shown > INT32_MAX ? INT32_MAX : shown) : 0);
    if (m.count > 0)
    {
        int64_t top = m.integerDigits - 1 - m.first + exponent;
        int64_t bottom = m.integerDigits - 1 - m.last + exponent;
        if (top > mlocDecimalMaxExponent || bottom < -mlocDecimalMaxExponent)
        {
            return Result(mloc_errorCapacity, 0);
        }
        memcpy(d.digits, m.digits, (size_t)m.count);
        d.count = (uint8_t)m.count;
        d.exponent = (int32_t)bottom;
    }
    if (shown > mlocDecimalMaxExponent)
    {
        return Result(mloc_errorCapacity, 0);
    }
    *decimalOut = d;
    return Result(mloc_success, length);
}

mlocResult mlocFormatDecimal(const mlocDecimal* decimal, mlocOutput* output)
{
    if (decimal == nullptr || output == nullptr ||
        (output->buffer == nullptr && output->capacity > 0) || !mlocValidDecimal(decimal))
    {
        return mloc_errorInvalid;
    }
    static const char s_infinity[8] = {'I', 'n', 'f', 'i', 'n', 'i', 't', 'y'};
    static const char s_nan[3] = {'N', 'a', 'N'};
    const char* special = decimal->kind == mloc_decimalInfinity ? s_infinity
                          : decimal->kind == mloc_decimalNaN    ? s_nan
                                                                : nullptr;
    int64_t specialLength = decimal->kind == mloc_decimalInfinity ? 8 : 3;
    int64_t top = mlocTopPower(decimal);
    int64_t integerDigits = special != nullptr ? specialLength : top >= 0 ? top + 1 : 1;
    int64_t fraction = decimal->fractionDigits;
    size_t length = (size_t)(decimal->negative + integerDigits + (fraction > 0 ? fraction + 1 : 0));
    output->length = length;
    // Every decimal writes at least one byte, so no buffer is too short.
    if (length > output->capacity || output->buffer == nullptr)
    {
        return mloc_errorCapacity;
    }
    char* out = output->buffer;
    size_t n = 0;
    if (decimal->negative)
    {
        out[n++] = '-';
    }
    if (special != nullptr)
    {
        memcpy(out + n, special, (size_t)specialLength);
        return mloc_success;
    }
    for (int64_t power = integerDigits - 1; power >= 0; power--)
    {
        out[n++] = (char)('0' + mlocDigitAt(decimal, power));
    }
    if (fraction > 0)
    {
        out[n++] = '.';
        for (int64_t power = -1; power >= -fraction; power--)
        {
            out[n++] = (char)('0' + mlocDigitAt(decimal, power));
        }
    }
    return mloc_success;
}

// The integer the digits from power high down to power low form: 10^18
// plus its low 18 digits when it has more.
static uint64_t Window(const mlocDecimal* d, int64_t high, int64_t low)
{
    uint64_t value = 0;
    bool wide = false;
    for (int k = 0; k < d->count; k++)
    {
        int64_t power = mlocTopPower(d) - k;
        if (power > high || power < low || d->digits[k] == 0)
        {
            continue;
        }
        if (power - low >= 18)
        {
            wide = true;
        }
        else
        {
            value += d->digits[k] * s_pow10[power - low];
        }
    }
    return wide ? s_pow10[18] + value : value;
}

void mlocFillPluralOperands(const mlocDecimal* d, mlocPluralOperands* operandsOut)
{
    uint32_t v = (uint32_t)d->fractionDigits;
    uint32_t w = d->count > 0 && d->exponent < 0 ? (uint32_t)-d->exponent : 0;
    int64_t top = mlocTopPower(d);
    if (top >= 18 || v > 18)
    {
        *operandsOut = (mlocPluralOperands){
            .i = Window(d, INT64_MAX, 0),
            .f = v > 0 ? Window(d, -1, -(int64_t)v) : 0,
            .t = w > 0 ? Window(d, -1, -(int64_t)w) : 0,
            .v = v,
            .w = w,
        };
        return;
    }
    // Each operand fits in its 18 digits: fold the integer digits, then
    // the fraction digits, which end at the last nonzero one.
    int integer = top < 0 ? 0 : d->count < top + 1 ? d->count : (int)top + 1;
    uint64_t i = 0;
    uint64_t t = 0;
    for (int k = 0; k < integer; k++)
    {
        i = i * 10 + d->digits[k];
    }
    for (int k = integer; k < d->count; k++)
    {
        t = t * 10 + d->digits[k];
    }
    *operandsOut = (mlocPluralOperands){
        .i = d->exponent > 0 ? i * s_pow10[d->exponent] : i,
        .f = t * s_pow10[v - w],
        .t = t,
        .v = v,
        .w = w,
    };
}

mlocResult mlocGetPluralOperands(const mlocDecimal* decimal, mlocPluralOperands* operandsOut)
{
    if (decimal == nullptr || operandsOut == nullptr || !mlocValidDecimal(decimal) ||
        decimal->kind != mloc_decimalFinite)
    {
        return mloc_errorInvalid;
    }
    mlocFillPluralOperands(decimal, operandsOut);
    return mloc_success;
}
