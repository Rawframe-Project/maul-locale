// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_value.h"

#include "maul-unicode/normalize.h"

#include <string.h>

// Whether an argument's name is the variable's (NFC, from the compiled
// message): byte for byte when the argument's is NFC, as it usually is,
// else normalized on the stack first; a name longer than the stack's
// buffer that is not NFC is not found.
bool mlocSameName(const mlocMessageArgument* a, const uint8_t* name, size_t length)
{
    if (a->name == NULL)
    {
        return false;
    }
    if (a->nameLength == length && memcmp(a->name, name, length) == 0)
    {
        return true;
    }
    // An ASCII name is NFC already, so it is the variable's only when the
    // same bytes.
    bool ascii = true;
    for (size_t i = 0; i < a->nameLength && ascii; i++)
    {
        ascii = (uint8_t)a->name[i] < 0x80;
    }
    if (ascii)
    {
        return false;
    }
    muniQuickCheck answer = muni_quickCheckNo;
    if (muniCheckNormalization(a->name, a->nameLength, muni_nfc, &answer).status != muni_success ||
        answer == muni_quickCheckYes)
    {
        return false;
    }
    char normal[256];
    size_t needed = 0;
    muniTextResult result = muniNormalize(a->name, a->nameLength, muni_nfc, muni_convertStrict,
                                          normal, sizeof normal, &needed);
    return result.status == muni_success && needed == length && memcmp(normal, name, length) == 0;
}

// A numeric argument's decimal (a currency amount's amount); false for
// another kind or a number past a decimal's limits.
bool mlocArgumentDecimal(const mlocMessageValue* argument, mlocDecimal* out)
{
    switch (argument->kind)
    {
    case mloc_messageValueInteger:
        return mlocMakeDecimal(argument->as.integer, out) == mloc_success;
    case mloc_messageValueDouble:
        return mlocMakeShortestDecimal(argument->as.number, out) == mloc_success;
    case mloc_messageValueDecimal:
        *out = *argument->as.decimal;
        return true;
    case mloc_messageValueCurrency:
        *out = *argument->as.currency.amount;
        return true;
    case mloc_messageValueUnit:
        *out = *argument->as.unit.amount;
        return true;
    default:
        return false;
    }
}

bool mlocArgumentDigits(const mlocMessageValue* argument, char* digits, size_t capacity,
                        size_t* lengthOut)
{
    if (argument->kind >= mloc_messageValueDate && argument->kind <= mloc_messageValueDateTime)
    {
        // A date, time or date-time in ISO 8601.
        *lengthOut = mlocWriteIsoDateTime(argument, digits, capacity);
        return *lengthOut > 0;
    }
    mlocDecimal decimal;
    mlocOutput out = {digits, capacity, 0, NULL, NULL};
    if (!mlocArgumentDecimal(argument, &decimal) ||
        mlocFormatDecimal(&decimal, &out) != mloc_success)
    {
        return false;
    }
    if (argument->kind == mloc_messageValueCurrency)
    {
        // The amount and the code, as ICU4J writes a CurrencyAmount.
        if (out.length + 4 > capacity)
        {
            return false;
        }
        digits[out.length] = ' ';
        mlocUpperCode(argument->as.currency.code, digits + out.length + 1);
        out.length += 4;
    }
    *lengthOut = out.length;
    return true;
}

bool mlocStringMatches(const Value* value, const uint8_t* key, size_t length,
                       mlocMessageFaultKind* faultOut)
{
    *faultOut = mloc_messageFaultNone;
    const char* text = (const char*)value->text;
    size_t size = value->length;
    char digits[64];
    if (value->behind == valueArgument)
    {
        if (!mlocArgumentDigits(value->argument, digits, sizeof digits, &size))
        {
            return false;
        }
        text = digits;
        const mlocMessageValue* a = value->argument;
        if (a->kind == mloc_messageValueUnit)
        {
            // Digits, a space and the unit, all compared by their bytes.
            return length == size + 1 + a->as.unit.unitLength && memcmp(key, digits, size) == 0 &&
                   key[size] == ' ' &&
                   memcmp(key + size + 1, a->as.unit.unit, a->as.unit.unitLength) == 0;
        }
    }
    else if (value->behind != valueText)
    {
        // Over a failed operand: no key matches.
        return false;
    }
    if (size == length && (size == 0 || memcmp(text, key, length) == 0))
    {
        return true;
    }
    muniQuickCheck answer = muni_quickCheckNo;
    if (size == 0 || muniCheckNormalization(text, size, muni_nfc, &answer).status != muni_success ||
        answer == muni_quickCheckYes)
    {
        return false;
    }
    char normal[256];
    size_t needed = 0;
    muniTextResult result =
        muniNormalize(text, size, muni_nfc, muni_convertStrict, normal, sizeof normal, &needed);
    if (result.status != muni_success)
    {
        *faultOut = mloc_messageFaultUnsupportedOperation;
        return false;
    }
    return needed == length && memcmp(normal, key, length) == 0;
}
