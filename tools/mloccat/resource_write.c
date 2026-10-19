// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message resource writer (resource_write.h).

#include "resource_write.h"

#include <stdint.h>

// The code point at text[*at], moving past it; the model's texts are
// UTF-8, an escaped NUL included.
static uint32_t Next(const ResText* t, size_t* at)
{
    const unsigned char* p = (const unsigned char*)t->bytes + *at;
    uint32_t c = p[0];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
    for (size_t i = 1; i <= extra && *at + i < t->length; i++)
    {
        c = c << 6 | (p[i] & 0x3F);
    }
    *at += extra + 1;
    return c;
}

static void PutCode(uint32_t c, FILE* out)
{
    char bytes[4];
    size_t n = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    static const unsigned char s_lead[] = {0, 0, 0xC0, 0xE0, 0xF0};
    for (size_t i = n; i-- > 1;)
    {
        bytes[i] = (char)(0x80 | (c & 0x3F));
        c >>= 6;
    }
    bytes[0] = (char)(n == 1 ? c : (s_lead[n] | c));
    fwrite(bytes, 1, n, out);
}

static void PutEscaped(uint32_t c, FILE* out)
{
    if (c < 0x100)
    {
        fprintf(out, "\\x%02X", (unsigned)c);
    }
    else if (c < 0x10000)
    {
        fprintf(out, "\\u%04X", (unsigned)c);
    }
    else
    {
        fprintf(out, "\\U%06X", (unsigned)c);
    }
}

static int IsIdSafe(uint32_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           (c >= 0xA1 && c <= 0x1FFF) || c == 0x200C || c == 0x200D ||
           (c >= 0x2030 && c <= 0x205E) || (c >= 0x2070 && c <= 0x2FEF) ||
           (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) ||
           (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

// What a value or comment may hold unescaped besides the backslash.
static int IsContent(uint32_t c)
{
    return c == ' ' || c == '\t' || (c >= 0x21 && c <= 0x7E && c != '\\') ||
           (c >= 0xA0 && c <= 0x2027) || (c >= 0x202A && c <= 0xD7FF) || c >= 0xE000;
}

// An id-part: identifier characters as they are, a first part's third
// leading hyphen escaped, anything else as a hex escape.
static void PutIdPart(const ResText* part, int first, FILE* out)
{
    size_t at = 0;
    size_t index = 0;
    while (at < part->length)
    {
        uint32_t c = Next(part, &at);
        int leadingThird =
            first && index == 2 && c == '-' && part->bytes[0] == '-' && part->bytes[1] == '-';
        if ((IsIdSafe(c) || c == '-') && !leadingThird)
        {
            PutCode(c, out);
        }
        else
        {
            PutEscaped(c, out);
        }
        index++;
    }
}

static void PutId(const ResId* id, FILE* out)
{
    for (size_t i = 0; i < id->count; i++)
    {
        fputs(i > 0 ? "." : "", out);
        PutIdPart(&id->parts[i], i == 0, out);
    }
}

// Whether a value's LF at at would make an empty first or last line,
// which reading drops: such an LF is written as \n.
static int EdgeNewline(const ResText* v, size_t at)
{
    size_t before = at;
    while (before > 0 && v->bytes[before - 1] == '\n')
    {
        before--;
    }
    size_t after = at;
    while (after < v->length && v->bytes[after] == '\n')
    {
        after++;
    }
    return before == 0 || after == v->length;
}

// Writes one character of a value: MF2's escapes as they are, a lone
// backslash and what the grammar does not allow escaped.
static void PutValueChar(const ResText* v, size_t* at, FILE* out)
{
    uint32_t c = Next(v, at);
    if (c == '\\')
    {
        char n = *at < v->length ? v->bytes[*at] : 0;
        if (n == '\\' || n == '{' || n == '|' || n == '}')
        {
            fputc('\\', out);
            fputc(n, out);
            (*at)++;
            return;
        }
        PutEscaped(c, out);
        return;
    }
    if (IsContent(c))
    {
        PutCode(c, out);
    }
    else if (c == '\r')
    {
        fputs("\\r", out);
    }
    else
    {
        PutEscaped(c, out);
    }
}

// Writes a value: each LF a line break with a two-space indent, but an
// LF that would leave an empty first or last line as \n; whitespace at a
// line's start or end escaped, since reading drops it.
static void PutValue(const ResText* v, FILE* out)
{
    size_t at = 0;
    int lineStart = 1;
    while (at < v->length)
    {
        char c = v->bytes[at];
        if (c == '\n' && !EdgeNewline(v, at))
        {
            fputs("\n  ", out);
            at++;
            lineStart = 1;
            continue;
        }
        if (c == '\n')
        {
            fputs("\\n", out);
            at++;
            lineStart = 0;
            continue;
        }
        size_t next = at + 1;
        int lineEnd = next >= v->length || (v->bytes[next] == '\n' && !EdgeNewline(v, next));
        if ((c == ' ' || c == '\t') && (lineStart || lineEnd))
        {
            fputs(c == ' ' ? "\\ " : "\\t", out);
            at++;
        }
        else
        {
            PutValueChar(v, &at, out);
        }
        lineStart = 0;
    }
}

static void PutComment(const ResText* comment, FILE* out)
{
    if (comment->length == 0)
    {
        return;
    }
    fputs("# ", out);
    for (size_t i = 0; i < comment->length; i++)
    {
        if (comment->bytes[i] == '\n')
        {
            fputs(i + 1 < comment->length && comment->bytes[i + 1] != '\n' ? "\n# " : "\n#", out);
        }
        else
        {
            fputc(comment->bytes[i], out);
        }
    }
    fputc('\n', out);
}

static void PutMeta(const ResMeta* meta, size_t count, FILE* out)
{
    for (size_t i = 0; i < count; i++)
    {
        fputc('@', out);
        PutIdPart(&meta[i].key, 0, out);
        if (meta[i].value.length > 0)
        {
            fputc(' ', out);
            PutValue(&meta[i].value, out);
        }
        fputc('\n', out);
    }
}

static void PutItem(const ResItem* item, FILE* out)
{
    if (item->kind == resComment)
    {
        // An empty line after it keeps it from the next entry; an empty
        // one is a lone #.
        if (item->comment.length == 0)
        {
            fputs("#\n", out);
        }
        PutComment(&item->comment, out);
        fputc('\n', out);
        return;
    }
    PutComment(&item->comment, out);
    PutMeta(item->meta, item->metaCount, out);
    PutId(&item->id, out);
    fputs(" = ", out);
    PutValue(&item->value, out);
    fputc('\n', out);
}

void ResourceWrite(const Resource* resource, FILE* out)
{
    if (resource->hasFrontmatter)
    {
        PutComment(&resource->comment, out);
        PutMeta(resource->meta, resource->metaCount, out);
        fputs("---\n", out);
    }
    for (size_t s = 0; s < resource->sectionCount; s++)
    {
        const ResSection* section = &resource->sections[s];
        if (s > 0)
        {
            fputc('\n', out);
            PutComment(&section->comment, out);
            PutMeta(section->meta, section->metaCount, out);
            fputc('[', out);
            PutId(&section->id, out);
            fputs("]\n", out);
        }
        for (size_t i = 0; i < section->itemCount; i++)
        {
            fputc('\n', out);
            PutItem(&section->items[i], out);
        }
    }
}
