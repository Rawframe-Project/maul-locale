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

// The offset of a name's first code point whose NFC form leaves the
// name no name, or the name's length when none does (record mloc-0045).
// Names are stored in NFC and read back as names; of the code points
// NFC changes, a few become characters no name holds (U+037E becomes
// ";", U+1FEF "`"). Composition with a neighbour never does: a
// composite is a name character whenever its parts are (checked over
// every canonical composite), so each code point is checked alone.
size_t mlocMessageNfcNameBreak(const uint8_t* name, size_t length);

#endif
