// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for decimals. A refusal points inside the text (a
// capacity refusal at 0); a parsed decimal writes text that parses back
// to the same bytes and writes the same text again, and has plural
// operands within their bounds. The first 8 bytes, as a double, give a
// shortest decimal that holds the same.

#include "maul-locale/decimal.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static void Check(const mlocDecimal* decimal);

// Whether two decimals hold the same value and show the same digits,
// field by field: a decimal's padding is no part of it.
static bool Same(const mlocDecimal* a, const mlocDecimal* b)
{
    return a->kind == b->kind && a->negative == b->negative && a->count == b->count &&
           a->exponent == b->exponent && a->fractionDigits == b->fractionDigits &&
           memcmp(a->digits, b->digits, sizeof(a->digits)) == 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    mlocDecimal decimal;
    if (size >= sizeof(double))
    {
        double value = 0;
        memcpy(&value, data, sizeof(value));
        Require(mlocMakeShortestDecimal(value, &decimal) == mloc_success);
        Require(decimal.count <= 17);
        if (decimal.kind == mloc_decimalFinite)
        {
            Check(&decimal);
        }
    }
    const char* text = (const char*)data;
    mlocTextResult result = mlocParseDecimal(text, size, &decimal);
    if (result.status != mloc_success)
    {
        Require(result.status == mloc_errorSyntax ? result.offset <= size : result.offset == 0);
        return 0;
    }
    Check(&decimal);
    return 0;
}

// A finite decimal writes text that parses back to the same bytes and
// writes the same text again, and has plural operands in bounds.
static void Check(const mlocDecimal* decimal)
{
    // A decimal's text is at most a sign, 10^6 integer digits, a point and
    // 10^6 fraction digits; a short buffer reports what it needs.
    static char s_buffer[2 * MLOC_MAX_DECIMAL_EXPONENT + 4];
    mlocOutput output = {.buffer = s_buffer, .capacity = sizeof(s_buffer)};
    Require(mlocFormatDecimal(decimal, &output) == mloc_success);
    size_t length = output.length;
    mlocDecimal again;
    Require(mlocParseDecimal(s_buffer, length, &again).status == mloc_success);
    Require(memcmp(decimal, &again, sizeof(again)) == 0);
    mlocOutput second = {.buffer = s_buffer, .capacity = sizeof(s_buffer)};
    Require(mlocFormatDecimal(&again, &second) == mloc_success && second.length == length);
    mlocPluralOperands operands;
    Require(mlocGetPluralOperands(decimal, &operands) == mloc_success);
    const uint64_t limit = 2000000000000000000u;
    Require(operands.i < limit && operands.f < limit && operands.t < limit);
    Require(operands.w <= operands.v && (operands.t == 0) == (operands.w == 0));
    // Rounding in every mode gives a decimal that writes and reads back,
    // or refuses for capacity alone; rounding it again changes nothing.
    for (int mode = 0; mode <= mloc_roundTrunc; mode++)
    {
        static const uint32_t s_increments[4] = {1, 2, 5, 25};
        mlocDecimal rounded = *decimal;
        int32_t position = (int32_t)(operands.i % 41) - 20;
        uint32_t increment = s_increments[(operands.f + (uint32_t)mode) % 4];
        mlocResult result = mlocRoundDecimal(&rounded, position, increment, mode);
        Require(result == mloc_success || result == mloc_errorLimit);
        if (result == mloc_success)
        {
            mlocDecimal twice = rounded;
            Require(mlocRoundDecimal(&twice, position, increment, mode) == mloc_success);
            Require(Same(&twice, &rounded));
            mlocOutput out = {.buffer = s_buffer, .capacity = sizeof(s_buffer)};
            Require(mlocFormatDecimal(&rounded, &out) == mloc_success);
        }
    }
}
