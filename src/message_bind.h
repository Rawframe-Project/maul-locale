// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message formatter and bound messages (record mloc-0045). A bound
// message is a header, copied in and out by bytes since the caller's
// buffer may have any alignment, then a word per variable (the
// declaration binding it, or none) and a word per expression (its
// function's slot, and in the top byte the length of its chain of
// declarations), then a word per expression for the resolutions it
// makes, then a word per expression for its prepared value, and the
// prepared values. It points into the compiled message and the
// formatter.

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
    // The default time zone, when hasZone.
    bool hasZone;
    mlocTimeZone zone;
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
// have, one of the default functions, or a program function (its index
// past the first).
enum
{
    messageFunctionNone = 0,
    messageFunctionUnknown = 1,
    messageFunctionString = 2,
    messageFunctionNumber = 3,
    messageFunctionInteger = 4,
    messageFunctionOffset = 5,
    messageFunctionPercent = 6,
    messageFunctionCurrency = 7,
    messageFunctionUnit = 8,
    messageFunctionDate = 9,
    messageFunctionTime = 10,
    messageFunctionDateTime = 11,
    messageFunctionProgram = 12,
};

// Slots take 24 bits: the program's functions are fewer.
static const uint32_t messageFunctionLimit = (1u << 24) - messageFunctionProgram;

// The most selectors a bound message has: their values are kept on the
// stack while a variant is chosen.
static const uint32_t messageSelectorLimit = 32;

// The most declarations a bound message has: a format call keeps a bit
// for each, whether its faults were reported.
enum
{
    messageDeclarationLimit = 1024,
};

// The longest chain of expressions, each naming the next through a
// declaration, that a bound message resolves on the stack.
static const uint32_t messageChainLimit = 64;

// The expressions and markups a message may have: a part's kind holds an
// index in 24 bits.
static const uint32_t messageIndexLimit = 1u << 24;

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
// the expressions' resolution counts, then where each expression's
// prepared value is (record mloc-0045; message_prepare.h).
const uint8_t* mlocBoundWords(const void* bound);

// Reads and checks what a bind is given: the formatter, and the compiled
// message within a bound message's limits; mloc_errorInvalid, or the
// compiled message's error.
mlocResult mlocOpenBind(const mlocMessageFormatter* formatter, const void* message, size_t size,
                        MessageFormatter* formatterOut, BoundHeader* headerOut);

// The bytes of a bound message's header and words.
uint64_t mlocBoundBase(const MessageView* view);

// An expression's function slot.
uint32_t mlocExpressionSlot(const MessageView* view, const MessageFormatter* formatter,
                            uint32_t expression);

// Writes the header and the words of a bound message of size bytes,
// no value prepared; mloc_errorInvalid past the chain or resolution
// limit, mloc_errorData for a declaration naming a later one.
mlocResult mlocWriteBound(const mlocMessageFormatter* formatter, const MessageFormatter* f,
                          BoundHeader* h, void* buffer, uint32_t size);

#endif
