// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An MF2 message split into what a translator changes and what they must
// not: each pattern's text, its placeholders and markup, and the syntax
// around the patterns (declarations, selectors, variant keys, the quotes
// of a quoted pattern and the message's leading space). The parts tile the
// message in order, so their bytes joined give it back; a pattern's start
// and end are marked by parts of no length.

#ifndef MLOCCAT_MF2_PARTS_H
#define MLOCCAT_MF2_PARTS_H

#include <stddef.h>

typedef enum Mf2PartKind
{
    // Pattern text, escapes as written.
    mf2Text,
    // A placeholder: an expression in a pattern.
    mf2Placeholder,
    // Markup in a pattern: {#open}, {/close}, {#standalone/}.
    mf2MarkupOpen,
    mf2MarkupClose,
    mf2MarkupStandalone,
    // Syntax outside the patterns.
    mf2Syntax,
    // A pattern's start and end, of no length.
    mf2PatternStart,
    mf2PatternEnd,
} Mf2PartKind;

typedef struct Mf2Part
{
    Mf2PartKind kind;
    size_t start;
    size_t length;
} Mf2Part;

// Splits a message that compiles; the caller frees the parts. 0 for a
// message that does not compile.
int Mf2Split(const char* text, size_t length, Mf2Part** partsOut, size_t* countOut);

// The length of MF2's optional space at at (space, tab, CR, LF, U+3000,
// ALM, LRM, RLM and the isolates); 0 for none.
size_t Mf2SpaceAt(const char* text, size_t length, size_t at);

#endif
