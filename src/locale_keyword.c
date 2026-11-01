// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A -u keyword's value (locale_keyword.h).

#include "locale_keyword.h"

#include "invariant.h"

#include <string.h>

// The value of a -u key, or an empty one: the tail is canonical, so the
// u extension's keys follow its attributes, each with its value.
// The value given for a key the locale has not.
static const char s_absent[] = "";

// The length of a key's value that begins at next: its type subtags
// (three to eight characters) run to the next key or singleton
// ("islamic-civil").
static size_t ValueLength(const char* tail, size_t size, size_t next)
{
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
            return length;
        }
        length = from + part - next;
    }
}

size_t mlocFindKeyword(const mlocLocale* locale, const char* key, const char** valueOut)
{
    // A key of two letters, a constant of each caller: two bytes of it
    // are compared.
    MLOC_ASSERT(strlen(key) == 2);
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
            *valueOut = tail + next;
            return ValueLength(tail, size, next);
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

// A tag being rewritten: its text, the key left out and the keyword to
// add again (none when null), and where its subtags have brought it.
typedef struct Rewriting
{
    char text[192];
    size_t n;
    const char* key;
    const char* keyword;
    size_t keywordLength;
    bool good;
    bool inU;
    bool skipping;
    bool added;
    bool privateUse;
    // The text's length before the u singleton, and with it.
    size_t beforeU;
    size_t withU;
} Rewriting;

// A singleton: the u extension ends here, or private use begins without
// one, so the keyword is added if it is not yet, and a u extension left
// empty is left out.
static void BeginExtension(Rewriting* r, char singleton)
{
    if (!r->added && (r->inU || singleton == 'x'))
    {
        r->good = AddKeyword(r->text, sizeof r->text, &r->n, r->inU, r->keyword, r->keywordLength);
        r->added = true;
    }
    if (r->inU && r->n == r->withU)
    {
        r->n = r->beforeU;
    }
    r->inU = singleton == 'u';
    r->privateUse = singleton == 'x';
    r->skipping = false;
    r->beforeU = r->n;
    r->withU = r->n + (r->n > 0) + 1;
}

// Writes a locale with its -u key left out and, unless keyword is null,
// added again as keyword ("ca-gregory") at the end of the u extension (or
// as a new one before private use): parsing puts keys and extensions in
// order again. A u extension left empty is left out.
static bool Rewrite(mlocLocale* locale, const char* key, const char* keyword, size_t keywordLength)
{
    // A key of two letters, a constant of each caller: two bytes of it
    // are compared and copied.
    MLOC_ASSERT(strlen(key) == 2);
    Rewriting r = {.key = key, .keyword = keyword, .keywordLength = keywordLength};
    r.added = keyword == nullptr;
    r.good = AppendHead(locale, r.text, sizeof r.text, &r.n);
    const char* tail = locale->tail;
    size_t size = FieldLength(tail, sizeof(locale->tail));
    for (size_t at = 0; r.good && at < size;)
    {
        size_t k = 0;
        while (at + k < size && tail[at + k] != '-')
        {
            k++;
        }
        if (k == 1 && !r.privateUse)
        {
            BeginExtension(&r, tail[at]);
        }
        else if (r.inU && k == 2)
        {
            r.skipping = memcmp(tail + at, r.key, 2) == 0;
        }
        if (r.good && !r.skipping)
        {
            r.good = Append(r.text, sizeof r.text, &r.n, tail + at, k);
        }
        at += k + 1;
    }
    if (r.good && !r.added)
    {
        r.good = AddKeyword(r.text, sizeof r.text, &r.n, r.inU, keyword, keywordLength);
    }
    if (r.inU && r.n == r.withU)
    {
        r.n = r.beforeU;
    }
    mlocLocale made;
    if (!r.good || mlocParseLocale(r.text, r.n, &made).status != mloc_success)
    {
        return false;
    }
    *locale = made;
    return true;
}

bool mlocSetKeyword(mlocLocale* locale, const char* key, const char* value, size_t length)
{
    char keyword[48];
    if (length + 3 >= sizeof keyword)
    {
        return false;
    }
    memcpy(keyword, key, 2);
    keyword[2] = '-';
    memcpy(keyword + 3, value, length);
    return Rewrite(locale, key, keyword, length + 3);
}

bool mlocRemoveKeyword(mlocLocale* locale, const char* key)
{
    return Rewrite(locale, key, nullptr, 0);
}
