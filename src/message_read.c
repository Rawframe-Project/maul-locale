// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_read.h"

#include "bytes.h"
#include "message_text.h"

#include "maul-unicode/base.h"
#include "maul-unicode/encoding.h"

#include <string.h>

uint32_t mlocMessageKey(const MessageView* view, uint32_t variant, uint32_t selector,
                        uint32_t* offsetOut, uint32_t* lengthOut)
{
    MLOC_ASSERT(variant < view->count[messageVariants] && selector < view->count[messageSelectors]);
    const uint8_t* key = view->bytes + view->keyStart +
                         (variant * view->count[messageSelectors] + selector) * messageKeySize;
    *offsetOut = mlocLoad32(key + 4);
    *lengthOut = mlocLoad32(key + 8);
    return mlocLoad32(key);
}

// A string in the pool, well-formed UTF-8; empty only where allowed.
static bool GoodString(const MessageView* v, uint32_t offset, uint32_t length, bool empty)
{
    uint32_t pool = v->count[messagePool];
    if (length == 0)
    {
        return empty && offset <= pool;
    }
    if (offset > pool || length > pool - offset)
    {
        return false;
    }
    return muniValidateUtf8((const char*)v->pool + offset, length).status == muni_success;
}

// A well-formed string that is one MF2 name, or with identifier set a
// namespace, ":" and a name.
static bool GoodName(const MessageView* v, uint32_t offset, uint32_t length, bool identifier)
{
    if (!GoodString(v, offset, length, false))
    {
        return false;
    }
    const uint8_t* text = v->pool + offset;
    size_t at = 0;
    bool start = true;
    bool colon = false;
    while (at < length)
    {
        size_t size;
        uint32_t c = mlocMessageDecode(text, at, length, &size);
        if (c == ':' && identifier && !colon && !start)
        {
            colon = true;
            start = true;
        }
        else if (start ? !mlocMessageIsNameStart(c) : !mlocMessageIsNameChar(c))
        {
            return false;
        }
        else
        {
            start = false;
        }
        at += size;
    }
    return !start;
}

static bool InRange(uint32_t first, uint32_t count, uint32_t total)
{
    return first <= total && count <= total - first;
}

static uint32_t Field(const MessageView* v, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(v, kind, index, field);
}

static bool GoodDeclarations(const MessageView* v)
{
    for (uint32_t i = 0; i < v->count[messageDeclarations]; i++)
    {
        if (Field(v, messageDeclarations, i, 0) > declarationLocal ||
            Field(v, messageDeclarations, i, 1) >= v->count[messageVariables] ||
            Field(v, messageDeclarations, i, 2) >= v->count[messageExpressions])
        {
            return false;
        }
    }
    return true;
}

static bool GoodOperand(const MessageView* v, uint32_t i)
{
    uint32_t a = Field(v, messageExpressions, i, 1);
    uint32_t b = Field(v, messageExpressions, i, 2);
    switch (Field(v, messageExpressions, i, 0))
    {
    case operandNone:
        return a == 0 && b == 0 && Field(v, messageExpressions, i, 4) > 0;
    case operandLiteral:
        return GoodString(v, a, b, true);
    case operandVariable:
        return a < v->count[messageVariables] && b == 0;
    default:
        return false;
    }
}

static bool GoodExpressions(const MessageView* v)
{
    for (uint32_t i = 0; i < v->count[messageExpressions]; i++)
    {
        uint32_t function = Field(v, messageExpressions, i, 4);
        if (!GoodOperand(v, i) ||
            (function > 0 && !GoodName(v, Field(v, messageExpressions, i, 3), function, true)) ||
            (function == 0 && Field(v, messageExpressions, i, 3) != 0) ||
            !InRange(Field(v, messageExpressions, i, 5), Field(v, messageExpressions, i, 6),
                     v->count[messageParameters]))
        {
            return false;
        }
    }
    return true;
}

static bool GoodMarkups(const MessageView* v)
{
    for (uint32_t i = 0; i < v->count[messageMarkups]; i++)
    {
        if (Field(v, messageMarkups, i, 0) > markupClose ||
            !GoodName(v, Field(v, messageMarkups, i, 1), Field(v, messageMarkups, i, 2), true) ||
            !InRange(Field(v, messageMarkups, i, 3), Field(v, messageMarkups, i, 4),
                     v->count[messageParameters]))
        {
            return false;
        }
    }
    return true;
}

