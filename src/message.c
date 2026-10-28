// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Messages (maul-locale/message.h): compiling, in two passes of
// message_parse through message_emit, then message_check; and writing
// back, through message_read and message_write.

#include "maul-locale/message.h"

#include "invariant.h"
#include "message_check.h"
#include "message_cost.h"
#include "message_emit.h"
#include "message_parse.h"
#include "message_read.h"
#include "message_write.h"

#include "maul-unicode/base.h"
#include "maul-unicode/encoding.h"

#include <string.h>

// The longest text compiled is MLOC_MAX_MESSAGE_TEXT bytes. Its compiled
// form's counters are 32 bits: the pool holds the text at most three times
// over (NFC expands a name or key at most threefold), so a text under 1 GiB
// keeps every count and offset exact.

static uint32_t UnicodeVersion(void)
{
    muniVersion version = muniGetUnicodeVersion();
    return (uint32_t)version.major << 16 | (uint32_t)version.minor << 8 | version.patch;
}

static mlocTextResult Refuse(mlocMessageError* errorOut, mlocMessageError error, size_t at)
{
    if (errorOut != NULL)
    {
        *errorOut = error;
    }
    mlocTextResult result = {mloc_errorSyntax, at};
    return result;
}

mlocTextResult mlocCompileMessage(const char* text, size_t length, void* buffer, size_t capacity,
                                  size_t* sizeOut, mlocMessageError* errorOut)
{
    mlocTextResult result = {mloc_errorInvalid, 0};
    if (errorOut != NULL)
    {
        *errorOut = mloc_messageErrorNone;
    }
    if ((text == NULL && length > 0) || (buffer == NULL && capacity > 0) || sizeOut == NULL)
    {
        return result;
    }
    if (length > MLOC_MAX_MESSAGE_TEXT)
    {
        result.status = mloc_errorLimit;
        return result;
    }
    muniTextResult valid = muniValidateUtf8(text, length);
    if (valid.status != muni_success)
    {
        return Refuse(errorOut, mloc_messageErrorSyntax, valid.offset);
    }
    const uint8_t* bytes = (const uint8_t*)text;
    MessageEmit emit;
    mlocEmitCount(&emit);
    MessageParse parse = mlocParseMessage(bytes, length, &emit);
    if (parse.error != mloc_messageErrorNone)
    {
        return Refuse(errorOut, parse.error, parse.errorAt);
    }
    MessageCounts counts;
    memcpy(counts.used, emit.used, sizeof counts.used);
    counts.keys = emit.keys;
    size_t needed = mlocEmitNeeded(&counts);
    if (needed == 0 || !mlocMessageCountsFit(counts.used))
    {
        result.status = mloc_errorLimit;
        return result;
    }
    if (capacity < needed)
    {
        *sizeOut = needed;
        result.status = mloc_errorCapacity;
        return result;
    }
    mlocEmitWrite(&emit, buffer, &counts);
    parse = mlocParseMessage(bytes, length, &emit);
    uint32_t size = mlocEmitFinish(&emit, parse.flags, UnicodeVersion());
    MessageCheck check = mlocCheckMessage(buffer, &emit.table, (uint8_t*)buffer + emit.wordStart);
    if (check.error != mloc_messageErrorNone)
    {
        return Refuse(errorOut, check.error, check.at);
    }
    // The chains and resolutions, in the check's words, which it no longer
    // needs.
    MessageView view;
    mlocResult read = mlocReadMessage(buffer, size, &view);
    MLOC_ASSERT(read == mloc_success);
    (void)read;
    if (mlocWriteMessageCosts(&view, (uint8_t*)buffer + emit.wordStart) != mloc_success)
    {
        result.status = mloc_errorLimit;
        return result;
    }
    *sizeOut = size;
    result.status = mloc_success;
    result.offset = length;
    return result;
}

mlocResult mlocWriteMessageText(const void* message, size_t size, mlocOutput* output)
{
    if (message == NULL || output == NULL || (output->buffer == NULL && output->capacity > 0))
    {
        return mloc_errorInvalid;
    }
    MessageView view;
    mlocResult result = mlocReadMessage(message, size, &view);
    if (result != mloc_success)
    {
        return result;
    }
    // Measured first: a variable's name is written at each reference, so
    // a crafted message of a few megabytes could ask for terabytes. A
    // text the compiler would refuse is not a message's text.
    mlocOutput measure = {NULL, 0, 0, NULL, NULL};
    (void)mlocWriteMessage(&view, &measure);
    if (measure.length > MLOC_MAX_MESSAGE_TEXT)
    {
        return mloc_errorFormat;
    }
    if (output->capacity < measure.length)
    {
        output->length = measure.length;
        return mloc_errorCapacity;
    }
    return mlocWriteMessage(&view, output);
}
