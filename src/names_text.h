// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// How the names part writes (records mloc-0037 and mloc-0038): text
// appended as long as it fits, with CLDR's nested bracket replacement
// when asked, and the first letter titlecased as ICU's display names
// titlecase it.

#ifndef MLOC_NAMES_TEXT_H
#define MLOC_NAMES_TEXT_H

#include "locale_context.h"

#include "maul-locale/locale.h"

#include <stddef.h>
#include <stdint.h>

// Where names are written: the bytes that fit go to the buffer, length
// counts them all; the first four are kept apart for the titlecase.
typedef struct NameWriter
{
    char* buffer;
    size_t capacity;
    size_t length;
    uint8_t head[4];
} NameWriter;

// Appends UTF-8 text; with brackets, "(" and ")" and their full-width
// forms become "[", "]", "［" and "］" (root's nestedBracketReplacement,
// which every CLDR 49 locale inherits).
void mlocNameAppend(NameWriter* w, const uint8_t* text, size_t length, bool brackets);

// Whether a context titlecases a usage's names, as ICU decides: at the
// beginning of a sentence always, in a list or menu or alone as the
// locale's context transforms say; contextUsageCount for a usage with
// no transforms.
bool mlocNameCapitalizes(const mlocDataSet* data, const mlocLocale* locale, ContextUsage usage,
                         mlocCapitalization capitalization);

// Titlecases the first code point written: ASCII by the library ("i" as
// Turkish and Azerbaijani titlecase it), the rest by names.mloc's
// titlecase table; in place when the text fits, else in the length.
void mlocNameCapitalize(const mlocDataSet* data, const mlocLocale* locale, NameWriter* w);

// Sets an output's length; mloc_errorCapacity when the text did not fit.
mlocResult mlocNameFinish(const NameWriter* w, mlocOutput* output);

#endif
