// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "print.h"

#include <stdarg.h>
#include <stdio.h>

bool TextPrint(char* text, size_t capacity, size_t* size, const char* format, ...)
{
    if (*size >= capacity)
    {
        return false;
    }
    va_list arguments;
    va_start(arguments, format);
    int n = vsnprintf(text + *size, capacity - *size, format, arguments);
    va_end(arguments);
    if (n < 0 || (size_t)n >= capacity - *size)
    {
        text[*size] = '\0';
        return false;
    }
    *size += (size_t)n;
    return true;
}
