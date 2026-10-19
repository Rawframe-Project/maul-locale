// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An XLIFF unit's codes (record mloc-0060): a message's placeholders,
// markup and syntax (mf2_parts.h) as inline codes over the unit's
// original data, one <data> per distinct text, shared by the source and
// the target. The source's codes are numbered in order; a target's code
// takes the number of the first source code of the same data not yet
// taken, else a new one. Markup open and close in one pattern pair as
// <sc> and <ec>; one left alone is isolated.

#ifndef MLOCCAT_XLIFF_CODES_H
#define MLOCCAT_XLIFF_CODES_H

#include "mf2_parts.h"
#include "resource.h"

#include <stddef.h>
#include <stdio.h>

typedef struct XliffMessage
{
    const char* text;
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
    // The source (0) and the target (1).
    XliffMessage messages[2];
    size_t numbers;
} XliffCodes;

// Adds the source (which 0, first) or the target (which 1); a message
// that does not split adds nothing.
void XliffCodesAdd(XliffCodes* codes, const ResText* message, int which);

// The <originalData> element, when there is data.
void XliffCodesWriteData(const XliffCodes* codes, FILE* out);

// A message's content: text with MF2's escapes undone, and its codes.
void XliffCodesWriteMessage(const XliffCodes* codes, int which, FILE* out);

void XliffCodesFree(XliffCodes* codes);

// Text escaped for XML: in an attribute or in content, where a character
// XML cannot hold is XLIFF's <cp>.
void XliffWriteText(FILE* out, const char* text, size_t length, int attribute);

#endif
