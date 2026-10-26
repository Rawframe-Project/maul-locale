// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// PO exchange (exchange_po.h).

#include "exchange_po.h"

#include "catalog_set.h"
#include "file.h"
#include "message_text.h"
#include "po.h"
#include "print.h"
#include "resource_edit.h"
#include "stale.h"

#include <stdlib.h>
#include <string.h>

static char* Copy(const char* bytes, size_t length)
{
    char* copy = malloc(length + 1);
    if (copy == nullptr)
    {
        exit(1);
    }
    memcpy(copy, bytes, length);
    copy[length] = '\0';
    return copy;
}

// The source entry's comment and its @param lines, joined by LF; null
// for none.
static char* Comments(const CatalogEntry* e)
{
    size_t length = e->item->comment.length;
    for (size_t i = 0; i < e->item->metaCount; i++)
    {
        length += e->item->meta[i].value.length + 9;
    }
    char* text = malloc(length + 1);
    if (text == nullptr)
    {
        exit(1);
    }
    size_t at = 0;
    memcpy(text, e->item->comment.bytes, e->item->comment.length);
    at += e->item->comment.length;
    for (size_t i = 0; i < e->item->metaCount; i++)
    {
        const ResMeta* m = &e->item->meta[i];
        if (m->key.length == 5 && memcmp(m->key.bytes, "param", 5) == 0)
        {
            // The length counted above holds every line.
            if (!TextPrint(text, length + 1, &at, "%s@param %.*s", at > 0 ? "\n" : "",
                           (int)m->value.length, m->value.bytes))
            {
                exit(1);
            }
        }
    }
    text[at] = '\0';
    if (at == 0)
    {
        free(text);
        return nullptr;
    }
    return text;
}

static char* Header(const char* source, const char* locale)
{
    const char* name = strrchr(source, '/');
    name = name != nullptr ? name + 1 : source;
    char head[1024];
    snprintf(head, sizeof(head),
             "Project-Id-Version: %s\nPO-Revision-Date: \nLast-Translator: \nLanguage-Team: \n"
             "Language: %s\nMIME-Version: 1.0\nContent-Type: text/plain; charset=UTF-8\n"
             "Content-Transfer-Encoding: 8bit\nX-Generator: mloccat\n",
             name, locale);
    return Copy(head, strlen(head));
}

int CatalogExportPo(const char* source, const char* translation, const char* po, FILE* report)
{
    char* paths[] = {(char*)source, (char*)translation};
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, 2, report);
    PoFile file = {0};
    if (ok)
    {
        const CatalogInput* s = &set.inputs[0];
        const CatalogInput* t = &set.inputs[1];
        file.header = Header(source, t->locale);
        for (uint32_t i = 0; i < s->count; i++)
        {
            const CatalogEntry* e = &s->entries[i];
            if (e->keep)
            {
                continue;
            }
            const CatalogEntry* done = CatalogFindEntry(t, e->key);
            file.entries = CatalogGrow(file.entries, file.count, sizeof(PoEntry));
            file.entries[file.count++] = (PoEntry){
                .context = Copy(e->key, e->length),
                .id = Copy(e->item->value.bytes, e->item->value.length),
                .string = done != nullptr ? Copy(done->item->value.bytes, done->item->value.length)
                                          : Copy("", 0),
                .comments = Comments(e),
                .fuzzy = done != nullptr && SourceStateOf(done->item->meta, done->item->metaCount,
                                                          &e->item->value) == sourceStale,
                .mf2 = 1,
            };
        }
        FILE* out = fopen(po, "wb");
        if (out != nullptr)
        {
            PoWrite(&file, out);
        }
        ok = out != nullptr && fclose(out) == 0;
        fprintf(report, ok ? "%s: %zu entries exported\n" : "%s: cannot write\n", po, file.count);
    }
    PoFree(&file);
    CatalogSetFree(&set);
    return ok;
}

// An imported translation: the source's entry and the PO entry.
typedef struct Import
{
    const CatalogEntry* source;
    const PoEntry* entry;
} Import;

// The entries to import: each translated, of a key the source asks to
// translate, compiling; 0 for an entry without msgctxt or one that does
// not compile.
static int Choose(const CatalogInput* s, const PoFile* file, const char* po, Import** out,
                  size_t* count, FILE* report)
{
    int ok = 1;
    for (size_t i = 0; i < file->count; i++)
    {
        const PoEntry* e = &file->entries[i];
        const CatalogEntry* key = e->context != nullptr ? CatalogFindEntry(s, e->context) : nullptr;
        if (e->context == nullptr)
        {
            fprintf(report, "%s:%zu: an entry without msgctxt, the key\n", po, e->line);
            ok = 0;
        }
        else if (key == nullptr || key->keep)
        {
            fprintf(report, "%s:%zu: not a key to translate: %s\n", po, e->line, e->context);
        }
        else if (e->string[0] != '\0' && !MessageCompiles(e->string, strlen(e->string)))
        {
            fprintf(report, "%s:%zu: a translation that does not compile: %s\n", po, e->line,
                    e->context);
            ok = 0;
        }
        else if (e->string[0] != '\0')
        {
            *out = CatalogGrow(*out, *count, sizeof(Import));
            (*out)[(*count)++] = (Import){key, e};
        }
    }
    return ok;
}

// Puts the imports into the translation; the hash of the source each
// answers recorded unless fuzzy.
static void Apply(Resource* r, const Import* imports, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const PoEntry* e = imports[i].entry;
        ResItem* item =
            ResourceEntryFor(r, &imports[i].source->section->id, &imports[i].source->item->id);
        ResourceSetValue(item, e->string, strlen(e->string));
        uint64_t hash = 0;
        if (!e->fuzzy && SourceHash(e->id, strlen(e->id), &hash))
        {
            char text[sourceHashDigits + 1];
            SourceHashText(hash, text);
            ResourceSetMeta(item, "mloc-source", text);
        }
    }
}

int CatalogImportPo(const char* source, const char* translation, const char* po, FILE* report)
{
    char* paths[] = {(char*)source, (char*)translation};
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, 2, report);
    size_t length = 0;
    char* text = ok ? FileRead(po, &length) : nullptr;
    PoFile file = {0};
    PoError error = {0, ""};
    if (ok && (text == nullptr || !PoRead(text, length, &file, &error)))
    {
        if (text == nullptr)
        {
            fprintf(report, "%s: cannot read\n", po);
        }
        else
        {
            fprintf(report, "%s:%zu: %s\n", po, error.line, error.reason);
        }
        ok = 0;
    }
    Import* imports = nullptr;
    size_t count = 0;
    ok = ok && Choose(&set.inputs[0], &file, po, &imports, &count, report);
    if (ok)
    {
        Apply(&set.inputs[1].resource, imports, count);
        ok = ResourceWriteFile(translation, &set.inputs[1].resource);
        fprintf(report, ok ? "%s: %zu entries imported\n" : "%s: cannot write\n", translation,
                count);
    }
    free(imports);
    PoFree(&file);
    free(text);
    CatalogSetFree(&set);
    return ok;
}
