// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Pseudo-locales (pseudo.h).

#include "pseudo.h"

#include "catalog_set.h"
#include "mf2_parts.h"
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

// The code points of a text part, an escape counted as one.
static size_t Points(const char* t, size_t length)
{
    size_t points = 0;
    for (size_t i = 0; i < length; i++)
    {
        points += ((unsigned char)t[i] & 0xC0) != 0x80;
        i += t[i] == '\\';
    }
    return points;
}

// A text part in its pseudo form: letters accented, escapes kept; or the
// run between RLO and PDF.
static void PutText(Text* out, const char* t, size_t length, PseudoKind kind)
{
    if (kind == pseudoBidi)
    {
        Put(out, s_rlo, sizeof(s_rlo) - 1);
        Put(out, t, length);
        Put(out, s_pdf, sizeof(s_pdf) - 1);
        return;
    }
    for (size_t i = 0; i < length; i++)
    {
        if (t[i] == '\\' && i + 1 < length)
        {
            Put(out, t + i, 2);
            i++;
        }
        else
        {
            PutAccented(out, t[i]);
        }
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
    Mf2Part* parts = nullptr;
    size_t count = 0;
    if (!Mf2Split(text, length, &parts, &count))
    {
        return nullptr;
    }
    Text out = {0};
    Put(&out, "", 0);
    size_t points = 0;
    for (size_t i = 0; i < count; i++)
    {
        const char* at = text + parts[i].start;
        if (parts[i].kind == mf2PatternStart && kind == pseudoAccented)
        {
            Put(&out, "[", 1);
            points = 0;
        }
        else if (parts[i].kind == mf2PatternEnd && kind == pseudoAccented)
        {
            for (size_t p = 0; p < (points * 2 + 4) / 5; p++)
            {
                Put(&out, s_dot, sizeof(s_dot) - 1);
            }
            Put(&out, "]", 1);
        }
        else if (parts[i].kind == mf2Text)
        {
            points += Points(at, parts[i].length);
            PutText(&out, at, parts[i].length, kind);
        }
        else
        {
            Put(&out, at, parts[i].length);
        }
    }
    free(parts);
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
