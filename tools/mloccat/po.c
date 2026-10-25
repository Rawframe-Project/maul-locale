// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// PO files (po.h).

#include "po.h"

#include "catalog_set.h"

#include <stdlib.h>
#include <string.h>

// A growing NUL-terminated string.
typedef struct Str
{
    char* bytes;
    size_t length;
} Str;

static void Append(Str* s, const char* bytes, size_t length)
{
    char* grown = realloc(s->bytes, s->length + length + 1);
    if (grown == nullptr)
    {
        exit(1);
    }
    memcpy(grown + s->length, bytes, length);
    s->bytes = grown;
    s->length += length;
    s->bytes[s->length] = '\0';
}

typedef enum Field
{
    fieldNone,
    fieldContext,
    fieldId,
    fieldString,
} Field;

// The entry being read.
typedef struct Reading
{
    Str context;
    Str id;
    Str string;
    Str comments;
    int hasContext;
    int hasId;
    int hasString;
    int fuzzy;
    int mf2;
    size_t line;
    Field field;
} Reading;

static int Hex(char c)
{
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}

// One escape after a backslash at p; its byte in out, its length.
static size_t Escape(const char* p, const char* end, char* out)
{
    static const char s_from[] = "ntr\"\\abfv?'";
    static const char s_to[] = "\n\t\r\"\\\a\b\f\v?'";
    const char* known = p < end && *p != '\0' ? strchr(s_from, *p) : nullptr;
    if (known != nullptr)
    {
        *out = s_to[known - s_from];
        return 1;
    }
    if (p < end && *p >= '0' && *p <= '7')
    {
        int value = 0;
        size_t n = 0;
        while (n < 3 && p + n < end && p[n] >= '0' && p[n] <= '7')
        {
            value = value * 8 + (p[n] - '0');
            n++;
        }
        *out = (char)value;
        return value <= 0xFF ? n : 0;
    }
    if (p + 1 < end && *p == 'x' && Hex(p[1]) >= 0)
    {
        int value = Hex(p[1]);
        size_t n = 2;
        if (p + 2 < end && Hex(p[2]) >= 0)
        {
            value = value * 16 + Hex(p[2]);
            n = 3;
        }
        *out = (char)value;
        return n;
    }
    return 0;
}

// A quoted string from p to the line's end, appended; null, or why it is
// refused. The tool keeps strings as C strings, so a NUL byte in one, as
// written or escaped, would cut the message short.
static const char* QuotedString(const char* p, const char* end, Str* out)
{
    while (p < end && (*p == ' ' || *p == '\t'))
    {
        p++;
    }
    if (p == end || *p != '"')
    {
        return "a string that is not one";
    }
    for (p++; p < end && *p != '"'; p++)
    {
        char c = *p;
        if (c == '\\')
        {
            size_t n = Escape(p + 1, end, &c);
            if (n == 0)
            {
                return "a string that is not one";
            }
            p += n;
        }
        if (c == '\0')
        {
            return "a NUL byte in a string";
        }
        Append(out, &c, 1);
    }
    if (p == end)
    {
        return "a string that is not one";
    }
    for (p++; p < end && (*p == ' ' || *p == '\t' || *p == '\r'); p++)
    {
    }
    return p == end ? nullptr : "a string that is not one";
}

static char* Take(Str* s)
{
    if (s->bytes == nullptr)
    {
        Append(s, "", 0);
    }
    char* bytes = s->bytes;
    *s = (Str){0};
    return bytes;
}

static void Discard(Reading* r)
{
    free(r->context.bytes);
    free(r->id.bytes);
    free(r->string.bytes);
    free(r->comments.bytes);
    *r = (Reading){0};
}

