// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A caller's locale checked (locale_valid.h).

#include "locale_valid.h"

#include <stddef.h>
#include <stdint.h>

static bool IsLower(char c)
{
    return c >= 'a' && c <= 'z';
}

static bool IsUpper(char c)
{
    return c >= 'A' && c <= 'Z';
}

static bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

// The length of a field's text, or SIZE_MAX when a byte after it is not
// NUL.
static size_t TextLength(const char* field, size_t size)
{
    size_t n = 0;
    while (n < size && field[n] != '\0')
    {
        n++;
    }
    for (size_t i = n; i < size; i++)
    {
        if (field[i] != '\0')
        {
            return SIZE_MAX;
        }
    }
    return n;
}

static bool ValidLanguage(const char* language, size_t length)
{
    if (length < 2 || length > 8 || length == 4)
    {
        return false;
    }
    for (size_t i = 0; i < length; i++)
    {
        if (!IsLower(language[i]))
        {
            return false;
        }
    }
    return true;
}

static bool ValidScript(const char* script, size_t length)
{
    return length == 0 || (length == 4 && IsUpper(script[0]) && IsLower(script[1]) &&
                           IsLower(script[2]) && IsLower(script[3]));
}

static bool ValidRegion(const char* region, size_t length)
{
    return length == 0 || (length == 2 && IsUpper(region[0]) && IsUpper(region[1])) ||
           (length == 3 && IsDigit(region[0]) && IsDigit(region[1]) && IsDigit(region[2]));
}

static bool ValidTail(const char* tail, size_t length)
{
    size_t subtag = 0;
    for (size_t i = 0; i < length; i++)
    {
        if (tail[i] == '-')
        {
            if (subtag == 0)
            {
                return false;
            }
            subtag = 0;
        }
        else if ((IsLower(tail[i]) || IsDigit(tail[i])) && subtag < 8)
        {
            subtag++;
        }
        else
        {
            return false;
        }
    }
    return length == 0 || subtag > 0;
}

bool mlocLocaleValid(const mlocLocale* locale)
{
    if (locale == nullptr)
    {
        return false;
    }
    size_t language = TextLength(locale->language, sizeof(locale->language));
    size_t script = TextLength(locale->script, sizeof(locale->script));
    size_t region = TextLength(locale->region, sizeof(locale->region));
    size_t tail = TextLength(locale->tail, sizeof(locale->tail));
    return language != SIZE_MAX && script != SIZE_MAX && region != SIZE_MAX && tail != SIZE_MAX &&
           ValidLanguage(locale->language, language) && ValidScript(locale->script, script) &&
           ValidRegion(locale->region, region) && ValidTail(locale->tail, tail);
}
