// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for messages (record mloc-0045). The input's first
// byte chooses: even, the rest is MF2 text, which, when it compiles, is
// bound and formatted (two arguments, two functions), written back, compiles again, and writes back
// the same text (the canonical form is a fixed point) with the same counts; odd, the rest is taken
// as a compiled message (its magic and versions made this library's), which mlocWriteMessageText
// reads or refuses without reading past it, and whose text, when it reads it, writes the same twice
// and measures its own length.

#include "bytes.h"
#include "message_form.h"

#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

static uint8_t* Compile(const char* text, size_t length, size_t* sizeOut)
{
    size_t needed = 0;
    mlocMessageError error;
    mlocTextResult measured = mlocCompileMessage(text, length, NULL, 0, &needed, &error);
    if (measured.status != mloc_errorCapacity)
    {
        return NULL;
    }
    uint8_t* message = malloc(needed);
    mlocTextResult result = mlocCompileMessage(text, length, message, needed, sizeOut, &error);
    if (result.status != mloc_success)
    {
        free(message);
        return NULL;
    }
    Require(*sizeOut <= needed);
    return message;
}

// The message's text, measured first then written; null if refused.
static char* Write(const uint8_t* message, size_t size, size_t* lengthOut)
{
    mlocOutput measure = {NULL, 0, 0, NULL, NULL};
    mlocResult result = mlocWriteMessageText(message, size, &measure);
    if (result != mloc_errorCapacity && result != mloc_success)
    {
        return NULL;
    }
    char* text = malloc(measure.length + 1);
    mlocOutput output = {text, measure.length, 0, NULL, NULL};
    Require(mlocWriteMessageText(message, size, &output) == mloc_success &&
            output.length == measure.length);
    *lengthOut = output.length;
    return text;
}

// f:x: reads every option through the accessors, refuses a fallback
// operand, and resolves to its operand's text (up to 63 bytes), which it
// writes, matches a key equal to, and prefers shorter keys for.
static mlocMessageFaultKind FuzzResolve(void* context, const mlocMessageCall* call,
                                        const mlocMessageOperand* operand,
                                        mlocMessageResolved* valueOut)
{
    (void)context;
    if (operand->kind == mloc_messageOperandFallback)
    {
        return mloc_messageFaultBadOperand;
    }
    size_t count = mlocGetMessageOptionCount(call);
    for (size_t i = 0; i < count; i++)
    {
        const char* name;
        size_t length;
        mlocMessageOperand value;
        mlocMessageResolved storage;
        Require(mlocGetMessageOption(call, i, &name, &length, &value, &storage) == mloc_success);
        Require(mlocFindMessageOption(call, name, length, &value, &storage) == mloc_success);
    }
    mlocLocale locale;
    Require(mlocGetMessageLocale(call, &locale) == mloc_success);
    uint8_t value[64] = {0};
    if (operand->kind == mloc_messageOperandText)
    {
        value[0] = (uint8_t)(operand->length < 63 ? operand->length : 63);
        memcpy(value + 1, operand->text, value[0]);
    }
    memcpy(valueOut, value, sizeof value);
    return mloc_messageFaultNone;
}

static mlocMessageFaultKind FuzzFormat(void* context, const mlocMessageResolved* value,
                                       mlocOutput* output)
{
    (void)context;
    uint8_t bytes[64];
    memcpy(bytes, value, sizeof bytes);
    output->length = bytes[0];
    if (output->buffer != NULL)
    {
        memcpy(output->buffer, bytes + 1,
               bytes[0] < output->capacity ? bytes[0] : output->capacity);
    }
    return mloc_messageFaultNone;
}

static mlocMessageFaultKind FuzzMatch(void* context, const mlocMessageResolved* value,
                                      const char* key, size_t length, bool* matchOut)
{
    (void)context;
    uint8_t bytes[64];
    memcpy(bytes, value, sizeof bytes);
    *matchOut = length <= bytes[0] && memcmp(bytes + 1, key, length) == 0;
    return length == 0 ? mloc_messageFaultUnsupportedOperation : mloc_messageFaultNone;
}

static bool FuzzBetter(void* context, const mlocMessageResolved* value, const char* key1,
                       size_t length1, const char* key2, size_t length2)
{
    (void)context;
    (void)value;
    (void)key1;
    (void)key2;
    return length1 < length2;
}

// -12.5 as a decimal: digits 1, 2, 5, exponent -1, one fraction digit.
static const mlocDecimal s_amount = {.kind = mloc_decimalFinite,
                                     .negative = 1,
                                     .count = 3,
                                     .exponent = -1,
                                     .fractionDigits = 1,
                                     .digits = {1, 2, 5}};

