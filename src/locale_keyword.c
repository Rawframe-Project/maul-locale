// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A -u keyword's value (locale_keyword.h).

#include "locale_keyword.h"

#include <string.h>

// The value of a -u key, or an empty one: the tail is canonical, so the
// u extension's keys follow its attributes, each with its value.
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
    *valueOut = "";
    return 0;
}
