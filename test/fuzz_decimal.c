// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for decimals. A refusal points inside the text (a
// capacity refusal at 0); a parsed decimal writes text that parses back
// to the same bytes and writes the same text again, and has plural
// operands within their bounds.

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

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    const char* text = (const char*)data;
    mlocDecimal decimal;
    mlocTextResult result = mlocParseDecimal(text, size, &decimal);
    if (result.status != mloc_success)
    {
        Require(result.status == mloc_errorSyntax ? result.offset <= size : result.offset == 0);
        return 0;
    }
    // A decimal's text is at most a sign, 10^6 integer digits, a point and
    // 10^6 fraction digits; a short buffer reports what it needs.
    static char s_buffer[2 * mlocDecimalMaxExponent + 4];
    mlocOutput output = {.buffer = s_buffer, .capacity = sizeof(s_buffer)};
    Require(mlocFormatDecimal(&decimal, &output) == mloc_success);
    size_t length = output.length;
    mlocDecimal again;
    Require(mlocParseDecimal(s_buffer, length, &again).status == mloc_success);
    Require(memcmp(&decimal, &again, sizeof(decimal)) == 0);
    mlocOutput second = {.buffer = s_buffer, .capacity = sizeof(s_buffer)};
    Require(mlocFormatDecimal(&again, &second) == mloc_success && second.length == length);
    mlocPluralOperands operands;
    Require(mlocGetPluralOperands(&decimal, &operands) == mloc_success);
    const uint64_t limit = 2000000000000000000u;
    Require(operands.i < limit && operands.f < limit && operands.t < limit);
    Require(operands.w <= operands.v && (operands.t == 0) == (operands.w == 0));
    return 0;
}
