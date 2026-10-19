// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog set read (catalog_set.h).

#include "catalog_set.h"

#include "file.h"
#include "resource_check.h"

#include "maul-locale/locale.h"

#include <stdlib.h>
#include <string.h>

void* CatalogGrow(void* items, size_t count, size_t size)
{
    void* grown = realloc(items, (count + 1) * size);
    if (grown == nullptr)
    {
        fputs("mloccat: out of memory\n", stderr);
        exit(1);
    }
    return grown;
}

int CatalogHasMeta(const ResMeta* meta, size_t count, const char* key, const char* value)
{
    for (size_t i = 0; i < count; i++)
    {
        size_t length = strlen(key);
        if (meta[i].key.length == length && memcmp(meta[i].key.bytes, key, length) == 0 &&
            (value == nullptr || (meta[i].value.length == strlen(value) &&
                                  memcmp(meta[i].value.bytes, value, strlen(value)) == 0)))
        {
            return 1;
        }
    }
    return 0;
}

static int KeepsSource(const ResSection* s, const ResItem* item)
{
    return CatalogHasMeta(item->meta, item->metaCount, "translate", "no") ||
           CatalogHasMeta(item->meta, item->metaCount, "do-not-translate", nullptr) ||
           CatalogHasMeta(s->meta, s->metaCount, "translate", "no") ||
           CatalogHasMeta(s->meta, s->metaCount, "do-not-translate", nullptr);
}

static void CollectEntries(CatalogInput* in)
{
    for (size_t s = 0; s < in->resource.sectionCount; s++)
    {
        const ResSection* section = &in->resource.sections[s];
        for (size_t i = 0; i < section->itemCount; i++)
        {
            const ResItem* item = &section->items[i];
            char key[keyRoom];
            if (item->kind != resEntry ||
                CatalogHasMeta(item->meta, item->metaCount, "obsolete", nullptr) ||
                !ResourceKey(&section->id, &item->id, key, sizeof(key)))
            {
                continue;
            }
            in->entries = CatalogGrow(in->entries, in->count, sizeof(in->entries[0]));
            CatalogEntry* e = &in->entries[in->count++];
            e->length = strlen(key);
            e->key = malloc(e->length + 1);
            if (e->key == nullptr)
            {
                exit(1);
            }
            memcpy(e->key, key, e->length + 1);
            e->item = item;
            e->section = section;
            e->keep = KeepsSource(section, item);
        }
    }
}

// The locale's canonical id, from @locale (which the check parsed).
static void CanonicalLocale(CatalogInput* in)
{
    const ResMeta* m = nullptr;
    for (size_t i = 0; i < in->resource.metaCount; i++)
    {
        if (in->resource.meta[i].key.length == 6 &&
            memcmp(in->resource.meta[i].key.bytes, "locale", 6) == 0)
        {
            m = &in->resource.meta[i];
        }
    }
    mlocLocale locale;
    mlocOutput out = {in->locale, sizeof(in->locale) - 1, 0, nullptr, nullptr};
    if (m != nullptr &&
        mlocParseLocale(m->value.bytes, m->value.length, &locale).status == mloc_success &&
        mlocFormatLocale(&locale, &out) == mloc_success)
    {
        in->locale[out.length] = '\0';
    }
}

static int CompareEntries(const void* a, const void* b)
{
    return strcmp((*(const CatalogEntry* const*)a)->key, (*(const CatalogEntry* const*)b)->key);
}

static void IndexEntries(CatalogInput* in)
{
    in->byKey = (const CatalogEntry**)calloc(in->count + (size_t)1, sizeof(CatalogEntry*));
    if (in->byKey == nullptr)
    {
        exit(1);
    }
    for (uint32_t i = 0; i < in->count; i++)
    {
        in->byKey[i] = &in->entries[i];
    }
    qsort((void*)in->byKey, in->count, sizeof(CatalogEntry*), CompareEntries);
}

const CatalogEntry* CatalogFindEntry(const CatalogInput* in, const char* key)
{
    CatalogEntry probe = {.key = (char*)key};
    const CatalogEntry* wanted = &probe;
    const CatalogEntry** found =
        in->count == 0
            ? nullptr
            : (const CatalogEntry**)bsearch((const void*)&wanted, (const void*)in->byKey, in->count,
                                            sizeof(CatalogEntry*), CompareEntries);
    return found != nullptr ? *found : nullptr;
}

static int ReadInput(CatalogInput* in, const char* path, FILE* report)
{
    *in = (CatalogInput){.path = path};
    size_t length = 0;
    in->text = FileRead(path, &length);
    if (in->text == nullptr)
    {
        fprintf(report, "%s: cannot read\n", path);
        return 0;
    }
    ResError error;
    if (!ResourceRead(in->text, length, &in->resource, &error))
    {
        fprintf(report, "%s:%zu:%zu: %s\n", path, error.line, error.column, error.reason);
        return 0;
    }
    if (!ResourceCheck(&in->resource, path, report))
    {
        return 0;
    }
    CollectEntries(in);
    CanonicalLocale(in);
    IndexEntries(in);
    return in->locale[0] != '\0';
}

static void FreeInput(CatalogInput* in)
{
    for (uint32_t i = 0; i < in->count; i++)
    {
        free(in->entries[i].key);
    }
    free(in->entries);
    free((void*)in->byKey);
    ResourceFree(&in->resource);
    free(in->text);
}

const ResMeta* CatalogFindMeta(const ResMeta* meta, size_t count, const char* key)
{
    size_t length = strlen(key);
    for (size_t i = 0; i < count; i++)
    {
        if (meta[i].key.length == length && memcmp(meta[i].key.bytes, key, length) == 0)
        {
            return &meta[i];
        }
    }
    return nullptr;
}

int CatalogSetRead(CatalogSet* set, char* const* paths, size_t count, FILE* report)
{
    set->inputs = calloc(count + 1, sizeof(CatalogInput));
    set->count = count;
    if (set->inputs == nullptr)
    {
        exit(1);
    }
    int ok = count > 0;
    for (size_t i = 0; i < count; i++)
    {
        ok = ReadInput(&set->inputs[i], paths[i], report) && ok;
        for (size_t j = 0; ok && j < i; j++)
        {
            if (strcmp(set->inputs[i].locale, set->inputs[j].locale) == 0)
            {
                fprintf(report, "%s: a second catalog of %s\n", paths[i], set->inputs[i].locale);
                ok = 0;
            }
        }
    }
    return ok;
}

void CatalogSetFree(CatalogSet* set)
{
    for (size_t i = 0; i < set->count; i++)
    {
        FreeInput(&set->inputs[i]);
    }
    free(set->inputs);
    *set = (CatalogSet){0};
}
