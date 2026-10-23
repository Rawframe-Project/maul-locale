// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The library's fallback chains against CLDR's inheritance, for every
// locale file of the pinned release: reads the lines of `mlocgen chains`
// ("component<TAB>id<TAB>keys") and walks mlocGetParentLocale from the id
// on the compiled-in data, each parent written as a key. CI runs it.
//
// usage: mlocgen chains <common> < ids | conform_parents

#include "maul-locale/locale.h"

#include <stdio.h>
#include <string.h>

// Writes a locale as a key: "_" between subtags, variants in uppercase,
// "root" for und alone. Returns the length.
static size_t Key(const mlocLocale* locale, char* out, size_t capacity)
{
    mlocOutput output = {.buffer = out, .capacity = capacity};
    if (mlocFormatLocale(locale, &output) != mloc_success)
    {
        return 0;
    }
    if (output.length == 3 && memcmp(out, "und", 3) == 0)
    {
        memcpy(out, "root", 4);
        return 4;
    }
    bool variant = false;
    size_t subtag = 0;
    for (size_t i = 0; i < output.length; i++)
    {
        if (out[i] == '-')
        {
            out[i] = '_';
            subtag++;
            size_t end = i + 1;
            while (end < output.length && out[end] != '-')
            {
                end++;
            }
            size_t length = end - i - 1;
            variant = length >= 5 || (length == 4 && out[i + 1] >= '0' && out[i + 1] <= '9');
        }
        else if (variant && out[i] >= 'a' && out[i] <= 'z')
        {
            out[i] = (char)(out[i] - 32);
        }
    }
    (void)subtag;
    return output.length;
}

static bool CheckLine(const mlocDataSet* data, char* line)
{
    char* first = strchr(line, '\t');
    char* second = first == nullptr ? nullptr : strchr(first + 1, '\t');
    if (second == nullptr)
    {
        return false;
    }
    *first = '\0';
    *second = '\0';
    mlocComponent component =
        strcmp(line, "main") == 0 ? mloc_componentMain : mloc_componentCollation;
    const char* id = first + 1;
    char* expected = second + 1;
    expected[strcspn(expected, "\r\n")] = '\0';
    mlocLocale locale;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success)
    {
        return false;
    }
    char chain[1024] = "";
    size_t at = 0;
    for (int depth = 0; depth < 16; depth++)
    {
        mlocLocale parent;
        if (mlocGetParentLocale(data, component, &locale, &parent) != mloc_success)
        {
            break;
        }
        char key[200];
        size_t length = Key(&parent, key, sizeof(key));
        if (at + length + 2 >= sizeof(chain))
        {
            return false;
        }
        if (at > 0)
        {
            chain[at++] = ' ';
        }
        memcpy(chain + at, key, length);
        at += length;
        chain[at] = '\0';
        locale = parent;
    }
    if (strcmp(chain, expected) != 0)
    {
        fprintf(stderr, "differs: %s %s: CLDR \"%s\", library \"%s\"\n", line, id, expected, chain);
        return false;
    }
    return true;
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    char line[2048];
    int passed = 0;
    int failed = 0;
    while (fgets(line, sizeof(line), stdin) != nullptr)
    {
        if (CheckLine(&data, line))
        {
            passed++;
        }
        else
        {
            failed++;
        }
    }
    printf("fallback: %d of %d chains agree with CLDR\n", passed, passed + failed);
    return failed == 0 && passed > 0 ? 0 : 1;
}
