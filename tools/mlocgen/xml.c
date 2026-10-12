// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The XML reader (xml.h). A document is read into a tree in one pass;
// errors stop the pass and name the line.

#include "xml.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Elements nest no deeper than this; CLDR's deepest is about a dozen.
#define MAX_DEPTH 256

typedef struct Parser
{
    const char* start;
    const char* p;
    const char* end;
    // Line counting is incremental: lineAt has been counted up to.
    const char* lineAt;
    int line;
    char* error;
    size_t errorCapacity;
    int failed;
} Parser;

typedef struct Buffer
{
    char* data;
    size_t length;
    size_t capacity;
} Buffer;

static int LineOf(Parser* parser, const char* at)
{
    if (at < parser->lineAt)
    {
        parser->lineAt = parser->start;
        parser->line = 1;
    }
    for (; parser->lineAt < at; parser->lineAt++)
    {
        parser->line += *parser->lineAt == '\n';
    }
    return parser->line;
}

static void Refuse(Parser* parser, const char* format, ...)
{
    if (parser->failed)
    {
        return;
    }
    parser->failed = 1;
    int used =
        snprintf(parser->error, parser->errorCapacity, "line %d: ", LineOf(parser, parser->p));
    if (used >= 0 && (size_t)used < parser->errorCapacity)
    {
        va_list args;
        va_start(args, format);
        vsnprintf(parser->error + used, parser->errorCapacity - (size_t)used, format, args);
        va_end(args);
    }
}

