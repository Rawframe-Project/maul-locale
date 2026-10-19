// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Stale translations (record mloc-0056). A translation records the hash
// of the source message it was made from as @mloc-source; it is stale
// when the source's hash is now another. The hash is XXH64 (seed 0) of
// the source message's canonical text, the message compiled and written
// back by mlocWriteMessageText, so whitespace and quoting do not count,
// and text, options and variants do. It is written as 16 lowercase hex
// digits.

#ifndef MLOCCAT_STALE_H
#define MLOCCAT_STALE_H

#include "resource.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    sourceHashDigits = 16,
};

// The hash of an MF2 message; 0 when it does not compile.
int SourceHash(const char* text, size_t length, uint64_t* out);

// The hash as 16 lowercase hex digits and a NUL.
void SourceHashText(uint64_t hash, char out[sourceHashDigits + 1]);

// Reads 16 lowercase hex digits; 0 for anything else.
int SourceHashRead(const char* text, size_t length, uint64_t* out);

typedef enum SourceState
{
    // No @mloc-source: nothing says which source it answers.
    sourceUnrecorded,
    sourceFresh,
    sourceStale,
} SourceState;

// A translation entry's state against its source message now, by its
// metadata.
SourceState SourceStateOf(const ResMeta* meta, size_t count, const ResText* source);

#endif