// Ends the entry being read: the header, an entry, or nothing; 0 when it
// lacks its msgid or msgstr.
static int Finish(Reading* r, PoFile* po, PoError* error)
{
    if (!r->hasId && !r->hasString && !r->hasContext)
    {
        Discard(r);
        return 1;
    }
    if (!r->hasId || !r->hasString)
    {
        *error = (PoError){r->line,
                           r->hasId ? "an entry without its msgstr" : "an entry without its msgid"};
        Discard(r);
        return 0;
    }
    if (!r->hasContext && r->id.length == 0)
    {
        free(po->header);
        po->header = Take(&r->string);
        Discard(r);
        return 1;
    }
    po->entries = CatalogGrow(po->entries, po->count, sizeof(PoEntry));
    po->entries[po->count++] = (PoEntry){
        .context = r->hasContext ? Take(&r->context) : nullptr,
        .id = Take(&r->id),
        .string = Take(&r->string),
        .comments = r->comments.bytes != nullptr ? Take(&r->comments) : nullptr,
        .fuzzy = r->fuzzy,
        .mf2 = r->mf2,
        .line = r->line,
    };
    Discard(r);
    return 1;
}

// The flags of a "#," line.
static void Flags(Reading* r, const char* p, const char* end)
{
    while (p < end)
    {
        while (p < end && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\r'))
        {
            p++;
        }
        const char* start = p;
        while (p < end && *p != ',' && *p != ' ' && *p != '\t' && *p != '\r')
        {
            p++;
        }
        size_t n = (size_t)(p - start);
        r->fuzzy = r->fuzzy || (n == 5 && memcmp(start, "fuzzy", 5) == 0);
        r->mf2 = r->mf2 || (n == 10 && memcmp(start, "mf2-format", 10) == 0);
    }
}

static int StartsWith(const char* p, const char* end, const char* word)
{
    size_t n = strlen(word);
    return (size_t)(end - p) >= n && memcmp(p, word, n) == 0;
}

// One keyword line: msgctxt, msgid or msgstr and its string.
static const char* Keyword(Reading* r, const char* p, const char* end, size_t line)
{
    if (StartsWith(p, end, "msgid_plural") || StartsWith(p, end, "msgstr["))
    {
        return "a plural entry: MF2 selects inside the message";
    }
    Field field = StartsWith(p, end, "msgctxt")  ? fieldContext
                  : StartsWith(p, end, "msgid")  ? fieldId
                  : StartsWith(p, end, "msgstr") ? fieldString
                                                 : fieldNone;
    static const size_t s_lengths[] = {0, 7, 5, 6};
    Str* target = field == fieldContext ? &r->context : field == fieldId ? &r->id : &r->string;
    int* seen = field == fieldContext ? &r->hasContext
                : field == fieldId    ? &r->hasId
                                      : &r->hasString;
    if (field == fieldNone || *seen)
    {
        return field == fieldNone ? "a line the format does not have" : "a keyword given twice";
    }
    // An entry's line is its msgid's, and its first keyword's until then, so
    // a refusal of an entry without msgid names a line too.
    if (field == fieldId || (!r->hasContext && !r->hasId && !r->hasString))
    {
        r->line = line;
    }
    *seen = 1;
    r->field = field;
    return QuotedString(p + s_lengths[field], end, target);
}

// One line of the file.
// A comment line: extracted comments and flags kept, the others (the
// translator's, references, previous strings, obsolete entries) read
// past.
static void Comment(Reading* r, const char* p, const char* end)
{
    r->field = fieldNone;
    if (StartsWith(p, end, "#."))
    {
        const char* text = p + 2 + (end - p > 2 && p[2] == ' ');
        const char* stop = end > text && end[-1] == '\r' ? end - 1 : end;
        if (r->comments.bytes != nullptr)
        {
            Append(&r->comments, "\n", 1);
        }
        Append(&r->comments, text, (size_t)(stop - text));
    }
    else if (StartsWith(p, end, "#,"))
    {
        Flags(r, p + 2, end);
    }
}

// One line of the file.
static const char* Line(Reading* r, PoFile* po, const char* p, const char* end, size_t line,
                        PoError* error, int* failed)
{
    // A new entry begins at a blank line, a comment or a keyword after a
    // msgstr; a string line continues it.
    if (r->hasString && (p == end || *p == '\r' || *p == '#' || StartsWith(p, end, "msg")))
    {
        *failed = !Finish(r, po, error);
        if (*failed)
        {
            return nullptr;
        }
    }
    if (p == end || *p == '\r' || *p == '#')
    {
        if (p != end && *p == '#')
        {
            Comment(r, p, end);
        }
        r->field = fieldNone;
        return nullptr;
    }
    if (*p == '"')
    {
        Str* target = r->field == fieldContext  ? &r->context
                      : r->field == fieldId     ? &r->id
                      : r->field == fieldString ? &r->string
                                                : nullptr;
        return target != nullptr ? QuotedString(p, end, target) : "a string with no keyword";
    }
    return Keyword(r, p, end, line);
}

int PoRead(const char* text, size_t length, PoFile* out, PoError* error)
{
    *out = (PoFile){0};
    Reading r = {0};
    size_t line = 1;
    const char* p = text;
    const char* end = text + length;
    while (p < end)
    {
        const char* stop = memchr(p, '\n', (size_t)(end - p));
        stop = stop != nullptr ? stop : end;
        int failed = 0;
        const char* problem = Line(&r, out, p, stop, line, error, &failed);
        if (failed || problem != nullptr)
        {
            if (problem != nullptr)
            {
                *error = (PoError){line, problem};
            }
            Discard(&r);
            PoFree(out);
            return 0;
        }
        p = stop < end ? stop + 1 : end;
        line++;
    }
    if (!Finish(&r, out, error))
    {
        PoFree(out);
        return 0;
    }
    return 1;
}

void PoFree(PoFile* po)
{
    for (size_t i = 0; i < po->count; i++)
    {
        free(po->entries[i].context);
        free(po->entries[i].id);
        free(po->entries[i].string);
        free(po->entries[i].comments);
    }
    free(po->entries);
    free(po->header);
    *po = (PoFile){0};
}

// One byte of a string, escaped as gettext writes it.
static void PutByte(FILE* out, unsigned char c)
{
    const char* escape = c == '"'    ? "\\\""
                         : c == '\\' ? "\\\\"
                         : c == '\n' ? "\\n"
                         : c == '\t' ? "\\t"
                         : c == '\r' ? "\\r"
                                     : nullptr;
    if (escape != nullptr)
    {
        fputs(escape, out);
    }
    else if (c < 0x20 || c == 0x7F)
    {
        fprintf(out, "\\%03o", c);
    }
    else
    {
        fputc(c, out);
    }
}

// A string, split after each LF when one stands before its end.
static void WriteString(FILE* out, const char* keyword, const char* s)
{
    const char* lf = strchr(s, '\n');
    int split = lf != nullptr && lf[1] != '\0';
    fprintf(out, split ? "%s \"\"\n\"" : "%s \"", keyword);
    for (const char* p = s; *p != '\0'; p++)
    {
        PutByte(out, (unsigned char)*p);
        if (split && *p == '\n' && p[1] != '\0')
        {
            fputs("\"\n\"", out);
        }
    }
    fputs("\"\n", out);
}

void PoWrite(const PoFile* po, FILE* out)
{
    if (po->header != nullptr)
    {
        WriteString(out, "msgid", "");
        WriteString(out, "msgstr", po->header);
    }
    for (size_t i = 0; i < po->count; i++)
    {
        const PoEntry* e = &po->entries[i];
        fputc('\n', out);
        // Every line of the comments, an empty last one too, which the
        // reader keeps.
        for (const char* c = e->comments; c != nullptr;)
        {
            size_t n = strcspn(c, "\n");
            fprintf(out, n > 0 ? "#. %.*s\n" : "#.%.*s\n", (int)n, c);
            c = c[n] == '\n' ? c + n + 1 : nullptr;
        }
        if (e->fuzzy || e->mf2)
        {
            fprintf(out, "#,%s%s\n", e->fuzzy ? " fuzzy" : "",
                    e->mf2 ? (e->fuzzy ? ", mf2-format" : " mf2-format") : "");
        }
        if (e->context != nullptr)
        {
            WriteString(out, "msgctxt", e->context);
        }
        WriteString(out, "msgid", e->id);
        WriteString(out, "msgstr", e->string);
    }
}
