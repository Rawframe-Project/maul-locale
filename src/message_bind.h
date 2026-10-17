// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message formatter and bound messages (record mloc-0045). A bound
// message is a header, copied in and out by bytes since the caller's
// buffer may have any alignment, then a word per variable (the
// declaration binding it, or none) and a word per expression (its
// function's slot, and in the top byte the length of its chain of
// declarations), then a word per expression for the resolutions it
// makes. It points into the compiled message and the formatter.

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
    // Whether placeholders are isolated, and the message's direction.
    bool isolate;
    mlocDirection direction;
    const mlocDataSet* data;
    const mlocMessageFunction* functions;
    size_t functionCount;
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

// An expression's function slot: none, a function the formatter does not
// have, :string, or a program function (its index past the first).
enum
{
    messageFunctionNone = 0,
    messageFunctionUnknown = 1,
    messageFunctionString = 2,
    messageFunctionProgram = 3,
};

// Slots take 24 bits: the program's functions are fewer.
static const uint32_t messageFunctionLimit = (1u << 24) - messageFunctionProgram;

// The most selectors a bound message has: their values are kept on the
// stack while a variant is chosen.
static const uint32_t messageSelectorLimit = 32;

// The longest chain of expressions, each naming the next through a
// declaration, that a bound message resolves on the stack.
static const uint32_t messageChainLimit = 64;

// The most expression resolutions a format call makes. Each reference to
// a local resolves it again (call-by-name), so a message whose locals are
// each named twice by the next grows as a power of two; binding refuses
// one whose count, each option read once, is past the limit, and a call
// stops resolving there whatever its functions read.
static const uint32_t messageResolutionLimit = 1u << 16;

// Reads a formatter the library made; false for any other bytes.
bool mlocReadMessageFormatter(const mlocMessageFormatter* formatter, MessageFormatter* out);

// Reads a bound message's header and its formatter; false for bytes the
// library did not bind.
bool mlocReadBound(const void* bound, BoundHeader* headerOut, MessageFormatter* formatterOut);

// A bound message's words: the variables', then the expressions', then
// the expressions' resolution counts.
const uint8_t* mlocBoundWords(const void* bound);

#endif
