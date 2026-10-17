// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The character classes of MF2's grammar (UTS #35 Part 9, message.abnf)
// over text already checked as well-formed UTF-8.

#ifndef MLOC_SRC_MESSAGE_TEXT_H
#define MLOC_SRC_MESSAGE_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The braces, by code: a brace in a character literal would read as a
// block's to the tools that measure functions.
enum
{
    messageOpenBrace = 0x7B,
    messageCloseBrace = 0x7D,
};

// Decodes the code point at text[at], which must be the start of a
// well-formed sequence before end, and sets *sizeOut to its length.
uint32_t mlocMessageDecode(const uint8_t* text, size_t at, size_t end, size_t* sizeOut);

// ws: space, tab, CR, LF, U+3000.
bool mlocMessageIsSpace(uint32_t c);

// bidi: ALM, LRM, RLM and the isolates LRI, RLI, FSI and PDI.
bool mlocMessageIsBidi(uint32_t c);

// name-start: the letters and most code points past ASCII, without
// whitespace, controls and bidi controls.
bool mlocMessageIsNameStart(uint32_t c);

// name-char: name-start, digits, "-" and ".".
bool mlocMessageIsNameChar(uint32_t c);

#endif
