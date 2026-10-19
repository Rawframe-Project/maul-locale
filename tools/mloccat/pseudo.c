// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Pseudo-locales (pseudo.h).

#include "pseudo.h"

#include "catalog_set.h"
#include "resource_write.h"

#include "maul-locale/message.h"

#include <stdlib.h>
#include <string.h>

// The accented form of each ASCII letter, A to Z then a to z: one
// precomposed letter each, so the text stays in NFC.
static const char* const s_accented[52] = {
    "\xC3\x85",     "\xC6\x81", "\xC3\x87",     "\xC4\x90", "\xC3\x89", "\xC6\x91",     "\xC4\x9C",
    "\xC4\xA4",     "\xC3\x8E", "\xC4\xB4",     "\xC4\xB6", "\xC4\xBB", "\xE1\xB9\x80", "\xC3\x91",
    "\xC3\x96",     "\xC3\x9E", "\xC7\xAA",     "\xC5\x94", "\xC5\xA0", "\xC5\xA2",     "\xC3\x9B",
    "\xE1\xB9\xBC", "\xC5\xB4", "\xE1\xBA\x8A", "\xC3\x9D", "\xC5\xBD", "\xC3\xA5",     "\xC6\x80",
    "\xC3\xA7",     "\xC3\xB0", "\xC3\xA9",     "\xC6\x92", "\xC4\x9D", "\xC4\xA5",     "\xC3\xAE",
    "\xC4\xB5",     "\xC4\xB7", "\xC4\xBC",     "\xC9\xB1", "\xC3\xB1", "\xC3\xB6",     "\xC3\xBE",
    "\xC7\xAB",     "\xC5\x95", "\xC5\xA1",     "\xC5\xA3", "\xC3\xBB", "\xE1\xB9\xBD", "\xC5\xB5",
    "\xE1\xBA\x8B", "\xC3\xBD", "\xC5\xBE",
};

static const char s_dot[] = "\xC2\xB7";
static const char s_rlo[] = "\xE2\x80\xAE";
static const char s_pdf[] = "\xE2\x80\xAC";

// A growing text; exits when memory runs out.
typedef struct Text
{
    char* bytes;
    size_t length;
    size_t capacity;
} Text;

static void Put(Text* t, const char* bytes, size_t length)
{
    if (t->length + length + 1 > t->capacity)
    {
        size_t capacity = (t->length + length + 1) * 2;
        char* grown = realloc(t->bytes, capacity);
        if (grown == nullptr)
        {
            exit(1);
        }
        t->bytes = grown;
        t->capacity = capacity;
    }
    memcpy(t->bytes + t->length, bytes, length);
    t->length += length;
    t->bytes[t->length] = '\0';
}

