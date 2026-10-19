// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Pseudo-locales (record mloc-0058), written from a source as catalog
// sources of its set, so that a program loads them as translations and a
// tester sees what would break:
// - en-XA: each pattern's letters accented ("Ŝéţţîñĝš"), the pattern in
//   brackets, padded by 40% of its text's length with middle dots, so
//   truncation, concatenation and hard-coded text show;
// - ar-XB: each run of a pattern's text between RLO and PDF, so it shows
//   mirrored in a right-to-left layout.
// Only pattern text changes: placeholders, markup, declarations, options
// and variant keys stay as written, and escapes stay escapes.

#ifndef MLOCCAT_PSEUDO_H
#define MLOCCAT_PSEUDO_H

#include <stddef.h>
#include <stdio.h>

typedef enum PseudoKind
{
    pseudoAccented,
    pseudoBidi,
} PseudoKind;

// The pseudo form of an MF2 message, NUL-terminated; the caller frees it.
// Null when the message does not compile, or (never expected) when its
// pseudo form would not.
char* PseudoMessage(const char* text, size_t length, PseudoKind kind, size_t* lengthOut);

// Writes en-XA.mf2 and ar-XB.mf2 into the directory from the source;
// writes problems and what was written to report. Returns 0 when the
// source does not read or check, or a file could not be written.
int CatalogPseudo(const char* directory, const char* source, FILE* report);

#endif
