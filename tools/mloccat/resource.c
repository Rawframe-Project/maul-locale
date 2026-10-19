// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The message resource reader (resource.h): a character reader that
// follows resource.abnf rule by rule, then attaches comments and
// metadata as its notes say.

#include "resource.h"

#include "maul-unicode/encoding.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A growing text being built.
typedef struct Buf
{
    char* bytes;
    size_t length;
    size_t capacity;
} Buf;

typedef struct Reader
{
    const unsigned char* s;
    size_t n;
    size_t at;
    size_t line;
    size_t lineStart;
    ResError* error;
    Resource* out;
    // The comment and metadata waiting for what they describe.
    Buf comment;
    int hasComment;
    ResMeta* meta;
    size_t metaCount;
    // Whether a section head or an entry has been read.
    int sawBody;
} Reader;

[[noreturn]] static void OutOfMemory(void)
{
    fputs("mloccat: out of memory\n", stderr);
    exit(1);
}

// Grows an array of count items of size bytes to hold one more.
static void* GrowArray(void* items, size_t count, size_t size)
{
    void* grown = realloc(items, (count + 1) * size);
    if (grown == nullptr)
    {
        OutOfMemory();
    }
    return grown;
}

static void BufAppend(Buf* b, const void* bytes, size_t length)
{
    if (b->bytes == nullptr || b->length + length + 1 > b->capacity)
    {
        size_t capacity = b->capacity == 0 ? 32 : b->capacity;
        while (capacity < b->length + length + 1)
        {
            capacity *= 2;
        }
        char* grown = realloc(b->bytes, capacity);
        if (grown == nullptr)
        {
            OutOfMemory();
        }
        b->bytes = grown;
        b->capacity = capacity;
    }
    if (length > 0 && bytes != nullptr)
    {
        memcpy(b->bytes + b->length, bytes, length);
    }
    b->length += length;
    b->bytes[b->length] = '\0';
}

static void BufAppendCode(Buf* b, uint32_t c)
{
    char bytes[4];
    size_t used = 0;
    (void)muniEncodeUtf8(c, bytes, &used);
    BufAppend(b, bytes, used);
}

// Hands a built text over, leaving the buffer empty.
static ResText BufTake(Buf* b)
{
    ResText t = {b->bytes, b->length};
    if (t.bytes == nullptr)
    {
        t.bytes = calloc(1, 1);
        if (t.bytes == nullptr)
        {
            OutOfMemory();
        }
    }
    *b = (Buf){0};
    return t;
}

static int Fail(Reader* r, const char* reason)
{
    if (r->error->reason == nullptr)
    {
        r->error->line = r->line;
        r->error->column = r->at - r->lineStart + 1;
        r->error->reason = reason;
    }
    return 0;
}

// The code point at the reader, and its length in bytes; the text is
// valid UTF-8, so it decodes.
static uint32_t Peek(const Reader* r, size_t* lengthOut)
{
    const unsigned char* p = r->s + r->at;
    uint32_t c = p[0];
    size_t extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
    for (size_t i = 1; i <= extra; i++)
    {
        c = c << 6 | (p[i] & 0x3F);
    }
    *lengthOut = extra + 1;
    return c;
}

static int AtEnd(const Reader* r)
{
    return r->at >= r->n;
}

static int IsWs(uint32_t c)
{
    return c == ' ' || c == '\t';
}

// The length of a newline at the reader (LF or CRLF), 0 for none.
static size_t NewlineLength(const Reader* r)
{
    if (r->at < r->n && r->s[r->at] == '\n')
    {
        return 1;
    }
    return r->at + 1 < r->n && r->s[r->at] == '\r' && r->s[r->at + 1] == '\n' ? 2 : 0;
}

static void SkipNewline(Reader* r)
{
    r->at += NewlineLength(r);
    r->line++;
    r->lineStart = r->at;
}

static void SkipWs(Reader* r)
{
    while (!AtEnd(r) && IsWs(r->s[r->at]))
    {
        r->at++;
    }
}

// Whether a line ends at the reader: a newline or the end.
static int AtLineEnd(const Reader* r)
{
    return AtEnd(r) || NewlineLength(r) > 0;
}

// value-start: what a value or comment may hold besides SP, HTAB and the
// backslash: no controls, no line or paragraph separator.
static int IsValueStart(uint32_t c)
{
    return (c >= 0x21 && c <= 0x5B) || (c >= 0x5D && c <= 0x7E) || (c >= 0xA0 && c <= 0x2027) ||
           (c >= 0x202A && c <= 0xD7FF) || c >= 0xE000;
}

