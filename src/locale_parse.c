// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Parsing a locale id into canonical syntax, and writing it back as a
// BCP 47 tag (record mloc-0015, UTS #35 Part 1). The text is split into
// subtags, checked against the grammar while each extension lists its
// subtags in canonical order, and the extensions are then written by
// their singleton.

#include "maul-locale/locale.h"

#include <string.h>

#define MAX_SUBTAGS    64
#define MAX_EXTENSIONS 32

typedef struct Subtag
{
    uint32_t start;
    uint32_t length;
} Subtag;

// An extension: its singleton and its subtags' indices in output order,
// a range of Parser.order.
typedef struct Extension
{
    char singleton;
    int first;
    int count;
} Extension;

typedef struct Parser
{
    const char* text;
    int count;
    Subtag subtags[MAX_SUBTAGS];
    int orderCount;
    int order[MAX_SUBTAGS];
    int extensionCount;
    Extension extensions[MAX_EXTENSIONS];
} Parser;

static mlocTextResult Result(mlocResult status, size_t offset)
{
    return (mlocTextResult){status, offset};
}

static bool IsAlpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool IsDigit(char c)
{
    return c >= '0' && c <= '9';
}

static char Lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

static char Upper(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
}

// Splits the text into subtags of 1 to 8 letters and digits.
static mlocTextResult Split(Parser* p, const char* text, size_t length)
{
    p->text = text;
    p->count = 0;
    size_t start = 0;
    for (size_t i = 0; i <= length; i++)
    {
        if (i < length && text[i] != '-' && text[i] != '_')
        {
            if (!IsAlpha(text[i]) && !IsDigit(text[i]))
            {
                return Result(mloc_errorSyntax, i);
            }
            continue;
        }
        if (i == start || i - start > 8)
        {
            return Result(mloc_errorSyntax, start);
        }
        if (p->count == MAX_SUBTAGS)
        {
            return Result(mloc_errorCapacity, start);
        }
        p->subtags[p->count++] = (Subtag){(uint32_t)start, (uint32_t)(i - start)};
        start = i + 1;
    }
    return Result(mloc_success, length);
}

static bool AllAlpha(const Parser* p, int s)
{
    for (uint32_t k = 0; k < p->subtags[s].length; k++)
    {
        if (!IsAlpha(p->text[p->subtags[s].start + k]))
        {
            return false;
        }
    }
    return true;
}

static bool AllDigit(const Parser* p, int s)
{
    for (uint32_t k = 0; k < p->subtags[s].length; k++)
    {
        if (!IsDigit(p->text[p->subtags[s].start + k]))
        {
            return false;
        }
    }
    return true;
}

static char At(const Parser* p, int s, uint32_t k)
{
    return p->text[p->subtags[s].start + k];
}

static uint32_t Length(const Parser* p, int s)
{
    return p->subtags[s].length;
}

static bool IsLanguage(const Parser* p, int s)
{
    uint32_t n = Length(p, s);
    return AllAlpha(p, s) && (n == 2 || n == 3 || n >= 5);
}

static bool IsScript(const Parser* p, int s)
{
    return Length(p, s) == 4 && AllAlpha(p, s);
}

static bool IsRegion(const Parser* p, int s)
{
    return (Length(p, s) == 2 && AllAlpha(p, s)) || (Length(p, s) == 3 && AllDigit(p, s));
}

static bool IsVariant(const Parser* p, int s)
{
    return Length(p, s) >= 5 || (Length(p, s) == 4 && IsDigit(At(p, s, 0)));
}

// Whether subtag s, lowercased, is the given lowercase word.
static bool Is(const Parser* p, int s, const char* word)
{
    if (Length(p, s) != strlen(word))
    {
        return false;
    }
    for (uint32_t k = 0; k < Length(p, s); k++)
    {
        if (Lower(At(p, s, k)) != word[k])
        {
            return false;
        }
    }
    return true;
}

// Orders subtags by their lowercase letters, a prefix first.
static int Compare(const Parser* p, int a, int b)
{
    uint32_t n = Length(p, a) < Length(p, b) ? Length(p, a) : Length(p, b);
    for (uint32_t k = 0; k < n; k++)
    {
        char x = Lower(At(p, a, k));
        char y = Lower(At(p, b, k));
        if (x != y)
        {
            return x < y ? -1 : 1;
        }
    }
    return Length(p, a) < Length(p, b) ? -1 : Length(p, a) > Length(p, b);
}

