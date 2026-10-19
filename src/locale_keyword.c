// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A -u keyword's value (locale_keyword.h).

#include "locale_keyword.h"

#include <string.h>

// The value of a -u key, or an empty one: the tail is canonical, so the
// u extension's keys follow its attributes, each with its value.
// The value given for a key the locale has not.
static const char s_absent[] = "";

size_t mlocFindKeyword(const mlocLocale* locale, const char* key, const char** valueOut)
{
    const char* tail = locale->tail;
    size_t size = sizeof(locale->tail);
    bool inU = false;
    for (size_t at = 0; at < size && tail[at] != '\0';)
    {
        size_t n = 0;
        while (at + n < size && tail[at + n] != '\0' && tail[at + n] != '-')
        {
            n++;
        }
        size_t next = at + n + (at + n < size && tail[at + n] == '-');
        if (n == 1 && tail[at] == 'x')
        {
            // Private use runs to the end; its subtags are no keys.
            break;
        }
        if (n == 1)
        {
            inU = tail[at] == 'u';
        }
        else if (inU && n == 2 && memcmp(tail + at, key, 2) == 0)
        {
            // The value's type subtags (three to eight characters) run to
            // the next key or singleton: "islamic-civil".
            size_t length = 0;
            for (;;)
            {
                size_t part = 0;
                size_t from = next + length + (length > 0);
                while (from + part < size && tail[from + part] != '\0' && tail[from + part] != '-')
                {
                    part++;
                }
                if (part < 3)
                {
                    break;
                }
                length = from + part - next;
            }
            *valueOut = tail + next;
            return length;
        }
        at = next;
    }
    *valueOut = s_absent;
    return 0;
}

bool mlocHasKeyword(const mlocLocale* locale, const char* key)
{
    const char* value = nullptr;
    return mlocFindKeyword(locale, key, &value) > 0 || value != s_absent;
}

// The length of a field up to its first NUL byte or its size (strnlen
// is not standard C).
static size_t FieldLength(const char* field, size_t size)
{
    const char* end = memchr(field, '\0', size);
    return end == nullptr ? size : (size_t)(end - field);
}

// Appends a hyphen (unless first) and text to a tag being built; false
// when it does not fit.
static bool Append(char* text, size_t capacity, size_t* length, const char* part, size_t size)
{
    size_t hyphen = *length > 0;
    if (*length + hyphen + size >= capacity)
    {
        return false;
    }
    text[*length] = '-';
    memcpy(text + *length + hyphen, part, size);
    *length += hyphen + size;
    return true;
}

// Appends a locale's language, script and region.
static bool AppendHead(const mlocLocale* locale, char* text, size_t capacity, size_t* length)
{
    return Append(text, capacity, length, locale->language, FieldLength(locale->language, 8)) &&
           (locale->script[0] == '\0' || Append(text, capacity, length, locale->script, 4)) &&
           (locale->region[0] == '\0' ||
            Append(text, capacity, length, locale->region, FieldLength(locale->region, 4)));
}

// Appends a key and its value at the end of the u extension, or as a new
// u extension.
static bool AddKeyword(char* text, size_t capacity, size_t* length, bool inU, const char* keyword,
                       size_t keywordLength)
{
    return (inU || Append(text, capacity, length, "u", 1)) &&
           Append(text, capacity, length, keyword, keywordLength);
}

bool mlocSetKeyword(mlocLocale* locale, const char* key, const char* value, size_t length)
{
    // The tag as text, the key's old subtags left out and the key added at
    // the end of the u extension (or as a new one before private use):
    // parsing puts keys and extensions in order again.
    char text[192];
    size_t n = 0;
    char keyword[48];
    if (length + 3 >= sizeof keyword)
    {
        return false;
    }
    memcpy(keyword, key, 2);
    keyword[2] = '-';
    memcpy(keyword + 3, value, length);
    size_t keywordLength = length + 3;
    bool good = AppendHead(locale, text, sizeof text, &n);
    const char* tail = locale->tail;
    size_t size = FieldLength(tail, sizeof(locale->tail));
    bool inU = false;
    bool skipping = false;
    bool added = false;
    bool privateUse = false;
    for (size_t at = 0; good && at < size;)
    {
        size_t k = 0;
        while (at + k < size && tail[at + k] != '-')
        {
            k++;
        }
        bool singleton = k == 1 && !privateUse;
        if (singleton && !added && (inU || tail[at] == 'x'))
        {
            // The u extension ends here, or private use begins without one.
            good = AddKeyword(text, sizeof text, &n, inU, keyword, keywordLength);
            added = true;
        }
        if (singleton)
        {
            inU = tail[at] == 'u';
            privateUse = tail[at] == 'x';
            skipping = false;
        }
        else if (inU && k == 2)
        {
            skipping = memcmp(tail + at, key, 2) == 0;
        }
        if (good && !skipping)
        {
            good = Append(text, sizeof text, &n, tail + at, k);
        }
        at += k + 1;
    }
    if (good && !added)
    {
        good = AddKeyword(text, sizeof text, &n, inU, keyword, keywordLength);
    }
    mlocLocale made;
    if (!good || mlocParseLocale(text, n, &made).status != mloc_success)
    {
        return false;
    }
    *locale = made;
    return true;
}