// The length of MF2's optional space at at (o: s or bidi): space, tab,
// CR, LF, U+3000, ALM, LRM, RLM and the isolates LRI to PDI; 0 for none.
static size_t SpaceAt(const char* t, size_t n, size_t at)
{
    const unsigned char* u = (const unsigned char*)t + at;
    size_t left = n - at;
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

// The end of a pattern from at: the text's end, or the "}}" closing a
// quoted pattern; counts its text's code points.
static size_t PatternEnd(const char* t, size_t n, size_t at, int quoted, size_t* pointsOut)
{
    size_t points = 0;
    size_t i = at;
    while (i < n && !(quoted && t[i] == '}'))
    {
        if (t[i] == '{')
        {
            i = SkipBraces(t, n, i);
            continue;
        }
        size_t step = t[i] == '\\' && i + 1 < n ? 2 : 1;
        points += t[i] == '\\' || ((unsigned char)t[i] & 0xC0) != 0x80;
        i += step;
    }
    *pointsOut = points;
    return i;
}

// One character of pattern text, accented when an ASCII letter.
static void PutAccented(Text* out, char c)
{
    if (c >= 'A' && c <= 'Z')
    {
        Put(out, s_accented[c - 'A'], strlen(s_accented[c - 'A']));
    }
    else if (c >= 'a' && c <= 'z')
    {
        Put(out, s_accented[26 + c - 'a'], strlen(s_accented[26 + c - 'a']));
    }
    else
    {
        Put(out, &c, 1);
    }
}

// Writes the pattern from at to end in its pseudo form.
static void Pattern(const char* t, size_t at, size_t end, size_t points, PseudoKind kind, Text* out)
{
    int inRun = 0;
    if (kind == pseudoAccented)
    {
        Put(out, "[", 1);
    }
    for (size_t i = at; i < end;)
    {
        if (t[i] == '{')
        {
            size_t close = SkipBraces(t, end, i);
            if (inRun)
            {
                Put(out, s_pdf, sizeof(s_pdf) - 1);
                inRun = 0;
            }
            Put(out, t + i, close - i);
            i = close;
            continue;
        }
        if (kind == pseudoBidi && !inRun)
        {
            Put(out, s_rlo, sizeof(s_rlo) - 1);
            inRun = 1;
        }
        size_t step = t[i] == '\\' && i + 1 < end ? 2 : 1;
        if (step == 2 || kind == pseudoBidi)
        {
            Put(out, t + i, step);
        }
        else
        {
            PutAccented(out, t[i]);
        }
        i += step;
    }
    if (inRun)
    {
        Put(out, s_pdf, sizeof(s_pdf) - 1);
    }
    if (kind == pseudoAccented)
    {
        for (size_t p = 0; p < (points * 2 + 4) / 5; p++)
        {
            Put(out, s_dot, sizeof(s_dot) - 1);
        }
        Put(out, "]", 1);
    }
}

// A complex message: each quoted pattern in its pseudo form, the rest as
// written.
static void Complex(const char* t, size_t n, PseudoKind kind, Text* out)
{
    size_t i = 0;
    while (i < n)
    {
        if (t[i] == '{' && i + 1 < n && t[i + 1] == '{')
        {
            size_t points = 0;
            size_t end = PatternEnd(t, n, i + 2, 1, &points);
            Put(out, "{{", 2);
            Pattern(t, i + 2, end, points, kind, out);
            i = end;
            continue;
        }
        size_t next = i + 1;
        if (t[i] == '{')
        {
            next = SkipBraces(t, n, i);
        }
        else if (t[i] == '|')
        {
            next = i + 1;
            while (next < n && t[next] != '|')
            {
                next += t[next] == '\\' ? 2 : 1;
            }
            next = next < n ? next + 1 : n;
        }
        Put(out, t + i, next - i);
        i = next;
    }
}

static int Compiles(const char* text, size_t length)
{
    size_t size = 0;
    mlocTextResult r = mlocCompileMessage(text, length, nullptr, 0, &size, nullptr);
    return r.status == mloc_success || r.status == mloc_errorCapacity;
}

char* PseudoMessage(const char* text, size_t length, PseudoKind kind, size_t* lengthOut)
{
    if (!Compiles(text, length))
    {
        return nullptr;
    }
    Text out = {0};
    Put(&out, "", 0);
    size_t start = 0;
    for (size_t step = SpaceAt(text, length, 0); step > 0; step = SpaceAt(text, length, start))
    {
        start += step;
    }
    Put(&out, text, start);
    if (start < length && (text[start] == '.' ||
                           (text[start] == '{' && start + 1 < length && text[start + 1] == '{')))
    {
        Complex(text + start, length - start, kind, &out);
    }
    else
    {
        size_t points = 0;
        size_t end = PatternEnd(text, length, start, 0, &points);
        Pattern(text, start, end, points, kind, &out);
    }
    if (!Compiles(out.bytes, out.length))
    {
        free(out.bytes);
        return nullptr;
    }
    *lengthOut = out.length;
    return out.bytes;
}

// Gives each entry of the source its pseudo form, but the obsolete and
// those the source keeps; sets the locale. 0 when a message has none.
static int Transform(Resource* r, PseudoKind kind, const char* locale, FILE* report,
                     const char* path)
{
    for (size_t i = 0; i < r->metaCount; i++)
    {
        if (r->meta[i].key.length == 6 && memcmp(r->meta[i].key.bytes, "locale", 6) == 0)
        {
            free(r->meta[i].value.bytes);
            size_t length = strlen(locale);
            r->meta[i].value = (ResText){malloc(length + 1), length};
            if (r->meta[i].value.bytes == nullptr)
            {
                exit(1);
            }
            memcpy(r->meta[i].value.bytes, locale, length + 1);
        }
    }
    for (size_t s = 0; s < r->sectionCount; s++)
    {
        ResSection* section = &r->sections[s];
        int sectionKeeps =
            CatalogHasMeta(section->meta, section->metaCount, "translate", "no") ||
            CatalogHasMeta(section->meta, section->metaCount, "do-not-translate", nullptr);
        for (size_t k = 0; k < section->itemCount; k++)
        {
            ResItem* item = &section->items[k];
            if (item->kind != resEntry || sectionKeeps ||
                CatalogHasMeta(item->meta, item->metaCount, "obsolete", nullptr) ||
                CatalogHasMeta(item->meta, item->metaCount, "translate", "no") ||
                CatalogHasMeta(item->meta, item->metaCount, "do-not-translate", nullptr))
            {
                continue;
            }
            size_t length = 0;
            char* pseudo = PseudoMessage(item->value.bytes, item->value.length, kind, &length);
            if (pseudo == nullptr)
            {
                fprintf(report, "%s:%zu: no pseudo form\n", path, item->line);
                return 0;
            }
            free(item->value.bytes);
            item->value = (ResText){pseudo, length};
        }
    }
    return 1;
}

static int WritePseudo(const char* directory, const char* source, PseudoKind kind,
                       const char* locale, FILE* report)
{
    char* paths[] = {(char*)source};
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, 1, report) &&
             Transform(&set.inputs[0].resource, kind, locale, report, source);
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.mf2", directory, locale);
    FILE* f = ok ? fopen(path, "wb") : nullptr;
    if (ok && f == nullptr)
    {
        fprintf(report, "%s: cannot write\n", path);
        ok = 0;
    }
    if (f != nullptr)
    {
        ResourceWrite(&set.inputs[0].resource, f);
        ok = fclose(f) == 0;
        fprintf(report, ok ? "%s: written\n" : "%s: cannot write\n", path);
    }
    CatalogSetFree(&set);
    return ok;
}

int CatalogPseudo(const char* directory, const char* source, FILE* report)
{
    return WritePseudo(directory, source, pseudoAccented, "en-XA", report) &&
           WritePseudo(directory, source, pseudoBidi, "ar-XB", report);
}