// Sorts groups of subtags by their first subtag: groups[i] is the index
// in Parser.order where group i starts, and each runs to the next.
// Returns the index of a subtag equal to an earlier one, or -1.
static int SortGroups(Parser* p, int first, int groupCount, const int* starts, int end)
{
    int sorted[MAX_SUBTAGS];
    int lengths[MAX_SUBTAGS];
    int keys[MAX_SUBTAGS];
    for (int g = 0; g < groupCount; g++)
    {
        int next = g + 1 < groupCount ? starts[g + 1] : end;
        int key = p->order[starts[g]];
        int at = g;
        while (at > 0 && Compare(p, keys[at - 1], key) > 0)
        {
            keys[at] = keys[at - 1];
            sorted[at] = sorted[at - 1];
            lengths[at] = lengths[at - 1];
            at--;
        }
        if (at > 0 && Compare(p, keys[at - 1], key) == 0)
        {
            return key;
        }
        keys[at] = key;
        sorted[at] = starts[g];
        lengths[at] = next - starts[g];
    }
    int copy[MAX_SUBTAGS];
    memcpy(copy, p->order + first, (size_t)(end - first) * sizeof(int));
    int out = first;
    for (int g = 0; g < groupCount; g++)
    {
        memcpy(p->order + out, copy + (sorted[g] - first), (size_t)lengths[g] * sizeof(int));
        out += lengths[g];
    }
    return -1;
}

// Variants from subtag *s on, in order and without duplicates.
static mlocTextResult Variants(Parser* p, int* s)
{
    int first = p->orderCount;
    int starts[MAX_SUBTAGS];
    int count = 0;
    while (*s < p->count && IsVariant(p, *s))
    {
        starts[count++] = p->orderCount;
        p->order[p->orderCount++] = (*s)++;
    }
    int duplicate = SortGroups(p, first, count, starts, p->orderCount);
    if (duplicate >= 0)
    {
        return Result(mloc_errorSyntax, p->subtags[duplicate].start);
    }
    return Result(mloc_success, 0);
}

static bool EndsExtension(const Parser* p, int s)
{
    return s >= p->count || Length(p, s) == 1;
}

// A "u" extension after its singleton: attributes, then keys each with
// their values; attributes and keys in order, a value "true" removed.
static mlocTextResult UnicodeExtension(Parser* p, int* s, size_t singletonOffset)
{
    int first = p->orderCount;
    int starts[MAX_SUBTAGS];
    int attributes = 0;
    while (!EndsExtension(p, *s) && Length(p, *s) >= 3)
    {
        starts[attributes++] = p->orderCount;
        p->order[p->orderCount++] = (*s)++;
    }
    int duplicate = SortGroups(p, first, attributes, starts, p->orderCount);
    int keysFirst = p->orderCount;
    int keys = 0;
    while (duplicate < 0 && !EndsExtension(p, *s))
    {
        if (Length(p, *s) != 2 || !IsAlpha(At(p, *s, 1)))
        {
            return Result(mloc_errorSyntax, p->subtags[*s].start);
        }
        starts[keys++] = p->orderCount;
        p->order[p->orderCount++] = (*s)++;
        int values = *s;
        while (!EndsExtension(p, *s) && Length(p, *s) >= 3)
        {
            p->order[p->orderCount++] = (*s)++;
        }
        if (*s == values + 1 && Is(p, values, "true"))
        {
            p->orderCount--;
        }
    }
    if (duplicate < 0)
    {
        duplicate = SortGroups(p, keysFirst, keys, starts, p->orderCount);
    }
    if (duplicate >= 0)
    {
        return Result(mloc_errorSyntax, p->subtags[duplicate].start);
    }
    if (attributes + keys == 0)
    {
        return Result(mloc_errorSyntax, singletonOffset);
    }
    return Result(mloc_success, 0);
}