static void* Grow(void* memory, size_t count, size_t size)
{
    void* grown = realloc(memory, count * size);
    if (grown == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    return grown;
}

static void Append(Buffer* buffer, const char* bytes, size_t length)
{
    if (buffer->length + length + 1 > buffer->capacity)
    {
        size_t capacity = buffer->capacity == 0 ? 64 : buffer->capacity;
        while (buffer->length + length + 1 > capacity)
        {
            capacity *= 2;
        }
        buffer->data = Grow(buffer->data, capacity, 1);
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, bytes, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
}

static char* Finish(Buffer* buffer)
{
    if (buffer->data == nullptr)
    {
        Append(buffer, "", 0);
    }
    return buffer->data;
}

static int StartsWith(const Parser* parser, const char* text)
{
    size_t length = strlen(text);
    return (size_t)(parser->end - parser->p) >= length && memcmp(parser->p, text, length) == 0;
}

static int IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static void SkipSpace(Parser* parser)
{
    while (parser->p < parser->end && IsSpace(*parser->p))
    {
        parser->p++;
    }
}

// Moves past the next occurrence of marker; returns where it began, or
// null (and refuses) when the document ends first.
static const char* SkipPast(Parser* parser, const char* marker, const char* what)
{
    size_t length = strlen(marker);
    for (const char* at = parser->p; at + length <= parser->end; at++)
    {
        if (memcmp(at, marker, length) == 0)
        {
            parser->p = at + length;
            return at;
        }
    }
    Refuse(parser, "%s is not closed", what);
    return nullptr;
}

// A DOCTYPE: its quoted strings and internal subset are skipped whole.
static void SkipDoctype(Parser* parser)
{
    int depth = 0;
    char quote = 0;
    for (; parser->p < parser->end; parser->p++)
    {
        char c = *parser->p;
        if (quote != 0)
        {
            quote = c == quote ? 0 : quote;
        }
        else if (c == '"' || c == '\'')
        {
            quote = c;
        }
        else if (c == '[')
        {
            depth++;
        }
        else if (c == ']')
        {
            depth--;
        }
        else if (c == '>' && depth == 0)
        {
            parser->p++;
            return;
        }
    }
    Refuse(parser, "DOCTYPE is not closed");
}

// Comments, processing instructions and a DOCTYPE between markup; returns
// 1 when one was skipped.
static int SkipMisc(Parser* parser)
{
    if (StartsWith(parser, "<!--"))
    {
        parser->p += 4;
        return SkipPast(parser, "-->", "a comment") != nullptr;
    }
    if (StartsWith(parser, "<?"))
    {
        parser->p += 2;
        return SkipPast(parser, "?>", "a processing instruction") != nullptr;
    }
    if (StartsWith(parser, "<!DOCTYPE"))
    {
        SkipDoctype(parser);
        return !parser->failed;
    }
    return 0;
}

static int IsNameByte(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           c == ':' || c == '.' || c == '-' || c >= 0x80;
}

static char* ParseName(Parser* parser)
{
    const char* begin = parser->p;
    while (parser->p < parser->end && IsNameByte((unsigned char)*parser->p))
    {
        parser->p++;
    }
    if (parser->p == begin || (*begin >= '0' && *begin <= '9') || *begin == '-' || *begin == '.')
    {
        Refuse(parser, "a name was expected");
        return nullptr;
    }
    Buffer name = {0};
    Append(&name, begin, (size_t)(parser->p - begin));
    return name.data;
}

static void AppendCodePoint(Buffer* buffer, uint32_t c)
{
    char bytes[4];
    size_t length = 0;
    if (c < 0x80)
    {
        bytes[length++] = (char)c;
    }
    else if (c < 0x800)
    {
        bytes[length++] = (char)(0xC0 | (c >> 6));
        bytes[length++] = (char)(0x80 | (c & 0x3F));
    }
    else if (c < 0x10000)
    {
        bytes[length++] = (char)(0xE0 | (c >> 12));
        bytes[length++] = (char)(0x80 | ((c >> 6) & 0x3F));
        bytes[length++] = (char)(0x80 | (c & 0x3F));
    }
    else
    {
        bytes[length++] = (char)(0xF0 | (c >> 18));
        bytes[length++] = (char)(0x80 | ((c >> 12) & 0x3F));
        bytes[length++] = (char)(0x80 | ((c >> 6) & 0x3F));
        bytes[length++] = (char)(0x80 | (c & 0x3F));
    }
    Append(buffer, bytes, length);
}

// A character reference after "&#": decimal, or hexadecimal after 'x'.
static void DecodeCharacterReference(Parser* parser, const char* body, size_t length, Buffer* out)
{
    int hex = length > 0 && body[0] == 'x';
    size_t i = (size_t)hex;
    uint32_t value = 0;
    if (i == length)
    {
        Refuse(parser, "an empty character reference");
        return;
    }
    for (; i < length; i++)
    {
        char c = body[i];
        uint32_t digit = c >= '0' && c <= '9'          ? (uint32_t)(c - '0')
                         : hex && c >= 'a' && c <= 'f' ? (uint32_t)(c - 'a' + 10)
                         : hex && c >= 'A' && c <= 'F' ? (uint32_t)(c - 'A' + 10)
                                                       : 99;
        if (digit > (hex ? 15u : 9u) || value > 0x10FFFF)
        {
            Refuse(parser, "a malformed character reference");
            return;
        }
        value = value * (hex ? 16 : 10) + digit;
    }
    if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
    {
        Refuse(parser, "a character reference to U+%04X, which XML does not allow", value);
        return;
    }
    AppendCodePoint(out, value);
}

static void DecodeEntity(Parser* parser, const char* body, size_t length, Buffer* out)
{
    static const struct
    {
        const char* name;
        char value;
    } entities[] = {{"lt", '<'}, {"gt", '>'}, {"amp", '&'}, {"quot", '"'}, {"apos", '\''}};
    if (length > 0 && body[0] == '#')
    {
        DecodeCharacterReference(parser, body + 1, length - 1, out);
        return;
    }
    for (size_t i = 0; i < sizeof(entities) / sizeof(entities[0]); i++)
    {
        if (strlen(entities[i].name) == length && memcmp(entities[i].name, body, length) == 0)
        {
            Append(out, &entities[i].value, 1);
            return;
        }
    }
    Refuse(parser, "the entity &%.*s; is not one XML predefines", (int)length, body);
}

// Appends text with its references decoded. In attribute values, each
// white space character becomes a space, as XML normalizes them.
static void AppendDecoded(Parser* parser, const char* begin, const char* end, int attribute,
                          Buffer* out)
{
    const char* run = begin;
    for (const char* at = begin; at < end && !parser->failed; at++)
    {
        if (*at != '&' && !(attribute && IsSpace(*at)))
        {
            continue;
        }
        Append(out, run, (size_t)(at - run));
        if (*at != '&')
        {
            Append(out, " ", 1);
            run = at + 1;
            continue;
        }
        const char* semicolon = memchr(at, ';', (size_t)(end - at));
        if (semicolon == nullptr)
        {
            Refuse(parser, "an entity reference is not closed");
            return;
        }
        DecodeEntity(parser, at + 1, (size_t)(semicolon - at - 1), out);
        at = semicolon;
        run = at + 1;
    }
    Append(out, run, (size_t)(end - run));
}

static void AddAttribute(Parser* parser, XmlNode* node, char* name, char* value)
{
    for (int i = 0; i < node->attributeCount; i++)
    {
        if (strcmp(node->attributes[i].name, name) == 0)
        {
            Refuse(parser, "the attribute %s is given twice", name);
            free(name);
            free(value);
            return;
        }
    }
    node->attributes =
        Grow(node->attributes, (size_t)node->attributeCount + 1, sizeof(XmlAttribute));
    node->attributes[node->attributeCount++] = (XmlAttribute){name, value};
}

// Attributes up to '>' or "/>"; returns 1 for an empty element.
static int ParseAttributes(Parser* parser, XmlNode* node)
{
    for (;;)
    {
        SkipSpace(parser);
        if (StartsWith(parser, "/>"))
        {
            parser->p += 2;
            return 1;
        }
        if (StartsWith(parser, ">"))
        {
            parser->p++;
            return 0;
        }
        char* name = ParseName(parser);
        if (name == nullptr)
        {
            return 0;
        }
        SkipSpace(parser);
        int equals = parser->p < parser->end && *parser->p == '=';
        parser->p += equals;
        SkipSpace(parser);
        char quote = equals && parser->p < parser->end ? *parser->p : 0;
        if (quote != '"' && quote != '\'')
        {
            free(name);
            Refuse(parser, "an attribute value was expected");
            return 0;
        }
        parser->p++;
        const char* begin = parser->p;
        const char* close = memchr(begin, quote, (size_t)(parser->end - begin));
        if (close == nullptr || memchr(begin, '<', (size_t)(close - begin)) != nullptr)
        {
            free(name);
            Refuse(parser, "an attribute value is not closed");
            return 0;
        }
        Buffer value = {0};
        AppendDecoded(parser, begin, close, 1, &value);
        parser->p = close + 1;
        AddAttribute(parser, node, name, Finish(&value));
        if (parser->failed)
        {
            return 0;
        }
    }
}

static XmlNode* ParseElement(Parser* parser, XmlNode* parent, int depth);

static void AddChild(XmlNode* node, XmlNode* child)
{
    node->children = Grow(node->children, (size_t)node->childCount + 1, sizeof(XmlNode*));
    node->children[node->childCount++] = child;
}

// The content after a start tag, up to and including the end tag.
static void ParseContent(Parser* parser, XmlNode* node, Buffer* text, int depth)
{
    while (!parser->failed)
    {
        const char* lt = memchr(parser->p, '<', (size_t)(parser->end - parser->p));
        if (lt == nullptr)
        {
            Refuse(parser, "the element <%s> is not closed", node->name);
            return;
        }
        AppendDecoded(parser, parser->p, lt, 0, text);
        parser->p = lt;
        if (StartsWith(parser, "</"))
        {
            parser->p += 2;
            size_t length = strlen(node->name);
            if ((size_t)(parser->end - parser->p) < length ||
                memcmp(parser->p, node->name, length) != 0 ||
                IsNameByte((unsigned char)parser->p[length]))
            {
                Refuse(parser, "the end tag does not close <%s>", node->name);
                return;
            }
            parser->p += length;
            SkipSpace(parser);
            if (!StartsWith(parser, ">"))
            {
                Refuse(parser, "the end tag of <%s> is malformed", node->name);
                return;
            }
            parser->p++;
            return;
        }
        if (StartsWith(parser, "<![CDATA["))
        {
            parser->p += 9;
            const char* begin = parser->p;
            const char* close = SkipPast(parser, "]]>", "a CDATA section");
            if (close != nullptr)
            {
                Append(text, begin, (size_t)(close - begin));
            }
            continue;
        }
        if (SkipMisc(parser))
        {
            continue;
        }
        XmlNode* child = ParseElement(parser, node, depth + 1);
        if (child != nullptr)
        {
            AddChild(node, child);
        }
    }
}

static XmlNode* ParseElement(Parser* parser, XmlNode* parent, int depth)
{
    if (depth > MAX_DEPTH)
    {
        Refuse(parser, "elements nest deeper than %d", MAX_DEPTH);
        return nullptr;
    }
    if (!StartsWith(parser, "<"))
    {
        Refuse(parser, "an element was expected");
        return nullptr;
    }
    XmlNode* node = Grow(nullptr, 1, sizeof(XmlNode));
    *node = (XmlNode){.parent = parent, .line = LineOf(parser, parser->p)};
    parser->p++;
    node->name = ParseName(parser);
    Buffer text = {0};
    if (node->name != nullptr && !ParseAttributes(parser, node) && !parser->failed)
    {
        ParseContent(parser, node, &text, depth);
    }
    node->text = Finish(&text);
    node->textLength = text.length;
    if (parser->failed)
    {
        XmlFree(node);
        return nullptr;
    }
    return node;
}

XmlNode* XmlParse(const char* bytes, size_t length, char* errorOut, size_t errorCapacity)
{
    Parser parser = {bytes, bytes, bytes + length, bytes, 1, errorOut, errorCapacity, 0};
    if (length >= 3 && memcmp(bytes, "\xEF\xBB\xBF", 3) == 0)
    {
        parser.p += 3;
    }
    for (;;)
    {
        SkipSpace(&parser);
        if (!SkipMisc(&parser))
        {
            break;
        }
    }
    if (!parser.failed && parser.p == parser.end)
    {
        Refuse(&parser, "no root element");
    }
    XmlNode* root = parser.failed ? nullptr : ParseElement(&parser, nullptr, 0);
    while (root != nullptr && !parser.failed)
    {
        SkipSpace(&parser);
        if (parser.p == parser.end)
        {
            return root;
        }
        if (!SkipMisc(&parser))
        {
            Refuse(&parser, "content after the root element");
        }
    }
    XmlFree(root);
    return nullptr;
}

XmlNode* XmlParseFile(const char* path, char* errorOut, size_t errorCapacity)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
    {
        snprintf(errorOut, errorCapacity, "cannot open %s", path);
        return nullptr;
    }
    Buffer bytes = {0};
    char chunk[65536];
    size_t got;
    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0)
    {
        Append(&bytes, chunk, got);
    }
    int readError = ferror(file);
    fclose(file);
    if (readError)
    {
        free(bytes.data);
        snprintf(errorOut, errorCapacity, "cannot read %s", path);
        return nullptr;
    }
    char message[512];
    XmlNode* root = XmlParse(Finish(&bytes), bytes.length, message, sizeof(message));
    free(bytes.data);
    if (root == nullptr)
    {
        snprintf(errorOut, errorCapacity, "%s: %s", path, message);
    }
    return root;
}

const char* XmlGetAttribute(const XmlNode* node, const char* name)
{
    for (int i = 0; i < node->attributeCount; i++)
    {
        if (strcmp(node->attributes[i].name, name) == 0)
        {
            return node->attributes[i].value;
        }
    }
    return nullptr;
}

XmlNode* XmlFindChild(const XmlNode* node, const char* name)
{
    for (int i = 0; i < node->childCount; i++)
    {
        if (strcmp(node->children[i]->name, name) == 0)
        {
            return node->children[i];
        }
    }
    return nullptr;
}

void XmlFree(XmlNode* root)
{
    if (root == nullptr)
    {
        return;
    }
    for (int i = 0; i < root->childCount; i++)
    {
        XmlFree(root->children[i]);
    }
    for (int i = 0; i < root->attributeCount; i++)
    {
        free(root->attributes[i].name);
        free(root->attributes[i].value);
    }
    free(root->attributes);
    free(root->children);
    free(root->name);
    free(root->text);
    free(root);
}
