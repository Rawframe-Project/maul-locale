// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message formatter and bound messages (record mloc-0045). A bound
// message is a header, copied in and out by bytes since the caller's
// buffer may have any alignment, then a word per variable (the
// declaration binding it, or none) and a word per expression (its
// function's slot, and in the top byte the length of its chain of
// declarations). It points into the compiled message and the formatter.

#ifndef MLOC_SRC_MESSAGE_BIND_H
#define MLOC_SRC_MESSAGE_BIND_H

#include "message_read.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct MessageFormatter
{
    uint8_t stamp;
    bool strict;
    const mlocDataSet* data;
    mlocLocale locale;
} MessageFormatter;

typedef struct BoundHeader
{
    uint32_t magic;
    uint32_t size;
    const mlocMessageFormatter* formatter;
    MessageView view;
} BoundHeader;

// A variable bound by no declaration.
static const uint32_t messageUnbound = UINT32_MAX;

// An expression's function slot: none, or a function the formatter does
// not have.
enum
{
    messageFunctionNone = 0,
    messageFunctionUnknown = 1,
};

// The longest chain of expressions, each naming the next through a
// declaration, that a bound message resolves on the stack.
static const uint32_t messageChainLimit = 64;

// Reads a formatter the library made; false for any other bytes.
bool mlocReadMessageFormatter(const mlocMessageFormatter* formatter, MessageFormatter* out);

// Reads a bound message's header and its formatter; false for bytes the
// library did not bind.
bool mlocReadBound(const void* bound, BoundHeader* headerOut, MessageFormatter* formatterOut);

// A bound message's words: the variables', then the expressions'.
const uint8_t* mlocBoundWords(const void* bound);

#endif
