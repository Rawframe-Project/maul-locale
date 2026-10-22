// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "set_pattern.h"

#include "table_gen.h"

#include "maul-unicode/encoding.h"

#include <string.h>

enum
{
    maxString = 64,
};

static int Hex(char c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}

static bool Letter(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

// A \u or \U escape's code point at a place; the place moves past it.
static uint32_t Escape(const char* pattern, size_t end, size_t* at)
{
    const char* p = pattern + *at;
    size_t digits = p[1] == 'u' ? 4 : 8;
    uint32_t cp = 0;
    for (size_t i = 0; i < digits; i++)
    {
        int h = *at + 2 + i < end ? Hex(p[2 + i]) : -1;
        if (h < 0)
        {
            GenDie("a set pattern's \\u escape without its digits", pattern);
        }
        cp = cp * 16 + (uint32_t)h;
    }
    if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
    {
        GenDie("a set pattern's escape that is no scalar value", pattern);
    }
    *at += 2 + digits;
    return cp;
}

// The character at a place, an escape resolved; the place moves past it.
static uint32_t Character(const char* pattern, size_t end, size_t* at)
{
    const char* p = pattern + *at;
    if (*p == '\\')
    {
        if (*at + 1 >= end)
        {
            GenDie("a set pattern's escape without its character", pattern);
        }
        if (p[1] == 'u' || p[1] == 'U')
        {
            return Escape(pattern, end, at);
        }
        if (Letter(p[1]))
        {
            GenDie("a set pattern's escape the reader does not take", pattern);
        }
        p++;
        *at += 1;
    }
    else if (strchr("[]{}&$", *p) != nullptr)
    {
        GenDie("a set pattern's operator the reader does not take", pattern);
    }
    uint32_t cp = 0;
    size_t size = 0;
    if (muniDecodeUtf8(p, end - (size_t)(p - pattern), &cp, &size) != muni_success)
    {
        GenDie("a set pattern that is not UTF-8", pattern);
    }
    *at += size;
    return cp;
}

// A string in braces from the place after its opening brace, up to the
// closing one, which the place moves past.
static void String(const char* pattern, size_t end, size_t* at, const SetSink* sink)
{
    char text[maxString];
    size_t length = 0;
    while (*at < end && pattern[*at] != '}')
    {
        char bytes[4];
        size_t size = 0;
        uint32_t cp = Character(pattern, end, at);
        if (muniEncodeUtf8(cp, bytes, &size) != muni_success || length + size > sizeof(text))
        {
            GenDie("a set pattern's string too long", pattern);
        }
        memcpy(text + length, bytes, size);
        length += size;
    }
    if (*at == end || length == 0)
    {
        GenDie("a set pattern's string empty or without its end", pattern);
    }
    *at += 1;
    sink->string(sink->context, text, length);
}

void GenReadSetPattern(const char* pattern, const SetSink* sink)
{
    size_t length = strlen(pattern);
    if (length < 2 || pattern[0] != '[' || pattern[length - 1] != ']' || pattern[1] == '^' ||
        pattern[1] == ':')
    {
        GenDie("a set pattern that is not a bracketed list", pattern);
    }
    size_t end = length - 1;
    // The character read last, which a hyphen may make a range's first.
    int64_t previous = -1;
    for (size_t at = 1; at < end;)
    {
        char c = pattern[at];
        if (c == ' ')
        {
            at++;
            continue;
        }
        if (c == '{')
        {
            if (previous >= 0)
            {
                sink->range(sink->context, (uint32_t)previous, (uint32_t)previous);
            }
            at++;
            String(pattern, end, &at, sink);
            previous = -1;
            continue;
        }
        size_t next = at + 1;
        while (next < end && pattern[next] == ' ')
        {
            next++;
        }
        if (c == '-' && previous >= 0 && next < end)
        {
            at = next;
            uint32_t last = Character(pattern, end, &at);
            if (last < (uint32_t)previous)
            {
                GenDie("a set pattern's range backwards", pattern);
            }
            sink->range(sink->context, (uint32_t)previous, last);
            previous = -1;
            continue;
        }
        if (previous >= 0)
        {
            sink->range(sink->context, (uint32_t)previous, (uint32_t)previous);
        }
        previous = Character(pattern, end, &at);
    }
    if (previous >= 0)
    {
        sink->range(sink->context, (uint32_t)previous, (uint32_t)previous);
    }
}