static const mlocMessageArgument s_arguments[] = {
    {"s", 1, {mloc_messageValueString, 0, {.string = {"1", 1}}}},
    {"n", 1, {mloc_messageValueInteger, 0, {.integer = -7}}},
    {"c", 1, {mloc_messageValueCurrency, 0, {.currency = {&s_amount, {'c', 'h', 'f'}}}}},
    {"u", 1, {mloc_messageValueUnit, 0, {.unit = {&s_amount, "foot-and-inch", 13}}}},
};

static const mlocMessageFunction s_functions[] = {
    {"f:x", 3, NULL, FuzzResolve, FuzzFormat, FuzzMatch, FuzzBetter, NULL},
    {"f:y", 3, NULL, FuzzResolve, NULL, NULL, NULL, NULL},
};

// Binds a message and formats it with two arguments ($s a string, $n a
// number): the other variables unresolved, the functions other than
// :string, f:x and f:y unknown, the text measured then written.
static void Format(const uint8_t* message, size_t size)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    mlocLocale locale;
    mlocMessageFormatter formatter;
    mlocMessageOptions options = {false, mloc_messageIsolationDefault, s_functions, 2};
    Require(mlocParseLocale("en", 2, &locale).status == mloc_success &&
            mlocInitMessageFormatter(&formatter, &data, &locale, &options) == mloc_success);
    size_t needed = 0;
    if (mlocBindMessage(&formatter, message, size, NULL, 0, &needed) != mloc_errorCapacity)
    {
        return;
    }
    uint8_t* bound = malloc(needed);
    size_t boundSize;
    mlocResult bind = mlocBindMessage(&formatter, message, size, bound, needed, &boundSize);
    // A chain past the limit is found only when binding.
    Require(bind == mloc_success || bind == mloc_errorInvalid);
    if (bind != mloc_success)
    {
        free(bound);
        return;
    }
    mlocOutput measure = {NULL, 0, 0, NULL, NULL};
    mlocResult measured = mlocFormatMessage(bound, s_arguments, 4, &measure, NULL);
    char* text = malloc(measure.length + 1);
    mlocOutput output = {text, measure.length, 0, NULL, NULL};
    mlocResult result = mlocFormatMessage(bound, s_arguments, 4, &output, NULL);
    Require((measured == mloc_errorCapacity || measure.length == 0) &&
            (result == mloc_success || result == mloc_withFallbacks) &&
            output.length == measure.length);
    free(text);
    free(bound);
}

static void RoundTrip(const char* text, size_t length)
{
    size_t size;
    uint8_t* first = Compile(text, length, &size);
    if (first == NULL)
    {
        return;
    }
    Format(first, size);
    size_t written;
    char* canonical = Write(first, size, &written);
    Require(canonical != NULL);
    size_t again;
    uint8_t* second = Compile(canonical, written, &again);
    Require(second != NULL);
    // The same records, the source offsets aside.
    Require(memcmp(first + messageCounts, second + messageCounts, messageCount * 4) == 0);
    size_t rewritten;
    char* fixed = Write(second, again, &rewritten);
    Require(fixed != NULL && rewritten == written && memcmp(fixed, canonical, written) == 0);
    free(fixed);
    free(second);
    free(canonical);
    free(first);
}

static void ReadCompiled(const uint8_t* bytes, size_t size)
{
    // A copy of exactly the size, so that a read past it is caught.
    uint8_t* message = malloc(size > 0 ? size : 1);
    memcpy(message, bytes, size);
    if (size >= messageHeaderSize)
    {
        // This library's magic and versions, so that seeds compiled by
        // another version still reach the records.
        uint8_t current[128];
        size_t currentSize;
        mlocMessageError error;
        mlocTextResult result =
            mlocCompileMessage("", 0, current, sizeof current, &currentSize, &error);
        Require(result.status == mloc_success);
        memcpy(message, current, 4);
        memcpy(message + messageLibrary, current + messageLibrary, 8);
    }
    size_t length;
    char* text = Write(message, size, &length);
    if (text != NULL)
    {
        size_t again;
        char* twice = Write(message, size, &again);
        Require(twice != NULL && again == length && memcmp(text, twice, length) == 0);
        free(twice);
    }
    free(text);
    free(message);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size == 0)
    {
        return 0;
    }
    if ((data[0] & 1) == 0)
    {
        RoundTrip((const char*)data + 1, size - 1);
    }
    else
    {
        ReadCompiled(data + 1, size - 1);
    }
    return 0;
}
