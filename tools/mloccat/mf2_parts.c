// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An MF2 message split (mf2_parts.h).

#include "mf2_parts.h"

#include "catalog_set.h"

#include "maul-locale/message.h"

#include <stdlib.h>

typedef struct Parts
{
    Mf2Part* items;
    size_t count;
} Parts;

static void Add(Parts* p, Mf2PartKind kind, size_t start, size_t length)
{
    // Text and syntax next to their own kind join, so a run is one part.
    if (p->count > 0 && (kind == mf2Text || kind == mf2Syntax) &&
        p->items[p->count - 1].kind == kind &&
        p->items[p->count - 1].start + p->items[p->count - 1].length == start)
    {
        p->items[p->count - 1].length += length;
        return;
    }
    if (length == 0 && kind != mf2PatternStart && kind != mf2PatternEnd)
    {
        return;
    }
    p->items = CatalogGrow(p->items, p->count, sizeof(Mf2Part));
    p->items[p->count++] = (Mf2Part){kind, start, length};
}

size_t Mf2SpaceAt(const char* text, size_t length, size_t at)
{
    const unsigned char* u = (const unsigned char*)text + at;
    size_t left = length - at;
    if (left >= 1 && (u[0] == ' ' || u[0] == '\t' || u[0] == '\r' || u[0] == '\n'))
    {
        return 1;
    }
    if (left >= 2 && u[0] == 0xD8 && u[1] == 0x9C)
    {
        return 2;
    }
    if (left >= 3 && ((u[0] == 0xE3 && u[1] == 0x80 && u[2] == 0x80) ||
                      (u[0] == 0xE2 && u[1] == 0x80 && (u[2] == 0x8E || u[2] == 0x8F)) ||
                      (u[0] == 0xE2 && u[1] == 0x81 && u[2] >= 0xA6 && u[2] <= 0xA9)))
    {
        return 3;
    }
    return 0;
}

// The index after the '}' that closes the '{' at at: quoted literals and
// escapes inside are skipped.
static size_t SkipBraces(const char* t, size_t n, size_t at)
{
    int quoted = 0;
    for (size_t i = at + 1; i < n; i++)
    {
        if (t[i] == '\\')
        {
            i++;
        }
        else if (t[i] == '|')
        {
            quoted = !quoted;
        }
        else if (!quoted && t[i] == '}')
        {
            return i + 1;
        }
    }
    return n;
}

// A placeholder's kind by its first character after the brace (and any
// space): markup opens with '#' or closes with '/', and stands alone when
// it ends with "/}".
static Mf2PartKind ExpressionKind(const char* t, size_t at, size_t end)
{
    size_t i = at + 1;
    while (i < end && Mf2SpaceAt(t, end, i) > 0)
    {
        i += Mf2SpaceAt(t, end, i);
    }
    if (i < end && t[i] == '/')
    {
        return mf2MarkupClose;
    }
    if (i < end && t[i] == '#')
    {
        return end - at >= 3 && t[end - 2] == '/' ? mf2MarkupStandalone : mf2MarkupOpen;
    }
    return mf2Placeholder;
}

// A pattern from at: to the text's end, or to the "}}" closing a quoted
// pattern; returns where it ends.
static size_t Pattern(const char* t, size_t n, size_t at, int quoted, Parts* out)
{
    Add(out, mf2PatternStart, at, 0);
    size_t i = at;
    while (i < n && !(quoted && t[i] == '}'))
    {
        if (t[i] == '{')
        {
            size_t close = SkipBraces(t, n, i);
            Add(out, ExpressionKind(t, i, close), i, close - i);
            i = close;
            continue;
        }
        size_t step = t[i] == '\\' && i + 1 < n ? 2 : 1;
        Add(out, mf2Text, i, step);
        i += step;
    }
    Add(out, mf2PatternEnd, i, 0);
    return i;
}

// A complex message from at: each quoted pattern split, the rest syntax.
static void Complex(const char* t, size_t n, size_t at, Parts* out)
{
    size_t i = at;
    while (i < n)
    {
        if (t[i] == '{' && i + 1 < n && t[i + 1] == '{')
        {
            Add(out, mf2Syntax, i, 2);
            i = Pattern(t, n, i + 2, 1, out);
            continue;
        }
        size_t next = i + 1;
        if (t[i] == '{')
        {
            next = SkipBraces(t, n, i);
        }
        else if (t[i] == '|')
        {
            while (next < n && t[next] != '|')
            {
                next += t[next] == '\\' ? 2 : 1;
            }
            next = next < n ? next + 1 : n;
        }
        Add(out, mf2Syntax, i, next - i);
        i = next;
    }
}

int Mf2Split(const char* text, size_t length, Mf2Part** partsOut, size_t* countOut)
{
    size_t size = 0;
    mlocTextResult r = mlocCompileMessage(text, length, nullptr, 0, &size, nullptr);
    if (r.status != mloc_success && r.status != mloc_errorCapacity)
    {
        return 0;
    }
    Parts parts = {0};
    size_t start = 0;
    for (size_t step = Mf2SpaceAt(text, length, 0); step > 0;
         step = Mf2SpaceAt(text, length, start))
    {
        start += step;
    }
    Add(&parts, mf2Syntax, 0, start);
    if (start < length && (text[start] == '.' ||
                           (text[start] == '{' && start + 1 < length && text[start + 1] == '{')))
    {
        Complex(text, length, start, &parts);
    }
    else
    {
        (void)Pattern(text, length, start, 0, &parts);
    }
    *partsOut = parts.items;
    *countOut = parts.count;
    return 1;
}