static bool GoodParameter(const MessageView* v, uint32_t i)
{
    uint32_t a = Field(v, messageParameters, i, 3);
    uint32_t b = Field(v, messageParameters, i, 4);
    if (!GoodName(v, Field(v, messageParameters, i, 1), Field(v, messageParameters, i, 2), true))
    {
        return false;
    }
    switch (Field(v, messageParameters, i, 0))
    {
    case parameterOptionLiteral:
    case parameterAttributeLiteral:
        return GoodString(v, a, b, true);
    case parameterOptionVariable:
        return a < v->count[messageVariables] && b == 0;
    case parameterAttribute:
        return a == 0 && b == 0;
    default:
        return false;
    }
}

static bool GoodMatcher(const MessageView* v)
{
    for (uint32_t i = 0; i < v->count[messageSelectors]; i++)
    {
        if (Field(v, messageSelectors, i, 0) >= v->count[messageVariables])
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < v->count[messageVariants]; i++)
    {
        if (!InRange(Field(v, messageVariants, i, 0), Field(v, messageVariants, i, 1),
                     v->count[messagePieces]))
        {
            return false;
        }
        for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
        {
            uint32_t offset;
            uint32_t length;
            uint32_t kind = mlocMessageKey(v, i, s, &offset, &length);
            bool good = kind == keyCatchAll
                            ? offset == 0 && length == 0
                            : kind == keyLiteral && GoodString(v, offset, length, true);
            if (!good)
            {
                return false;
            }
        }
    }
    return true;
}

static bool GoodPieces(const MessageView* v)
{
    for (uint32_t i = 0; i < v->count[messagePieces]; i++)
    {
        uint32_t a = Field(v, messagePieces, i, 1);
        uint32_t b = Field(v, messagePieces, i, 2);
        uint32_t kind = Field(v, messagePieces, i, 0);
        bool good = (kind == pieceText && GoodString(v, a, b, false)) ||
                    (kind == pieceExpression && a < v->count[messageExpressions] && b == 0) ||
                    (kind == pieceMarkup && a < v->count[messageMarkups] && b == 0);
        if (!good)
        {
            return false;
        }
    }
    return true;
}

static bool GoodRecords(const MessageView* v)
{
    for (uint32_t i = 0; i < v->count[messageParameters]; i++)
    {
        if (!GoodParameter(v, i))
        {
            return false;
        }
    }
    for (uint32_t i = 0; i < v->count[messageVariables]; i++)
    {
        if (!GoodName(v, Field(v, messageVariables, i, 0), Field(v, messageVariables, i, 1),
                      false) ||
            Field(v, messageVariables, i, 2) > variableLocal)
        {
            return false;
        }
    }
    return GoodDeclarations(v) && GoodExpressions(v) && GoodMarkups(v) && GoodMatcher(v) &&
           GoodPieces(v);
}

// The bytes of every string the records name, a variable's name once
// (its references name the variable, not the string). The compiler
// never shares a string, so they fit in the pool; a message whose
// strings overlap to exceed it would make writing it back cost more
// than its size, and is refused.
static uint64_t StringBytes(const MessageView* v)
{
    uint64_t total = 0;
    for (uint32_t i = 0; i < v->count[messageExpressions]; i++)
    {
        total += Field(v, messageExpressions, i, 4);
        if (Field(v, messageExpressions, i, 0) == operandLiteral)
        {
            total += Field(v, messageExpressions, i, 2);
        }
    }
    for (uint32_t i = 0; i < v->count[messageMarkups]; i++)
    {
        total += Field(v, messageMarkups, i, 2);
    }
    for (uint32_t i = 0; i < v->count[messageParameters]; i++)
    {
        total += Field(v, messageParameters, i, 2);
        if (Field(v, messageParameters, i, 0) != parameterOptionVariable)
        {
            total += Field(v, messageParameters, i, 4);
        }
    }
    for (uint32_t i = 0; i < v->count[messagePieces]; i++)
    {
        total += Field(v, messagePieces, i, 0) == pieceText ? Field(v, messagePieces, i, 2) : 0;
    }
    for (uint32_t i = 0; i < v->count[messageVariables]; i++)
    {
        total += Field(v, messageVariables, i, 1);
    }
    for (uint32_t i = 0; i < v->count[messageVariants]; i++)
    {
        for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
        {
            uint32_t offset;
            uint32_t length;
            mlocMessageKey(v, i, s, &offset, &length);
            total += length;
        }
    }
    return total;
}

