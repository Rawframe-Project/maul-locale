// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An XLIFF unit's codes: a message's placeholders, markup and syntax
// (mf2_parts.h) as inline codes over the unit's original data, one <data>
// per distinct text, shared by the source and the target, and by every
// segment of a unit. A source's codes are numbered on from the unit's last
// number; a target's code takes the number of the first code of its source
// with the same data not yet taken, else a new one. Markup open and close
// in one pattern pair as <sc> and <ec>; one left alone is isolated.

#ifndef MLOCCAT_XLIFF_CODES_H
#define MLOCCAT_XLIFF_CODES_H

#include "mf2_parts.h"
#include "resource.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct XliffMessage
{
    const char* text;
    // A pattern's text, owned: it is split as a quoted pattern, whose
    // quotes are not codes.
    char* owned;
    Mf2Part* parts;
    size_t count;
    // Per part: its data's index, its code's number, and for markup its
    // partner's part index, or SIZE_MAX when isolated.
    size_t* data;
    size_t* number;
    size_t* partner;
} XliffMessage;

typedef struct XliffCodes
{
    ResText* data;
    size_t dataCount;
    // Sources and targets, in the order added.
    XliffMessage* messages;
    size_t messageCount;
    size_t numbers;
} XliffCodes;

// A message that is a source, not a target of one.
constexpr size_t xliffSource = SIZE_MAX;

// Adds a message: a source, or a target of the source added as index
// source. Gives its index; a message that does not split has no parts.
size_t XliffCodesAdd(XliffCodes* codes, const ResText* message, size_t source);

// Adds a pattern (a variant's, as written between its quotes) the same
// way.
size_t XliffCodesAddPattern(XliffCodes* codes, const char* pattern, size_t length, size_t source);

// The <originalData> element, when there is data.
void XliffCodesWriteData(const XliffCodes* codes, FILE* out);

// A message's content: text with MF2's escapes undone, and its codes.
void XliffCodesWriteMessage(const XliffCodes* codes, size_t index, FILE* out);

void XliffCodesFree(XliffCodes* codes);

// Text escaped for XML: in an attribute or in content, where a character
// XML cannot hold is XLIFF's <cp>.
void XliffWriteText(FILE* out, const char* text, size_t length, int attribute);

#endif
