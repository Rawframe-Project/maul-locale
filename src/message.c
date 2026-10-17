// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Messages (maul-locale/message.h, record mloc-0045): compiling, in two
// passes of message_parse through message_emit, then message_check.

#include "maul-locale/message.h"

#include "message_check.h"
#include "message_emit.h"
#include "message_parse.h"

#include "maul-unicode/base.h"
#include "maul-unicode/encoding.h"

#include <string.h>

// The longest text compiled. Its compiled form's counters are 32 bits:
// the pool holds the text at most three times over (NFC expands a name
// or key at most threefold), so a text under 1 GiB keeps every count and
// offset, and the capacity measured, exact.
static const size_t longestMessage = (size_t)1 << 30;

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
    if ((text == NULL && length > 0) || (buffer == NULL && capacity > 0) || sizeOut == NULL ||
        length >= longestMessage)
    {
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
    if (needed == 0)
    {
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
    *sizeOut = size;
    result.status = mloc_success;
    result.offset = length;
    return result;
}
