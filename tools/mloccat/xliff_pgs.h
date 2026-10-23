// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A selection message as an XLIFF unit of the Plural, Gender and Select
// module: pgs:switch names its selectors, and a segment per tuple of case
// values carries the variant that tuple selects. A plural or ordinal
// selector's values are the source's exact numbers, in numeric order, then
// the target locale's categories in CLDR's order; a select's are the
// source's keys in byte order, then other. Each segment's source is the
// source variant the tuple selects, its target the translation's; a plural
// segment notes the target's numbers that fall in its categories.

#ifndef MLOCCAT_XLIFF_PGS_H
#define MLOCCAT_XLIFF_PGS_H

#include "pgs.h"
#include "resource.h"

#include <stddef.h>

enum
{
    // More segments than this and the message is one segment.
    xliffPgsMaxSegments = 256,
};

typedef struct XliffPgsSegment
{
    // Case values, the plan's; not owned.
    const char* cases[pgsMaxSelectors];
    const PgsVariant* source;
    // Null without a translation.
    const PgsVariant* target;
    // The plural examples' note, or null; owned.
    char* examples;
} XliffPgsSegment;

typedef struct XliffPgsUnit
{
    PgsMessage source;
    PgsMessage target;
    // "plural:n select:g"; owned.
    char* switchText;
    // Each selector's case values; owned.
    char** values[pgsMaxSelectors];
    size_t valueCount[pgsMaxSelectors];
    XliffPgsSegment* segments;
    size_t segmentCount;
} XliffPgsUnit;

// Plans a unit for a source message and its translation (null when
// none) into a target locale; 0 when the message does not qualify, its
// keys are not case values, the translation selects otherwise, or it
// would take more than xliffPgsMaxSegments segments.
int XliffPgsPlan(XliffPgsUnit* out, const ResText* source, const ResText* target,
                 const char* locale);

void XliffPgsFree(XliffPgsUnit* unit);

// Folds segments back into one message: the head of a source message
// read by PgsRead, then a variant per segment, its keys its case values
// (other as '*', the rest quoted literals) and its pattern MF2 text. The
// caller frees it.
char* XliffPgsFold(const PgsMessage* source, char* const* const* cases, char* const* patterns,
                   size_t count, size_t* lengthOut);

#endif