// A "t" extension after its singleton: an optional language, then
// fields each with their values, the fields in order.
static mlocTextResult TransformedExtension(Parser* p, int* s, size_t singletonOffset)
{
    int start = *s;
    if (!EndsExtension(p, *s) && IsLanguage(p, *s))
    {
        p->order[p->orderCount++] = (*s)++;
        if (!EndsExtension(p, *s) && IsScript(p, *s))
        {
            p->order[p->orderCount++] = (*s)++;
        }
        if (!EndsExtension(p, *s) && IsRegion(p, *s))
        {
            p->order[p->orderCount++] = (*s)++;
        }
        mlocTextResult variants = Variants(p, s);
        if (variants.status != mloc_success)
        {
            return variants;
        }
    }
    int fieldsFirst = p->orderCount;
    int starts[MAX_SUBTAGS];
    int fields = 0;
    while (!EndsExtension(p, *s))
    {
        int key = *s;
        if (Length(p, key) != 2 || !IsAlpha(At(p, key, 0)) || !IsDigit(At(p, key, 1)) ||
            EndsExtension(p, key + 1) || Length(p, key + 1) < 3)
        {
            return Result(mloc_errorSyntax, p->subtags[key].start);
        }
        starts[fields++] = p->orderCount;
        p->order[p->orderCount++] = (*s)++;
        while (!EndsExtension(p, *s) && Length(p, *s) >= 3)
        {
            p->order[p->orderCount++] = (*s)++;
        }
    }
    int duplicate = SortGroups(p, fieldsFirst, fields, starts, p->orderCount);
    if (duplicate >= 0)
    {
        return Result(mloc_errorSyntax, p->subtags[duplicate].start);
    }
    if (*s == start)
    {
        return Result(mloc_errorSyntax, singletonOffset);
    }
    return Result(mloc_success, 0);
}

// Any other extension, or private use (singleton "x", which takes every
// subtag after it).
static mlocTextResult OtherExtension(Parser* p, int* s, char singleton, size_t singletonOffset)
{
    int start = *s;
    while (*s < p->count && (singleton == 'x' || Length(p, *s) > 1))
    {
        p->order[p->orderCount++] = (*s)++;
    }
    if (*s == start)
    {
        return Result(mloc_errorSyntax, singletonOffset);
    }
    return Result(mloc_success, 0);
}

static mlocTextResult Extensions(Parser* p, int s)
{
    while (s < p->count)
    {
        size_t offset = p->subtags[s].start;
        char singleton = Lower(At(p, s, 0));
        if (Length(p, s) != 1)
        {
            return Result(mloc_errorSyntax, offset);
        }
        for (int e = 0; e < p->extensionCount; e++)
        {
            if (p->extensions[e].singleton == singleton)
            {
                return Result(mloc_errorSyntax, offset);
            }
        }
        s++;
        int first = p->orderCount;
        mlocTextResult result = singleton == 'u'   ? UnicodeExtension(p, &s, offset)
                                : singleton == 't' ? TransformedExtension(p, &s, offset)
                                                   : OtherExtension(p, &s, singleton, offset);
        if (result.status != mloc_success)
        {
            return result;
        }
        p->extensions[p->extensionCount++] = (Extension){singleton, first, p->orderCount - first};
    }
    return Result(mloc_success, 0);
}

// The order extensions are written in: by singleton, private use last.
static int Rank(char singleton)
{
    return singleton == 'x' ? 128 : singleton;
}

static void SortExtensions(Parser* p)
{
    for (int i = 1; i < p->extensionCount; i++)
    {
        Extension e = p->extensions[i];
        int at = i;
        while (at > 0 && Rank(p->extensions[at - 1].singleton) > Rank(e.singleton))
        {
            p->extensions[at] = p->extensions[at - 1];
            at--;
        }
        p->extensions[at] = e;
    }
}

typedef struct Writer
{
    char* out;
    size_t capacity;
    size_t length;
} Writer;

static bool Put(Writer* w, char c)
{
    if (w->length == w->capacity)
    {
        return false;
    }
    w->out[w->length++] = c;
    return true;
}

static bool PutSubtag(Writer* w, const Parser* p, int s)
{
    if (w->length > 0 && !Put(w, '-'))
    {
        return false;
    }
    for (uint32_t k = 0; k < Length(p, s); k++)
    {
        if (!Put(w, Lower(At(p, s, k))))
        {
            return false;
        }
    }
    return true;
}

