// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An XLIFF unit's codes (xliff_codes.h).

#include "xliff_codes.h"

#include "catalog_set.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// The length of the UTF-8 sequence at s and its code point; a byte that
// starts none is one of its own.
static size_t Decode(const unsigned char* s, size_t n, unsigned* cp)
{
    size_t length = s[0] < 0x80 ? 1 : s[0] < 0xE0 ? 2 : s[0] < 0xF0 ? 3 : 4;
    if (length > n || (s[0] < 0xC0 && s[0] >= 0x80))
    {
        *cp = s[0];
        return 1;
    }
    unsigned value = length == 1 ? s[0] : s[0] & (0x7F >> length);
    for (size_t i = 1; i < length; i++)
    {
        value = value << 6 | (s[i] & 0x3F);
    }
    *cp = value;
    return length;
}

// One character: escaped, or XLIFF's <cp> for one XML cannot hold (in an
// attribute a replacement the import never reads).
static void WriteCharacter(FILE* out, const unsigned char* s, size_t n, unsigned cp, int attribute)
{
    int allowed =
        cp == 0x9 || cp == 0xA || cp == 0xD || (cp >= 0x20 && cp != 0xFFFE && cp != 0xFFFF);
    if (!allowed)
    {
        fprintf(out, attribute ? "&#xFFFD;" : "<cp hex=\"%04X\"/>", cp);
    }
    else if (*s == '&' || *s == '<' || *s == '>' || (attribute && *s == '"'))
    {
        fputs(*s == '&' ? "&amp;" : *s == '<' ? "&lt;" : *s == '>' ? "&gt;" : "&quot;", out);
    }
    else if (attribute && (*s == '\n' || *s == '\r' || *s == '\t'))
    {
        fprintf(out, "&#%d;", *s);
    }
    else
    {
        fwrite(s, 1, n, out);
    }
}

void XliffWriteText(FILE* out, const char* text, size_t length, int attribute)
{
    const unsigned char* s = (const unsigned char*)text;
    for (size_t i = 0; i < length;)
    {
        unsigned cp = 0;
        size_t n = Decode(s + i, length - i, &cp);
        WriteCharacter(out, s + i, n, cp, attribute);
        i += n;
    }
}

static void* Allocate(size_t count, size_t size)
{
    void* p = calloc(count + 1, size);
    if (p == nullptr)
    {
        exit(1);
    }
    return p;
}

static int IsCode(const XliffMessage* m, size_t i)
{
    Mf2PartKind kind = m->parts[i].kind;
    return kind != mf2Text && kind != mf2PatternStart && kind != mf2PatternEnd &&
           (m->owned == nullptr || kind != mf2Syntax);
}

// The data of a code's text, added when new.
static size_t DataOf(XliffCodes* codes, const char* text, size_t length)
{
    for (size_t i = 0; i < codes->dataCount; i++)
    {
        if (codes->data[i].length == length && memcmp(codes->data[i].bytes, text, length) == 0)
        {
            return i;
        }
    }
    ResText* grown = realloc(codes->data, (codes->dataCount + 1) * sizeof(ResText));
    if (grown == nullptr)
    {
        exit(1);
    }
    codes->data = grown;
    codes->data[codes->dataCount] = (ResText){(char*)text, length};
    return codes->dataCount++;
}

// Pairs each pattern's markup: a close with the latest open still alone.
static void Pair(XliffMessage* m)
{
    size_t* open = Allocate(m->count, sizeof(size_t));
    size_t depth = 0;
    for (size_t i = 0; i < m->count; i++)
    {
        m->partner[i] = SIZE_MAX;
        if (m->parts[i].kind == mf2PatternStart)
        {
            depth = 0;
        }
        else if (m->parts[i].kind == mf2MarkupOpen)
        {
            open[depth++] = i;
        }
        else if (m->parts[i].kind == mf2MarkupClose && depth > 0)
        {
            size_t start = open[--depth];
            m->partner[i] = start;
            m->partner[start] = i;
        }
    }
    free(open);
}

// A target code's number: the first code of its source's data not taken.
static size_t Number(XliffCodes* codes, const XliffMessage* s, size_t data, unsigned char* taken)
{
    for (size_t i = 0; i < s->count; i++)
    {
        if (IsCode(s, i) && s->data[i] == data && !taken[i])
        {
            taken[i] = 1;
            return s->number[i];
        }
    }
    return ++codes->numbers;
}