static int IsIdSafe(uint32_t c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           (c >= 0xA1 && c <= 0x1FFF) || c == 0x200C || c == 0x200D ||
           (c >= 0x2030 && c <= 0x205E) || (c >= 0x2070 && c <= 0x2FEF) ||
           (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) ||
           (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

// symbols: what an identifier may escape besides the escapes of values.
static int IsSymbol(uint32_t c)
{
    return (c >= 0x21 && c <= 0x2F) || (c >= 0x3A && c <= 0x40) || (c >= 0x5B && c <= 0x60) ||
           (c >= 0x7B && c <= 0x7E) || (c >= 0xA1 && c <= 0xBF) || c == 0xD7 || c == 0xF7 ||
           (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x205E) ||
           (c >= 0x2190 && c <= 0x2BFF);
}

static int HexValue(unsigned char c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                  : -1;
}

// Reads count hex digits after \x, \u or \U into a code point.
static int ReadHex(Reader* r, size_t count, Buf* out)
{
    uint32_t c = 0;
    for (size_t i = 0; i < count; i++)
    {
        int v = r->at < r->n ? HexValue(r->s[r->at]) : -1;
        if (v < 0)
        {
            return Fail(r, "an escape's hex digits are too few");
        }
        c = c << 4 | (uint32_t)v;
        r->at++;
    }
    if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
    {
        // The grammar takes any digits; a text holds scalar values only.
        return Fail(r, "an escape names no Unicode scalar value");
    }
    BufAppendCode(out, c);
    return 1;
}

// Reads the escape after a backslash. In a value (inValue) the escapes
// MF2 reads pass through whole; in an identifier they and the symbols
// stand for themselves. An escaped newline and the indent after it are
// dropped.
static int ReadEscape(Reader* r, int inValue, Buf* out)
{
    if (AtEnd(r))
    {
        return Fail(r, "a backslash ends the text");
    }
    if (NewlineLength(r) > 0)
    {
        SkipNewline(r);
        if (AtEnd(r) || !IsWs(r->s[r->at]))
        {
            return Fail(r, "an escaped newline is not followed by an indent");
        }
        SkipWs(r);
        return 1;
    }
    size_t length = 0;
    uint32_t c = Peek(r, &length);
    static const char s_simple[] = "\\{|}";
    if (c < 0x80 && strchr(s_simple, (int)c) != nullptr && c != 0)
    {
        r->at += length;
        BufAppend(out, inValue ? "\\" : "", inValue ? 1 : 0);
        BufAppendCode(out, c);
        return 1;
    }
    r->at += length;
    switch (c)
    {
    case ' ':
    case '\t':
        BufAppendCode(out, c);
        return 1;
    case 'n':
        BufAppend(out, "\n", 1);
        return 1;
    case 'r':
        BufAppend(out, "\r", 1);
        return 1;
    case 't':
        BufAppend(out, "\t", 1);
        return 1;
    case 'x':
        return ReadHex(r, 2, out);
    case 'u':
        return ReadHex(r, 4, out);
    case 'U':
        return ReadHex(r, 6, out);
    default:
        break;
    }
    if (!inValue && IsSymbol(c))
    {
        BufAppendCode(out, c);
        return 1;
    }
    r->at -= length;
    return Fail(r, "an escape the grammar does not have");
}

// Reads one id-part: identifier characters and hyphens. first marks an
// identifier's first part, which may not begin with three hyphens.
static int ReadIdPart(Reader* r, int first, Buf* out)
{
    size_t start = r->at;
    size_t before = out->length;
    size_t hyphens = 0;
    while (!AtEnd(r))
    {
        size_t length = 0;
        uint32_t c = Peek(r, &length);
        if (c == '\\')
        {
            r->at++;
            if (!ReadEscape(r, 0, out))
            {
                return 0;
            }
        }
        else if (c == '-' || IsIdSafe(c))
        {
            hyphens += c == '-' && r->at - start == hyphens ? 1 : 0;
            BufAppendCode(out, c);
            r->at += length;
        }
        else
        {
            break;
        }
    }
    if (out->length == before)
    {
        // An escaped newline alone writes nothing, and a part is not empty.
        return Fail(r, "an identifier is expected");
    }
    if (first && hyphens >= 3)
    {
        r->at = start;
        return Fail(r, "an identifier begins with three hyphens unescaped");
    }
    return 1;
}

static void IdPush(ResId* id, Buf* part)
{
    id->parts = GrowArray(id->parts, id->count, sizeof(id->parts[0]));
    id->parts[id->count++] = BufTake(part);
}

// id: an id-start, then parts after dots, with whitespace around them.
static int ReadId(Reader* r, ResId* out)
{
    Buf part = {0};
    if (!ReadIdPart(r, 1, &part))
    {
        free(part.bytes);
        return 0;
    }
    IdPush(out, &part);
    for (;;)
    {
        size_t save = r->at;
        SkipWs(r);
        if (AtEnd(r) || r->s[r->at] != '.')
        {
            r->at = save;
            return 1;
        }
        r->at++;
        SkipWs(r);
        if (!ReadIdPart(r, 0, &part))
        {
            free(part.bytes);
            return 0;
        }
        IdPush(out, &part);
    }
}

// Reads one value-line: content and escapes up to the line's end. Its
// trailing whitespace, unless escaped, is not kept.
static int ReadValueLine(Reader* r, Buf* out)
{
    size_t keep = out->length;
    while (!AtLineEnd(r))
    {
        size_t length = 0;
        uint32_t c = Peek(r, &length);
        if (c == '\\')
        {
            r->at++;
            if (!ReadEscape(r, 1, out))
            {
                return 0;
            }
            keep = out->length;
        }
        else if (IsWs(c) || IsValueStart(c))
        {
            BufAppendCode(out, c);
            r->at += length;
            keep = IsWs(c) ? keep : out->length;
        }
        else
        {
            return Fail(r, "a control character or line separator in a value");
        }
    }
    out->length = keep;
    if (out->bytes != nullptr)
    {
        out->bytes[keep] = '\0';
    }
    return 1;
}

// Joins a value's lines with LF, without the empty lines at its start and
// end.
static ResText JoinLines(Buf* lines, size_t count)
{
    size_t first = 0;
    size_t last = count;
    while (first < last && lines[first].length == 0)
    {
        first++;
    }
    while (last > first && lines[last - 1].length == 0)
    {
        last--;
    }
    Buf joined = {0};
    for (size_t i = first; i < last; i++)
    {
        BufAppend(&joined, i > first ? "\n" : "", i > first ? 1 : 0);
        BufAppend(&joined, lines[i].bytes != nullptr ? lines[i].bytes : "", lines[i].length);
    }
    for (size_t i = 0; i < count; i++)
    {
        free(lines[i].bytes);
    }
    free(lines);
    return BufTake(&joined);
}

// value: its first line, then lines indented after one or more newlines.
// The reader stops after the last line read; the newlines after it are
// the resource's.
static int ReadValue(Reader* r, ResText* out)
{
    Buf* lines = GrowArray(nullptr, 0, sizeof(Buf));
    size_t count = 1;
    lines[0] = (Buf){0};
    int ok = ReadValueLine(r, &lines[0]);
    while (ok)
    {
        Reader save = *r;
        size_t empty = 0;
        while (NewlineLength(r) > 0)
        {
            SkipNewline(r);
            empty++;
        }
        if (empty == 0 || AtEnd(r) || !IsWs(r->s[r->at]))
        {
            *r = save;
            break;
        }
        SkipWs(r);
        for (size_t i = 1; i < empty; i++)
        {
            lines = GrowArray(lines, count, sizeof(Buf));
            lines[count++] = (Buf){0};
        }
        lines = GrowArray(lines, count, sizeof(Buf));
        lines[count] = (Buf){0};
        ok = ReadValueLine(r, &lines[count++]);
    }
    *out = JoinLines(lines, count);
    return ok;
}

// Reads a comment line after its #: the sigil and one space or tab off,
// trailing whitespace off.
static int ReadCommentLine(Reader* r, Buf* out)
{
    r->at++;
    if (!AtEnd(r) && IsWs(r->s[r->at]))
    {
        r->at++;
    }
    size_t keep = out->length;
    while (!AtLineEnd(r))
    {
        size_t length = 0;
        uint32_t c = Peek(r, &length);
        if (!(IsWs(c) || IsValueStart(c) || c == '\\'))
        {
            return Fail(r, "a control character or line separator in a comment");
        }
        BufAppendCode(out, c);
        r->at += length;
        keep = IsWs(c) ? keep : out->length;
    }
    out->length = keep;
    if (out->bytes != nullptr)
    {
        out->bytes[keep] = '\0';
    }
    return 1;
}

static ResSection* Current(Reader* r)
{
    return &r->out->sections[r->out->sectionCount - 1];
}

static void AddSection(Resource* out)
{
    out->sections = GrowArray(out->sections, out->sectionCount, sizeof(out->sections[0]));
    out->sections[out->sectionCount++] = (ResSection){0};
}

static ResItem* AddItem(ResSection* s)
{
    s->items = GrowArray(s->items, s->itemCount, sizeof(s->items[0]));
    s->items[s->itemCount] = (ResItem){0};
    return &s->items[s->itemCount++];
}

// A waiting comment that nothing follows becomes a comment of its own.
static void FlushComment(Reader* r)
{
    if (r->hasComment)
    {
        ResItem* item = AddItem(Current(r));
        item->kind = resComment;
        item->comment = BufTake(&r->comment);
        item->line = r->line;
        r->hasComment = 0;
    }
}

// Hands the waiting comment and metadata to what they describe.
static void Attach(Reader* r, ResText* comment, ResMeta** meta, size_t* metaCount)
{
    *comment = BufTake(&r->comment);
    r->hasComment = 0;
    *meta = r->meta;
    *metaCount = r->metaCount;
    r->meta = nullptr;
    r->metaCount = 0;
}

static int ReadComment(Reader* r)
{
    if (r->metaCount > 0)
    {
        return Fail(r, "a comment between metadata and what it describes");
    }
    if (r->hasComment)
    {
        BufAppend(&r->comment, "\n", 1);
    }
    r->hasComment = 1;
    return ReadCommentLine(r, &r->comment);
}

// metadata: @, an id-part, and a value after whitespace.
static int ReadMeta(Reader* r)
{
    r->at++;
    Buf key = {0};
    if (!ReadIdPart(r, 0, &key))
    {
        free(key.bytes);
        return 0;
    }
    ResMeta m = {BufTake(&key), {0}};
    int ok = 1;
    if (!AtLineEnd(r) && IsWs(r->s[r->at]))
    {
        SkipWs(r);
        ok = ReadValue(r, &m.value);
    }
    else if (!AtLineEnd(r))
    {
        ok = Fail(r, "metadata's key is not followed by whitespace");
    }
    if (m.value.bytes == nullptr)
    {
        Buf none = {0};
        m.value = BufTake(&none);
    }
    r->meta = GrowArray(r->meta, r->metaCount, sizeof(r->meta[0]));
    r->meta[r->metaCount++] = m;
    return ok;
}

// The comments before the frontmatter, and its metadata, are the
// resource's; nothing but comments and metadata may come before it.
static int ReadFrontmatter(Reader* r)
{
    if (r->out->hasFrontmatter)
    {
        return Fail(r, "a second frontmatter line");
    }
    if (r->sawBody)
    {
        return Fail(r, "a frontmatter line after a section or an entry");
    }
    FlushComment(r);
    Buf comment = {0};
    ResSection* s = Current(r);
    for (size_t i = 0; i < s->itemCount; i++)
    {
        BufAppend(&comment, i > 0 ? "\n\n" : "", i > 0 ? 2 : 0);
        BufAppend(&comment, s->items[i].comment.bytes, s->items[i].comment.length);
        free(s->items[i].comment.bytes);
    }
    free(s->items);
    s->items = nullptr;
    s->itemCount = 0;
    r->out->comment = BufTake(&comment);
    r->out->meta = r->meta;
    r->out->metaCount = r->metaCount;
    r->meta = nullptr;
    r->metaCount = 0;
    r->out->hasFrontmatter = 1;
    r->at += 3;
    SkipWs(r);
    return 1;
}

static int ReadSectionHead(Reader* r)
{
    size_t line = r->line;
    r->at++;
    SkipWs(r);
    AddSection(r->out);
    ResSection* s = Current(r);
    s->line = line;
    if (!ReadId(r, &s->id))
    {
        return 0;
    }
    SkipWs(r);
    if (AtEnd(r) || r->s[r->at] != ']')
    {
        return Fail(r, "a section head without its ]");
    }
    r->at++;
    SkipWs(r);
    Attach(r, &s->comment, &s->meta, &s->metaCount);
    r->sawBody = 1;
    return 1;
}

static int ReadEntry(Reader* r)
{
    ResItem* item = AddItem(Current(r));
    item->kind = resEntry;
    item->line = r->line;
    Attach(r, &item->comment, &item->meta, &item->metaCount);
    r->sawBody = 1;
    if (!ReadId(r, &item->id))
    {
        return 0;
    }
    SkipWs(r);
    if (AtEnd(r) || r->s[r->at] != '=')
    {
        return Fail(r, "an entry's identifier is not followed by =");
    }
    r->at++;
    SkipWs(r);
    return ReadValue(r, &item->value);
}

static int IsFrontmatter(const Reader* r)
{
    if (r->at + 3 > r->n || memcmp(r->s + r->at, "---", 3) != 0)
    {
        return 0;
    }
    size_t at = r->at + 3;
    while (at < r->n && IsWs(r->s[at]))
    {
        at++;
    }
    return at >= r->n || r->s[at] == '\n' ||
           (r->s[at] == '\r' && at + 1 < r->n && r->s[at + 1] == '\n');
}

// An empty line: ends a waiting comment; may not come between metadata
// and what it describes.
static int ReadEmptyLine(Reader* r)
{
    SkipWs(r);
    if (!AtLineEnd(r))
    {
        return Fail(r, r->s[r->at] == '\r'
                           ? "a carriage return without a line feed"
                           : "a line that starts with whitespace continues nothing");
    }
    if (r->metaCount > 0)
    {
        return Fail(r, "an empty line between metadata and what it describes");
    }
    FlushComment(r);
    return 1;
}

static int ReadLine(Reader* r)
{
    unsigned char c = r->s[r->at];
    int ok = c == '\n' || c == '\r' || c == ' ' || c == '\t' ? ReadEmptyLine(r)
             : c == '#'                                      ? ReadComment(r)
             : c == '@'                                      ? ReadMeta(r)
             : c == '['                                      ? ReadSectionHead(r)
             : IsFrontmatter(r)                              ? ReadFrontmatter(r)
                                                             : ReadEntry(r);
    if (ok && !AtLineEnd(r))
    {
        return Fail(r, r->s[r->at] == '\r' ? "a carriage return without a line feed"
                                           : "more on a line after its end");
    }
    return ok;
}

int ResourceRead(const char* text, size_t length, Resource* out, ResError* error)
{
    *out = (Resource){0};
    *error = (ResError){0};
    AddSection(out);
    Reader r = {
        .s = (const unsigned char*)text, .n = length, .line = 1, .error = error, .out = out};
    if (length > 0 && muniValidateUtf8(text, length).status != muni_success)
    {
        error->line = 1;
        error->column = 1;
        error->reason = "the text is not UTF-8";
        return 0;
    }
    int ok = 1;
    while (ok && !AtEnd(&r))
    {
        ok = ReadLine(&r);
        if (ok && !AtEnd(&r))
        {
            SkipNewline(&r);
        }
    }
    if (ok && r.metaCount > 0)
    {
        ok = Fail(&r, "metadata with nothing after it to describe");
    }
    if (ok)
    {
        FlushComment(&r);
    }
    free(r.comment.bytes);
    for (size_t i = 0; i < r.metaCount; i++)
    {
        free(r.meta[i].key.bytes);
        free(r.meta[i].value.bytes);
    }
    free(r.meta);
    return ok;
}

static void FreeMeta(ResMeta* meta, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        free(meta[i].key.bytes);
        free(meta[i].value.bytes);
    }
    free(meta);
}

static void FreeId(ResId* id)
{
    for (size_t i = 0; i < id->count; i++)
    {
        free(id->parts[i].bytes);
    }
    free(id->parts);
}

void ResourceFree(Resource* resource)
{
    for (size_t s = 0; s < resource->sectionCount; s++)
    {
        ResSection* section = &resource->sections[s];
        for (size_t i = 0; i < section->itemCount; i++)
        {
            ResItem* item = &section->items[i];
            FreeId(&item->id);
            free(item->comment.bytes);
            FreeMeta(item->meta, item->metaCount);
            free(item->value.bytes);
        }
        free(section->items);
        FreeId(&section->id);
        free(section->comment.bytes);
        FreeMeta(section->meta, section->metaCount);
    }
    free(resource->sections);
    free(resource->comment.bytes);
    FreeMeta(resource->meta, resource->metaCount);
    *resource = (Resource){0};
}
