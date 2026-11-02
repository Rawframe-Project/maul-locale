// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_value.h"

#include "maul-unicode/encoding.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

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
    if (argument->kind == mloc_messageValueDuration)
    {
        // A duration in ISO 8601, as Temporal writes it.
        *lengthOut = mlocWriteIsoDuration(argument->as.duration, digits, capacity);
        return *lengthOut > 0;
    }
    if (argument->kind >= mloc_messageValueDate && argument->kind <= mloc_messageValueZonedDateTime)
    {
        // A date, time, date-time, instant or zoned date-time in ISO 8601.
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

enum
{
    // The bytes the NFC of a text, or of a piece of it compared in turn,
    // is written into, and the most a piece may have: NFC makes UTF-8 at
    // most three times as long (UAX #15).
    normalRoom = 512,
    pieceRoom = MLOC_MAX_MESSAGE_SELECTOR_RUN,
};

static_assert(3 * pieceRoom <= normalRoom, "a piece's NFC fits");

// Whether the code point at text is an NFC boundary, as ICU4C's
// hasCompBoundaryBefore: a starter that composes with nothing before it,
// so that nothing before it moves past it or composes with it, and a
// text's NFC is that of its pieces cut there. Its NFD's first code point
// is a starter whose quick check is not maybe (which names the second
// code point of a composition); a singleton such as U+2126 or a CJK
// compatibility ideograph is one. *sizeOut receives the code point's
// length, 0 for ill-formed text.
static bool NfcBoundary(const char* text, size_t length, size_t* sizeOut)
{
    uint32_t codePoint = 0;
    *sizeOut = 0;
    if (muniDecodeUtf8(text, length, &codePoint, sizeOut) != muni_success)
    {
        *sizeOut = 0;
        return false;
    }
    uint32_t decomposed[MUNI_MAX_DECOMPOSITION];
    size_t count = 0;
    uint32_t first = muniGetDecomposition(codePoint, muni_nfd, decomposed, MUNI_MAX_DECOMPOSITION,
                                          &count) == muni_success &&
                             count > 0
                         ? decomposed[0]
                         : codePoint;
    char bytes[4];
    size_t size = 0;
    muniQuickCheck answer = muni_quickCheckMaybe;
    return muniGetCombiningClass(codePoint) == 0 && muniGetCombiningClass(first) == 0 &&
           muniEncodeUtf8(first, bytes, sizeof bytes, &size) == muni_success &&
           muniCheckNormalization(bytes, size, muni_nfc, &answer).status == muni_success &&
           answer != muni_quickCheckMaybe;
}

// Where the piece of text from at ends: the last boundary within
// pieceRoom bytes, or the text's end within them; 0 when there is none.
static size_t PieceEnd(const char* text, size_t size, size_t at)
{
    if (size - at <= pieceRoom)
    {
        return size;
    }
    size_t end = 0;
    size_t step = 0;
    (void)NfcBoundary(text + at, size - at, &step);
    for (size_t i = at + step; step > 0 && i - at <= pieceRoom; i += step)
    {
        if (NfcBoundary(text + i, size - i, &step))
        {
            end = i;
        }
    }
    return end;
}

// Whether a text's NFC is a key, normalized a piece at a time; with a
// fault when a piece cannot be normalized or has no boundary within
// pieceRoom bytes, and *limitOut set for the latter.
static bool PiecesMatch(const char* text, size_t size, const uint8_t* key, size_t length,
                        mlocMessageFaultKind* faultOut, bool* limitOut)
{
    size_t matched = 0;
    for (size_t at = 0; at < size;)
    {
        size_t end = PieceEnd(text, size, at);
        char normal[normalRoom];
        size_t needed = 0;
        if (end == 0 || muniNormalize(text + at, end - at, muni_nfc, muni_convertStrict, normal,
                                      sizeof normal, &needed)
                                .status != muni_success)
        {
            *faultOut = mloc_messageFaultUnsupportedOperation;
            *limitOut = end == 0;
            return false;
        }
        if (needed > length - matched || memcmp(normal, key + matched, needed) != 0)
        {
            return false;
        }
        matched += needed;
        at = end;
    }
    return matched == length;
}

bool mlocStringMatches(const Value* value, const uint8_t* key, size_t length,
                       mlocMessageFaultKind* faultOut, bool* limitOut)
{
    *faultOut = mloc_messageFaultNone;
    *limitOut = false;
    const char* text = (const char*)value->text;
    size_t size = value->length;
    char digits[128];
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
    char normal[normalRoom];
    size_t needed = 0;
    muniTextResult result =
        muniNormalize(text, size, muni_nfc, muni_convertStrict, normal, sizeof normal, &needed);
    // The whole length is given when the text does not fit: a key of
    // another length cannot match, and one as long is compared in pieces.
    if (result.status == muni_errorCapacity)
    {
        return needed == length && PiecesMatch(text, size, key, length, faultOut, limitOut);
    }
    if (result.status != muni_success)
    {
        *faultOut = mloc_messageFaultUnsupportedOperation;
        return false;
    }
    return needed == length && memcmp(normal, key, length) == 0;
}
