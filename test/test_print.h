// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formatted text appended to a test's buffer: a failed check, never a text
// cut short, when it does not fit.

#ifndef MLOC_TEST_PRINT_H
#define MLOC_TEST_PRINT_H

#include "test_harness.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

// Appends printf's output to a text of a capacity at *size, NUL-ended,
// and moves *size past it. When it does not fit, a failed check, with
// *size and the text before it unchanged.
[[gnu::format(printf, 4, 5)]] static void AppendPrint(char* text, size_t capacity, size_t* size,
                                                      const char* format, ...)
{
    int n = -1;
    if (*size < capacity)
    {
        va_list arguments;
        va_start(arguments, format);
        n = vsnprintf(text + *size, capacity - *size, format, arguments);
        va_end(arguments);
    }
    if (n < 0 || (size_t)n >= capacity - *size)
    {
        if (*size < capacity)
        {
            text[*size] = '\0';
        }
        CHECK(false, "a test's text fits its buffer");
        return;
    }
    *size += (size_t)n;
}

#endif