// Writes the variants (order[0, variantCount)) and the extensions.
static bool WriteTail(Parser* p, int variantCount, char* tail, size_t capacity)
{
    Writer w = {tail, capacity, 0};
    for (int i = 0; i < variantCount; i++)
    {
        if (!PutSubtag(&w, p, p->order[i]))
        {
            return false;
        }
    }
    SortExtensions(p);
    for (int e = 0; e < p->extensionCount; e++)
    {
        const Extension* x = &p->extensions[e];
        if ((w.length > 0 && !Put(&w, '-')) || !Put(&w, x->singleton))
        {
            return false;
        }
        for (int i = 0; i < x->count; i++)
        {
            if (!PutSubtag(&w, p, p->order[x->first + i]))
            {
                return false;
            }
        }
    }
    return true;
}

// The language, script and region at the start; returns the subtag
// after them, or -1.
static int LanguageId(const Parser* p, mlocLocale* locale)
{
    int s = 0;
    if (Is(p, 0, "root"))
    {
        memcpy(locale->language, "und", 3);
        return 1;
    }
    if (!IsLanguage(p, 0))
    {
        return -1;
    }
    for (uint32_t k = 0; k < Length(p, 0); k++)
    {
        locale->language[k] = Lower(At(p, 0, k));
    }
    s = 1;
    if (s < p->count && IsScript(p, s))
    {
        for (uint32_t k = 0; k < 4; k++)
        {
            locale->script[k] = k == 0 ? Upper(At(p, s, k)) : Lower(At(p, s, k));
        }
        s++;
    }
    if (s < p->count && IsRegion(p, s))
    {
        for (uint32_t k = 0; k < Length(p, s); k++)
        {
            locale->region[k] = Upper(At(p, s, k));
        }
        s++;
    }
    return s;
}

mlocTextResult mlocParseLocale(const char* text, size_t length, mlocLocale* localeOut)
{
    if ((text == nullptr && length > 0) || localeOut == nullptr)
    {
        return Result(mloc_errorInvalid, 0);
    }
    Parser p;
    mlocTextResult result = Split(&p, text, length);
    if (result.status != mloc_success)
    {
        return result;
    }
    mlocLocale locale = {0};
    int s = LanguageId(&p, &locale);
    if (s < 0)
    {
        return Result(mloc_errorSyntax, 0);
    }
    p.orderCount = 0;
    p.extensionCount = 0;
    result = Variants(&p, &s);
    int variantCount = p.orderCount;
    if (result.status == mloc_success)
    {
        result = Extensions(&p, s);
    }
    if (result.status != mloc_success)
    {
        return result;
    }
    if (!WriteTail(&p, variantCount, locale.tail, sizeof(locale.tail)))
    {
        return Result(mloc_errorCapacity, length);
    }
    *localeOut = locale;
    return Result(mloc_success, length);
}

// The length of a NUL-padded field.
static size_t FieldLength(const char* field, size_t size)
{
    size_t n = 0;
    while (n < size && field[n] != '\0')
    {
        n++;
    }
    return n;
}

mlocResult mlocFormatLocale(const mlocLocale* locale, mlocOutput* output)
{
    if (locale == nullptr || output == nullptr || (output->buffer == nullptr && output->capacity))
    {
        return mloc_errorInvalid;
    }
    const char* fields[4] = {locale->language, locale->script, locale->region, locale->tail};
    size_t lengths[4] = {
        FieldLength(locale->language, sizeof(locale->language)),
        FieldLength(locale->script, sizeof(locale->script)),
        FieldLength(locale->region, sizeof(locale->region)),
        FieldLength(locale->tail, sizeof(locale->tail)),
    };
    size_t needed = 0;
    for (int i = 0; i < 4; i++)
    {
        needed += lengths[i] + (i > 0 && lengths[i] > 0);
    }
    output->length = needed;
    if (needed > output->capacity)
    {
        return mloc_errorCapacity;
    }
    size_t at = 0;
    for (int i = 0; i < 4; i++)
    {
        if (lengths[i] == 0)
        {
            continue;
        }
        if (i > 0)
        {
            output->buffer[at++] = '-';
        }
        memcpy(output->buffer + at, fields[i], lengths[i]);
        at += lengths[i];
    }
    return mloc_success;
}
