// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// CLDR's UnicodeSet patterns as its exemplar and parse sets write them:
// a bracketed list of characters, ranges ("a-z") and strings in braces
// ("{ch}"), spaces between them ignored, a backslash escaping the next
// character or starting \uXXXX or \UXXXXXXXX. A hyphen with no
// character before it, or none after it, is itself. A property, a
// nested set, an operator or any other escape stops the program, so a
// CLDR release that starts writing one is noticed rather than misread.

#ifndef MLOCGEN_SET_PATTERN_H
#define MLOCGEN_SET_PATTERN_H

#include <stddef.h>
#include <stdint.h>

typedef struct SetSink
{
    // A range of code points, first <= last (a character alone is one).
    void (*range)(void* context, uint32_t first, uint32_t last);
    // A string's UTF-8, its escapes resolved.
    void (*string)(void* context, const char* text, size_t length);
    void* context;
} SetSink;

// Reads a pattern into a sink, in the pattern's order.
void GenReadSetPattern(const char* pattern, const SetSink* sink);

#endif
