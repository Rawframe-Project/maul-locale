// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Selection messages as XLIFF's Plural, Gender and Select module sees
// them. A message qualifies when it declares nothing but its selectors,
// each `.input` of a variable with :number or :integer (a plural, ordinal
// by select=ordinal, or a select by select=exact) or :string (a select),
// and matches on them. It is read from its canonical text
// (mlocWriteMessageText): the declarations, the selectors and each
// variant's keys and pattern; and written back from the same parts, so a
// translation's variants become a message again.

#ifndef MLOCCAT_PGS_H
#define MLOCCAT_PGS_H

#include <stddef.h>

enum
{
    pgsMaxSelectors = 4,
};

typedef enum PgsKind
{
    pgsPlural,
    pgsOrdinal,
    pgsSelect,
} PgsKind;

typedef struct PgsSelector
{
    PgsKind kind;
    // The variable's name, without '$'; owned.
    char* name;
} PgsSelector;

typedef struct PgsVariant
{
    // A key per selector, as written ("one", "0", "|a b|", "*"); owned.
    char* keys[pgsMaxSelectors];
    // Each key as PGS's case value: a literal unquoted with its escapes
    // undone, '*' as "other"; owned.
    char* cases[pgsMaxSelectors];
    // The pattern between the quotes, as written; owned.
    char* pattern;
} PgsVariant;

typedef struct PgsMessage
{
    // The declarations and the .match line, as the canonical text has
    // them, each line ending in LF; owned.
    char* head;
    PgsSelector selectors[pgsMaxSelectors];
    size_t selectorCount;
    PgsVariant* variants;
    size_t variantCount;
} PgsMessage;

// Reads a message; 0 when it does not compile or does not qualify.
int PgsRead(const char* text, size_t length, PgsMessage* out);

void PgsFree(PgsMessage* m);

// The variant whose case values are a tuple's, else the first whose keys
// are each the tuple's value or '*'; null when none.
const PgsVariant* PgsFind(const PgsMessage* m, char* const* cases);

// A message from the head and variants (keys and patterns given apart);
// the caller frees it.
char* PgsWrite(const char* head, char* const* const* keys, char* const* patterns, size_t count,
               size_t selectorCount, size_t* lengthOut);

#endif
