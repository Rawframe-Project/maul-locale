// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog source changed by the tool (resource_edit.h).

#include "resource_edit.h"

#include "catalog_set.h"
#include "resource_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#endif

static ResText CopyText(const char* text, size_t length)
{
    char* bytes = malloc(length + 1);
    if (bytes == nullptr)
    {
        exit(1);
    }
    memcpy(bytes, text, length);
    bytes[length] = '\0';
    return (ResText){bytes, length};
}

void ResourceSetMeta(ResItem* item, const char* key, const char* value)
{
    ResMeta* m = (ResMeta*)CatalogFindMeta(item->meta, item->metaCount, key);
    if (m == nullptr)
    {
        item->meta = CatalogGrow(item->meta, item->metaCount, sizeof(ResMeta));
        m = &item->meta[item->metaCount++];
        m->key = CopyText(key, strlen(key));
    }
    else
    {
        free(m->value.bytes);
    }
    m->value = CopyText(value, strlen(value));
}

void ResourceSetValue(ResItem* item, const char* text, size_t length)
{
    free(item->value.bytes);
    item->value = CopyText(text, length);
}

static int SameId(const ResId* a, const ResId* b)
{
    if (a->count != b->count)
    {
        return 0;
    }
    for (size_t i = 0; i < a->count; i++)
    {
        if (a->parts[i].length != b->parts[i].length ||
            memcmp(a->parts[i].bytes, b->parts[i].bytes, a->parts[i].length) != 0)
        {
            return 0;
        }
    }
    return 1;
}

static ResId CopyId(const ResId* id)
{
    ResId copy = {calloc(id->count + 1, sizeof(ResText)), id->count};
    if (copy.parts == nullptr)
    {
        exit(1);
    }
    for (size_t i = 0; i < id->count; i++)
    {
        copy.parts[i] = CopyText(id->parts[i].bytes, id->parts[i].length);
    }
    return copy;
}

ResItem* ResourceEntryFor(Resource* r, const ResId* section, const ResId* entry)
{
    ResSection* s = nullptr;
    for (size_t i = 0; s == nullptr && i < r->sectionCount; i++)
    {
        s = SameId(&r->sections[i].id, section) ? &r->sections[i] : nullptr;
    }
    if (s == nullptr)
    {
        r->sections = CatalogGrow(r->sections, r->sectionCount, sizeof(ResSection));
        s = &r->sections[r->sectionCount++];
        *s = (ResSection){.id = CopyId(section)};
    }
    for (size_t i = 0; i < s->itemCount; i++)
    {
        if (s->items[i].kind == resEntry && SameId(&s->items[i].id, entry))
        {
            return &s->items[i];
        }
    }
    s->items = CatalogGrow(s->items, s->itemCount, sizeof(ResItem));
    ResItem* item = &s->items[s->itemCount++];
    *item = (ResItem){.kind = resEntry, .id = CopyId(entry), .value = CopyText("", 0)};
    return item;
}

int ResourceWriteFile(const char* path, const Resource* r)
{
    char temporary[4096];
    if ((size_t)snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= sizeof(temporary))
    {
        return 0;
    }
    FILE* f = fopen(temporary, "wb");
    if (f == nullptr)
    {
        return 0;
    }
    ResourceWrite(r, f);
    int ok = fclose(f) == 0;
#ifdef _WIN32
    // rename does not replace a file there; this does, at once.
    ok = ok && MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    ok = ok && rename(temporary, path) == 0;
#endif
    if (!ok)
    {
        (void)remove(temporary);
    }
    return ok;
}
