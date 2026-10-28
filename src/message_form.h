// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The compiled message's layout, which message_compile writes and
// message_read checks: an asset (asset.h, docs/internals/data.md#asset-
// formats), the magic "MLMS", messageFormatVersion, a flag for a complex
// message, and fields in the order of messageTags, none required: a field
// that would hold no records is left out. All numbers are 32-bit
// little-endian. Each record kind is a field of whole records:
// declarations, expressions, markups, parameters (options and
// attributes), selectors, keys (one per selector per variant), variants,
// pieces, variables, and the pool of string bytes. A string is an offset
// into the pool and a length. Last, only when a string the compiler
// normalized holds a code point its Unicode version leaves unassigned,
// that Unicode version, whose NFC the string is.
//
// An asset layout: tools/check_data_format.py watches this header with messageFormatVersion.

#ifndef MLOC_MESSAGE_FORM_H
#define MLOC_MESSAGE_FORM_H

#include "asset.h"
#include "bytes.h"

#include <stdint.h>

// The first four bytes.
#define MLOC_MESSAGE_MAGIC "MLMS"

enum
{
    // Before 1.0.0 it changes whenever the bytes do.
    messageFormatVersion = 2,
    // The header's flag: a complex message (declarations or a quoted body).
    messageComplex = 1,
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

// The fields: one per record kind, in the order of messageCount but for
// the keys, which come before the variants, then the Unicode version.
enum
{
    messageFieldKeys = messageCount,
    messageFieldUnicode = messageCount + 1,
    messageFieldCount = messageCount + 2,
};

// Each field's tag in its order in a message, and what it holds (a
// record kind, messageFieldKeys or messageFieldUnicode).
static const uint32_t messageTags[messageFieldCount] = {
    MLOC_ASSET_TAG('D', 'E', 'C', 'L'), MLOC_ASSET_TAG('E', 'X', 'P', 'R'),
    MLOC_ASSET_TAG('M', 'A', 'R', 'K'), MLOC_ASSET_TAG('P', 'A', 'R', 'M'),
    MLOC_ASSET_TAG('S', 'E', 'L', 'E'), MLOC_ASSET_TAG('K', 'E', 'Y', 'S'),
    MLOC_ASSET_TAG('V', 'N', 'T', 'S'), MLOC_ASSET_TAG('P', 'I', 'E', 'C'),
    MLOC_ASSET_TAG('V', 'A', 'R', 'S'), MLOC_ASSET_TAG('P', 'O', 'O', 'L'),
    MLOC_ASSET_TAG('U', 'N', 'I', 'C'),
};
static const uint32_t messageFieldKinds[messageFieldCount] = {
    messageDeclarations, messageExpressions, messageMarkups,      messageParameters,
    messageSelectors,    messageFieldKeys,   messageVariants,     messagePieces,
    messageVariables,    messagePool,        messageFieldUnicode,
};

// The Unicode version's payload: major << 16 | minor << 8 | patch.
static const uint32_t messageUnicodeSize = 4;

#endif
