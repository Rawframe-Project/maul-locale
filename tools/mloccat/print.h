// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formatted text appended to a buffer of a capacity, refused rather than
// cut short.

#ifndef MLOCCAT_PRINT_H
#define MLOCCAT_PRINT_H

#include <stddef.h>

// Appends printf's output to a text of a capacity at *size, NUL-ended,
// and moves *size past it; false, with *size and the text before it
// unchanged, when it does not fit.
[[gnu::format(printf, 4, 5)]] bool TextPrint(char* text, size_t capacity, size_t* size,
                                             const char* format, ...);

#endif
