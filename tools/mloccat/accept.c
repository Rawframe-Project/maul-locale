// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A translation accepted (accept.h).

#include "accept.h"

#include "catalog_set.h"
#include "resource_check.h"
#include "resource_edit.h"
#include "stale.h"

#include <stdlib.h>
#include <string.h>

// The source's entries by key, for a binary search.
typedef struct Hashed
{
    const CatalogEntry* entry;
    uint64_t hash;
} Hashed;

static int CompareHashed(const void* a, const void* b)
{
    const CatalogEntry* x = ((const Hashed*)a)->entry;
    const CatalogEntry* y = ((const Hashed*)b)->entry;
    return strcmp(x->key, y->key);
}

static const Hashed* FindSource(const Hashed* hashed, size_t count, const char* key)
{
    CatalogEntry probe = {.key = (char*)key};
    Hashed wanted = {&probe, 0};
    return count == 0 ? nullptr : bsearch(&wanted, hashed, count, sizeof(Hashed), CompareHashed);
}

static int Named(char* const* keys, size_t keyCount, const char* key)
{
    for (size_t i = 0; i < keyCount; i++)
    {
        if (strcmp(keys[i], key) == 0)
        {
            return 1;
        }
    }
    return keyCount == 0;
}

// Records the hashes in the translation's entries; the count, and the
// keys named that were found, counted in found.
static size_t RecordAll(Resource* r, const Hashed* hashed, size_t hashedCount, char* const* keys,
                        size_t keyCount, int* found)
{
    size_t accepted = 0;
    for (size_t s = 0; s < r->sectionCount; s++)
    {
        ResSection* section = &r->sections[s];
        for (size_t i = 0; i < section->itemCount; i++)
        {
            ResItem* item = &section->items[i];
            char key[keyRoom];
            if (item->kind != resEntry ||
                CatalogHasMeta(item->meta, item->metaCount, "obsolete", nullptr) ||
                !ResourceKey(&section->id, &item->id, key, sizeof(key)) ||
                !Named(keys, keyCount, key))
            {
                continue;
            }
            const Hashed* h = FindSource(hashed, hashedCount, key);
            if (h != nullptr && !h->entry->keep)
            {
                char text[sourceHashDigits + 1];
                SourceHashText(h->hash, text);
                ResourceSetMeta(item, "mloc-source", text);
                accepted++;
                for (size_t k = 0; k < keyCount; k++)
                {
                    found[k] = found[k] || strcmp(keys[k], key) == 0;
                }
            }
        }
    }
    return accepted;
}

int CatalogAccept(const char* source, const char* translation, char* const* keys, size_t keyCount,
                  FILE* report)
{
    char* paths[] = {(char*)source, (char*)translation};
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, 2, report);
    const CatalogInput* in = &set.inputs[0];
    Hashed* hashed = calloc(in->count + (size_t)1, sizeof(Hashed));
    int* found = calloc(keyCount + 1, sizeof(int));
    if (hashed == nullptr || found == nullptr)
    {
        exit(1);
    }
    for (uint32_t i = 0; ok && i < in->count; i++)
    {
        const ResText* value = &in->entries[i].item->value;
        hashed[i].entry = &in->entries[i];
        ok = SourceHash(value->bytes, value->length, &hashed[i].hash);
    }
    if (ok)
    {
        qsort(hashed, in->count, sizeof(Hashed), CompareHashed);
        size_t accepted =
            RecordAll(&set.inputs[1].resource, hashed, in->count, keys, keyCount, found);
        for (size_t k = 0; k < keyCount; k++)
        {
            if (!found[k])
            {
                fprintf(report, "%s: no entry to accept: %s\n", translation, keys[k]);
                ok = 0;
            }
        }
        if (ok && !ResourceWriteFile(translation, &set.inputs[1].resource))
        {
            fprintf(report, "%s: cannot write\n", translation);
            ok = 0;
        }
        if (ok)
        {
            fprintf(report, "%s: %zu entries accepted\n", translation, accepted);
        }
    }
    free(found);
    free(hashed);
    CatalogSetFree(&set);
    return ok;
}