static size_t Add(XliffCodes* codes, const char* text, size_t length, char* owned, size_t source)
{
    codes->messages = CatalogGrow(codes->messages, codes->messageCount, sizeof(XliffMessage));
    size_t index = codes->messageCount++;
    XliffMessage* m = &codes->messages[index];
    *m = (XliffMessage){0};
    m->owned = owned;
    if (!Mf2Split(text, length, &m->parts, &m->count))
    {
        return index;
    }
    m->text = text;
    m->data = Allocate(m->count, sizeof(size_t));
    m->number = Allocate(m->count, sizeof(size_t));
    m->partner = Allocate(m->count, sizeof(size_t));
    const XliffMessage* s = source == xliffSource ? nullptr : &codes->messages[source];
    unsigned char* taken = Allocate(s != nullptr ? s->count : 0, 1);
    for (size_t i = 0; i < m->count; i++)
    {
        if (IsCode(m, i))
        {
            m->data[i] = DataOf(codes, m->text + m->parts[i].start, m->parts[i].length);
            m->number[i] = s == nullptr ? ++codes->numbers : Number(codes, s, m->data[i], taken);
        }
    }
    free(taken);
    Pair(m);
    return index;
}

size_t XliffCodesAdd(XliffCodes* codes, const ResText* message, size_t source)
{
    return Add(codes, message->bytes, message->length, nullptr, source);
}

size_t XliffCodesAddPattern(XliffCodes* codes, const char* pattern, size_t length, size_t source)
{
    char* quoted = malloc(length + 5);
    if (quoted == nullptr)
    {
        exit(1);
    }
    (void)snprintf(quoted, length + 5, "{{%.*s}}", (int)length, pattern);
    return Add(codes, quoted, length + 4, quoted, source);
}

void XliffCodesWriteData(const XliffCodes* codes, FILE* out)
{
    if (codes->dataCount == 0)
    {
        return;
    }
    fputs("      <originalData>\n", out);
    for (size_t i = 0; i < codes->dataCount; i++)
    {
        fprintf(out, "        <data id=\"d%zu\">", i + 1);
        XliffWriteText(out, codes->data[i].bytes, codes->data[i].length, 0);
        fputs("</data>\n", out);
    }
    fputs("      </originalData>\n", out);
}

// Pattern text with MF2's escapes undone.
static void WriteText(FILE* out, const char* text, size_t length)
{
    size_t from = 0;
    for (size_t i = 0; i < length; i++)
    {
        if (text[i] == '\\' && i + 1 < length)
        {
            XliffWriteText(out, text + from, i - from, 0);
            from = i + 1;
            i++;
        }
    }
    XliffWriteText(out, text + from, length - from, 0);
}

static void WriteCode(const XliffMessage* m, size_t i, FILE* out)
{
    size_t data = m->data[i] + 1;
    size_t number = m->number[i];
    switch (m->parts[i].kind)
    {
    case mf2MarkupOpen:
        fprintf(out,
                m->partner[i] == SIZE_MAX ? "<sc id=\"%zu\" dataRef=\"d%zu\" isolated=\"yes\"/>"
                                          : "<sc id=\"%zu\" dataRef=\"d%zu\"/>",
                number, data);
        break;
    case mf2MarkupClose:
        if (m->partner[i] == SIZE_MAX)
        {
            fprintf(out, "<ec id=\"%zu\" dataRef=\"d%zu\" isolated=\"yes\"/>", number, data);
        }
        else
        {
            fprintf(out, "<ec startRef=\"%zu\" dataRef=\"d%zu\"/>", m->number[m->partner[i]], data);
        }
        break;
    case mf2Syntax:
        fprintf(out, "<ph id=\"%zu\" dataRef=\"d%zu\" canDelete=\"no\"/>", number, data);
        break;
    default:
        fprintf(out, "<ph id=\"%zu\" dataRef=\"d%zu\"/>", number, data);
        break;
    }
}

void XliffCodesWriteMessage(const XliffCodes* codes, size_t index, FILE* out)
{
    const XliffMessage* m = &codes->messages[index];
    for (size_t i = 0; i < m->count; i++)
    {
        if (m->parts[i].kind == mf2Text)
        {
            WriteText(out, m->text + m->parts[i].start, m->parts[i].length);
        }
        else if (IsCode(m, i))
        {
            WriteCode(m, i, out);
        }
    }
}

void XliffCodesFree(XliffCodes* codes)
{
    for (size_t i = 0; i < codes->messageCount; i++)
    {
        free(codes->messages[i].parts);
        free(codes->messages[i].data);
        free(codes->messages[i].number);
        free(codes->messages[i].partner);
        free(codes->messages[i].owned);
    }
    free(codes->messages);
    free(codes->data);
    *codes = (XliffCodes){0};
}