// Whether each expression and markup is named at most once, in the
// compiler's order (the declarations', then the pieces'), and the
// parameter and piece ranges together fit their records: so writing a
// message back visits each record a bounded number of times.
static bool BoundedReferences(const MessageView* v)
{
    uint64_t next = 0;
    for (uint32_t d = 0; d < v->count[messageDeclarations]; d++)
    {
        uint32_t e = Field(v, messageDeclarations, d, 2);
        if (e < next)
        {
            return false;
        }
        next = (uint64_t)e + 1;
    }
    uint64_t nextMarkup = 0;
    for (uint32_t i = 0; i < v->count[messagePieces]; i++)
    {
        uint32_t kind = Field(v, messagePieces, i, 0);
        uint32_t a = Field(v, messagePieces, i, 1);
        uint64_t* order = kind == pieceExpression ? &next
                          : kind == pieceMarkup   ? &nextMarkup
                                                  : NULL;
        if (order != NULL)
        {
            if (a < *order)
            {
                return false;
            }
            *order = (uint64_t)a + 1;
        }
    }
    uint64_t parameters = 0;
    for (uint32_t i = 0; i < v->count[messageExpressions]; i++)
    {
        parameters += Field(v, messageExpressions, i, 6);
    }
    for (uint32_t i = 0; i < v->count[messageMarkups]; i++)
    {
        parameters += Field(v, messageMarkups, i, 4);
    }
    uint64_t pieces = 0;
    for (uint32_t i = 0; i < v->count[messageVariants]; i++)
    {
        pieces += Field(v, messageVariants, i, 1);
    }
    return parameters <= v->count[messageParameters] && pieces <= v->count[messagePieces];
}

static uint32_t UnicodeVersion(void)
{
    muniVersion version = muniGetUnicodeVersion();
    return (uint32_t)version.major << 16 | (uint32_t)version.minor << 8 | version.patch;
}

static const AssetFormat messageAsset = {
    .magic = MLOC_MESSAGE_MAGIC,
    .version = messageFormatVersion,
    .flags = messageComplex,
    .tags = messageTags,
    .tagCount = messageFieldCount,
    .required = 0,
};

// One field: a record kind's whole records, the keys, or the Unicode
// version, which must be this one's.
static mlocResult ReadField(const uint8_t* bytes, uint32_t payload, uint32_t field, uint32_t length,
                            MessageView* v, uint64_t* keyBytes)
{
    if (field < messageCount)
    {
        if (length % messageRecordSizes[field] != 0)
        {
            return mloc_errorFormat;
        }
        v->start[field] = payload;
        v->count[field] = length / messageRecordSizes[field];
    }
    else if (field == messageFieldKeys)
    {
        v->keyStart = payload;
        *keyBytes = length;
    }
    else if (length != messageUnicodeSize)
    {
        return mloc_errorFormat;
    }
    else if (mlocLoad32(bytes + payload) != UnicodeVersion())
    {
        return mloc_errorVersion;
    }
    return mloc_success;
}

mlocResult mlocReadMessageLayout(const uint8_t* bytes, size_t size, MessageView* v)
{
    uint32_t flags = 0;
    AssetField fields[messageFieldCount];
    mlocResult result = mlocReadAsset(bytes, size, &messageAsset, &flags, fields);
    if (result != mloc_success)
    {
        return result;
    }
    memset(v, 0, sizeof *v);
    v->bytes = bytes;
    v->flags = flags;
    uint64_t keyBytes = 0;
    for (uint32_t place = 0; place < messageFieldCount; place++)
    {
        result = fields[place].length == 0
                     ? mloc_success
                     : ReadField(bytes, fields[place].start, messageFieldKinds[place],
                                 fields[place].length, v, &keyBytes);
        if (result != mloc_success)
        {
            return result;
        }
    }
    // A simple message has neither declarations nor a matcher; a matcher
    // has both selectors and variants or neither, and a key for each
    // selector of each variant.
    bool simple = (v->flags & messageComplex) == 0;
    if ((simple && v->count[messageDeclarations] + v->count[messageVariants] > 0) ||
        (v->count[messageSelectors] == 0) != (v->count[messageVariants] == 0) ||
        keyBytes !=
            (uint64_t)v->count[messageSelectors] * v->count[messageVariants] * messageKeySize)
    {
        return mloc_errorFormat;
    }
    v->pool = bytes + v->start[messagePool];
    return mloc_success;
}

mlocResult mlocReadMessage(const uint8_t* bytes, size_t size, MessageView* view)
{
    MessageView v;
    mlocResult result = mlocReadMessageLayout(bytes, size, &v);
    if (result != mloc_success)
    {
        return result;
    }
    // The bounds first, from the counts alone, so that checking the
    // strings costs no more than the pool.
    if (StringBytes(&v) > v.count[messagePool] || !BoundedReferences(&v) || !GoodRecords(&v))
    {
        return mloc_errorFormat;
    }
    *view = v;
    return mloc_success;
}
