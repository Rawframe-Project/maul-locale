// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// MF2's grammar (UTS #35 Part 9, message.abnf): parses a message's text
// and emits its records. The data model errors that need nothing but
// the parse (a variant's key count, the fallback variant) are found
// here, and reported only when the whole text parses.

#ifndef MLOC_SRC_MESSAGE_PARSE_H
#define MLOC_SRC_MESSAGE_PARSE_H

#include "message_emit.h"

#include "maul-locale/message.h"

#include <stddef.h>
#include <stdint.h>

// A parse's outcome: the error (none on success), where it was found,
// and the header's flags.
typedef struct MessageParse
{
    mlocMessageError error;
    size_t errorAt;
    uint32_t flags;
} MessageParse;

// Parses well-formed UTF-8 text into the emitter.
MessageParse mlocParseMessage(const uint8_t* text, size_t length, MessageEmit* emit);

#endif
