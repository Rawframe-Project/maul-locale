// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The compiled message's layout, which message_compile writes and
// message_read checks. All numbers are 32-bit little-endian. A header,
// then the records of each kind in this order, each kind's count in the
// header: declarations, expressions, markups, parameters (options and
// attributes), selectors, keys (one per selector per variant), variants,
// pieces, variables, and last the pool of string bytes. A string is an
// offset into the pool and a length.

#ifndef MLOC_MESSAGE_FORM_H
#define MLOC_MESSAGE_FORM_H

#include "bytes.h"

#include <stdint.h>

// The first four bytes.
#define MLOC_MESSAGE_MAGIC "MLMS"

static const uint32_t messageFormatVersion = 1;
static const uint32_t messageHeaderSize = 56;
// Header flags: a complex message (declarations or a quoted body).
static const uint32_t messageComplex = 1;

// The header's fields, by byte offset.
enum
{
    messageMagic = 0,
    messageFormat = 4, // 16 bits
    messageFlags = 6,  // 16 bits
    messageSize = 8,
    messageDataFormat = 12, // MLOC_DATA_FORMAT_VERSION
    messageUnicode = 16,    // the Unicode version of its NFC
    messageCounts = 20,     // nine counts, in the order of messageCount
};

// The kinds of records, in their order in the message.
enum
{
    messageDeclarations = 0,
    messageExpressions = 1,
    messageMarkups = 2,
    messageParameters = 3,
    messageSelectors = 4,
    messageVariants = 5,
    messagePieces = 6,
    messageVariables = 7,
    messagePool = 8,
    messageCount = 9,
};

// Each kind's record size; keys have no count of their own (selectors
// times variants), so they follow the variants' count in the sizes.
static const uint32_t messageRecordSizes[messageCount] = {12, 32, 24, 20, 8, 12, 12, 12, 1};
static const uint32_t messageKeySize = 12;

// A declaration: kind, variable, expression.
enum
{
    declarationInput = 0,
    declarationLocal = 1,
};

// An expression: operand kind, operand (a literal's string, or a
// variable and 0), function (a string; length 0 for none), its first
// parameter and their count, and its byte offset in the source.
enum
{
    operandNone = 0,
    operandLiteral = 1,
    operandVariable = 2,
};

// A markup: kind, name (a string), first parameter, count, source offset.
enum
{
    markupOpen = 0,
    markupStandalone = 1,
    markupClose = 2,
};

// A parameter: kind, name (a string), value (a string, or a variable and
// 0; nothing for a bare attribute).
enum
{
    parameterOptionLiteral = 0,
    parameterOptionVariable = 1,
    parameterAttribute = 2,
    parameterAttributeLiteral = 3,
};

// A selector: its variable, and the variable's byte offset in the source.

// A key: kind, string (nothing for the catch-all).
enum
{
    keyCatchAll = 0,
    keyLiteral = 1,
};

// A variant: its pattern's first piece and count, and its byte offset in
// the source. Without variants the body is every piece.

// A piece: kind, then a string for text or an index for the others.
enum
{
    pieceText = 0,
    pieceExpression = 1,
    pieceMarkup = 2,
};

// A variable: name (a string, NFC), and its kind: an external input
// never declared, one bound by .input, or one bound by .local.
enum
{
    variableExternal = 0,
    variableInput = 1,
    variableLocal = 2,
};

// Each kind's first byte in a message whose counts are known to fit (one
// compiled here), and the keys'.
static inline void mlocMessageStarts(const uint8_t* message, uint32_t* starts, uint32_t* keyStart)
{
    uint32_t at = messageHeaderSize;
    uint32_t selectors = mlocLoad32(message + messageCounts + messageSelectors * 4);
    uint32_t variants = mlocLoad32(message + messageCounts + messageVariants * 4);
    for (uint32_t kind = 0; kind < messageCount; kind++)
    {
        if (kind == messageVariants)
        {
            *keyStart = at;
            at += selectors * variants * messageKeySize;
        }
        starts[kind] = at;
        at += mlocLoad32(message + messageCounts + kind * 4) * messageRecordSizes[kind];
    }
}

#endif
