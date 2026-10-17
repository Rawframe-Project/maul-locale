// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Message formatters and binding (maul-locale/message.h, record
// mloc-0045).

#include "message_bind.h"

#include "bytes.h"

#include <assert.h>
#include <string.h>

enum
{
    formatterStamp = 0x4D,
};

// "MLBD", read as a little-endian word.
static const uint32_t boundMagic = 0x44424C4D;

static_assert(sizeof(MessageFormatter) <= sizeof(mlocMessageFormatter),
              "the message formatter fits its public value");

static void Store32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

mlocResult mlocInitMessageFormatter(mlocMessageFormatter* formatterOut, const mlocDataSet* data,
                                    const mlocLocale* locale, const mlocMessageOptions* options)
{
    if (formatterOut == NULL || data == NULL || locale == NULL)
    {
        return mloc_errorInvalid;
    }
    MessageFormatter f;
    memset(&f, 0, sizeof f);
    f.stamp = formatterStamp;
    f.strict = options != NULL && options->strict;
    f.data = data;
    f.locale = *locale;
    memset(formatterOut, 0, sizeof *formatterOut);
    memcpy(formatterOut, &f, sizeof f);
    return mloc_success;
}

bool mlocReadMessageFormatter(const mlocMessageFormatter* formatter, MessageFormatter* out)
{
    if (formatter == NULL)
    {
        return false;
    }
    memcpy(out, formatter, sizeof *out);
    return out->stamp == formatterStamp;
}

bool mlocReadBound(const void* bound, BoundHeader* headerOut, MessageFormatter* formatterOut)
{
    if (bound == NULL)
    {
        return false;
    }
    memcpy(headerOut, bound, sizeof *headerOut);
    return headerOut->magic == boundMagic &&
           mlocReadMessageFormatter(headerOut->formatter, formatterOut);
}

const uint8_t* mlocBoundWords(const void* bound)
{
    return (const uint8_t*)bound + sizeof(BoundHeader);
}

static uint32_t Field(const MessageView* v, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(v, kind, index, field);
}

// The chain an expression starts: one more than the longest chain of a
// declaration whose variable it names (as operand or option value).
// Declarations name only earlier ones, so their expressions' lengths are
// known; one naming a later expression (only in crafted bytes, which
// could loop) is refused with 0.
static uint32_t ChainLength(const MessageView* v, const uint8_t* words, uint32_t e)
{
    const uint8_t* expressionWords = words + (size_t)v->count[messageVariables] * 4;
    uint32_t longest = 0;
    uint32_t first = Field(v, messageExpressions, e, 5);
    uint32_t count = Field(v, messageExpressions, e, 6);
    for (uint32_t i = 0; i <= count; i++)
    {
        uint32_t variable;
        if (i == count)
        {
            if (Field(v, messageExpressions, e, 0) != operandVariable)
            {
                continue;
            }
            variable = Field(v, messageExpressions, e, 1);
        }
        else if (Field(v, messageParameters, first + i, 0) == parameterOptionVariable)
        {
            variable = Field(v, messageParameters, first + i, 3);
        }
        else
        {
            continue;
        }
        uint32_t d = mlocLoad32(words + (size_t)variable * 4);
        if (d == messageUnbound)
        {
            continue;
        }
        uint32_t named = Field(v, messageDeclarations, d, 2);
        if (named == e && Field(v, messageDeclarations, d, 0) == declarationInput && i == count)
        {
            // An input's own operand is the argument.
            continue;
        }
        if (named >= e)
        {
            return 0;
        }
        uint32_t length = mlocLoad32(expressionWords + (size_t)named * 4) >> 24;
        longest = length > longest ? length : longest;
    }
    return longest + 1;
}

// Writes the words: each variable's declaration, then each expression's
// function slot and chain length; mloc_errorCapacity past the chain
// limit, mloc_errorData for a declaration naming a later one.
static mlocResult WriteWords(const MessageView* v, uint8_t* words)
{
    for (uint32_t i = 0; i < v->count[messageVariables]; i++)
    {
        Store32(words + (size_t)i * 4, messageUnbound);
    }
    for (uint32_t d = 0; d < v->count[messageDeclarations]; d++)
    {
        Store32(words + (size_t)Field(v, messageDeclarations, d, 1) * 4, d);
    }
    uint8_t* expressionWords = words + (size_t)v->count[messageVariables] * 4;
    for (uint32_t e = 0; e < v->count[messageExpressions]; e++)
    {
        uint32_t length = ChainLength(v, words, e);
        if (length == 0)
        {
            return mloc_errorData;
        }
        if (length > messageChainLimit)
        {
            return mloc_errorCapacity;
        }
        uint32_t slot =
            Field(v, messageExpressions, e, 4) > 0 ? messageFunctionUnknown : messageFunctionNone;
        Store32(expressionWords + (size_t)e * 4, slot | length << 24);
    }
    return mloc_success;
}

mlocResult mlocBindMessage(const mlocMessageFormatter* formatter, const void* message, size_t size,
                           void* buffer, size_t capacity, size_t* sizeOut)
{
    MessageFormatter f;
    if (!mlocReadMessageFormatter(formatter, &f) || message == NULL || sizeOut == NULL ||
        (buffer == NULL && capacity > 0))
    {
        return mloc_errorInvalid;
    }
    BoundHeader h;
    memset(&h, 0, sizeof h);
    mlocResult result = mlocReadMessage(message, size, &h.view);
    if (result != mloc_success)
    {
        return result;
    }
    uint64_t needed = sizeof h + 4 * ((uint64_t)h.view.count[messageVariables] +
                                      h.view.count[messageExpressions]);
    if (buffer == NULL || capacity < needed)
    {
        *sizeOut = (size_t)needed;
        return mloc_errorCapacity;
    }
    uint8_t* words = (uint8_t*)buffer + sizeof h;
    result = WriteWords(&h.view, words);
    if (result != mloc_success)
    {
        return result;
    }
    h.magic = boundMagic;
    h.size = (uint32_t)needed;
    h.formatter = formatter;
    memcpy(buffer, &h, sizeof h);
    *sizeOut = (size_t)needed;
    return mloc_success;
}
