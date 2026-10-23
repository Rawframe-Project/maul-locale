// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// PO files (GNU gettext's format) as the catalog tool exchanges them:
// entries of one msgctxt, msgid and msgstr, their extracted comments (#.)
// and flags (#,). Plural entries (msgid_plural, msgstr[n]) are refused, as
// MF2 selects inside the message; obsolete entries (#~) and other comments
// are read past. Strings take C's escapes as gettext writes them (\n \t \"
// \\ and the rest of C's).

#ifndef MLOCCAT_PO_H
#define MLOCCAT_PO_H

#include <stddef.h>
#include <stdio.h>

typedef struct PoEntry
{
    // NUL-terminated and owned; context null when the entry has none.
    char* context;
    char* id;
    char* string;
    // The extracted comments, lines joined by LF; null for none.
    char* comments;
    int fuzzy;
    int mf2;
    // The line of its msgid, from 1.
    size_t line;
} PoEntry;

typedef struct PoFile
{
    // The header entry's msgstr; null when the file has none.
    char* header;
    PoEntry* entries;
    size_t count;
} PoFile;

typedef struct PoError
{
    size_t line;
    const char* reason;
} PoError;

// Reads a PO file; 0 and the first problem when it is not one the tool
// takes.
int PoRead(const char* text, size_t length, PoFile* out, PoError* error);

// Writes a PO file: the header, then each entry's comments, flags,
// msgctxt, msgid and msgstr, strings split after each LF.
void PoWrite(const PoFile* po, FILE* out);

void PoFree(PoFile* po);

#endif
