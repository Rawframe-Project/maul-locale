// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog compiler (compile.h).

#include "compile.h"

#include "asset.h"
#include "catalog_set.h"
#include "catalog_tables.h"
#include "file.h"
#include "stale.h"
#include "table.h"

#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// A growing byte buffer for a unit.
typedef struct Bytes
{
    uint8_t* data;
    size_t size;
} Bytes;

static void Put(Bytes* b, const void* data, size_t size)
{
    uint8_t* grown = realloc(b->data, b->size + size + 1);
    if (grown == nullptr)
    {
        exit(1);
    }
    b->data = grown;
    if (size > 0)
    {
        memcpy(b->data + b->size, data, size);
    }
    b->size += size;
}

static void Put32(Bytes* b, uint32_t v)
{
    uint8_t bytes[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)};
    Put(b, bytes, 4);
}

static void Put64(Bytes* b, uint64_t v)
{
    Put32(b, (uint32_t)v);
    Put32(b, (uint32_t)(v >> 32));
}

static void Set64(Bytes* b, size_t at, uint64_t v)
{
    for (size_t i = 0; i < 8; i++)
    {
        b->data[at + i] = (uint8_t)(v >> (8 * i));
    }
}

// The keys unit, the keys in index order, its set id filled in.
static Bytes KeysUnit(const CatalogInput* source, const KeyTable* t, uint32_t* byIndex)
{
    Bytes b = {0};
    Put32(&b, t->count);
    Put32(&b, t->buckets);
    Put32(&b, t->places);
    Put32(&b, 0);
    Put64(&b, t->seed);
    Put64(&b, 0);
    for (uint32_t i = 0; i < t->buckets; i++)
    {
        uint8_t pilot[2] = {(uint8_t)t->pilots[i], (uint8_t)(t->pilots[i] >> 8)};
        Put(&b, pilot, 2);
    }
    static const uint8_t s_zero[4] = {0};
    Put(&b, s_zero, (4 - b.size % 4) % 4);
    for (uint32_t i = 0; i < t->places - t->count; i++)
    {
        Put32(&b, t->remap[i]);
    }
    for (uint32_t i = 0; i < t->count; i++)
    {
        byIndex[t->index[i]] = i;
    }
    uint32_t offset = 0;
    for (uint32_t i = 0; i <= t->count; i++)
    {
        Put32(&b, offset);
        offset += i < t->count ? (uint32_t)source->entries[byIndex[i]].length : 0;
    }
    for (uint32_t i = 0; i < t->count; i++)
    {
        Put(&b, source->entries[byIndex[i]].key, source->entries[byIndex[i]].length);
    }
    CatalogKeys view;
    if (mlocReadCatalogKeys((DataSpan){b.data, (uint32_t)b.size}, &view))
    {
        Set64(&b, 24, mlocCatalogSetId(&view));
    }
    return b;
}

// A message compiled into its own allocation; size 0 when it does not.
static Bytes CompileMessage(const ResText* value)
{
    Bytes b = {0};
    size_t size = 0;
    mlocTextResult r = mlocCompileMessage(value->bytes, value->length, nullptr, 0, &size, nullptr);
    if (r.status != mloc_errorCapacity && r.status != mloc_success)
    {
        return b;
    }
    b.data = malloc(size == 0 ? 1 : size);
    if (b.data == nullptr ||
        mlocCompileMessage(value->bytes, value->length, b.data, size, &b.size, nullptr).status !=
            mloc_success)
    {
        free(b.data);
        return (Bytes){0};
    }
    return b;
}

// The index of a key in the source's table, or UINT32_MAX.
static uint32_t IndexOf(const CatalogKeys* keys, const char* key, size_t length)
{
    if (keys->count == 0)
    {
        return UINT32_MAX;
    }
    uint32_t index = mlocCatalogIndexOf(keys, (const uint8_t*)key, length);
    uint32_t stored = 0;
    const uint8_t* found = mlocCatalogKeyAt(keys, index, &stored);
    return stored == length && memcmp(found, key, length) == 0 ? index : UINT32_MAX;
}

typedef struct Messages
{
    const ResText** values;
    uint32_t orphans;
    uint32_t present;
    uint32_t stale;
} Messages;

// Each index's message text for one input: its own, the source's where
// the source keeps it, none where it has none.
static Messages ChooseMessages(const CatalogInput* in, const CatalogInput* source,
                               const CatalogKeys* keys, FILE* report)
{
    Messages m = {(const ResText**)calloc(keys->count == 0 ? 1 : keys->count, sizeof(ResText*)), 0,
                  0, 0};
    const ResText** sources =
        (const ResText**)calloc(keys->count == 0 ? 1 : keys->count, sizeof(ResText*));
    if (m.values == nullptr || sources == nullptr)
    {
        exit(1);
    }
    for (uint32_t i = 0; i < source->count; i++)
    {
        uint32_t index = IndexOf(keys, source->entries[i].key, source->entries[i].length);
        sources[index] = &source->entries[i].item->value;
    }
    for (uint32_t i = 0; i < in->count; i++)
    {
        uint32_t index = IndexOf(keys, in->entries[i].key, in->entries[i].length);
        if (index == UINT32_MAX)
        {
            fprintf(report, "%s:%zu: a key the source does not have: %s\n", in->path,
                    in->entries[i].item->line, in->entries[i].key);
            m.orphans++;
            continue;
        }
        m.values[index] = &in->entries[i].item->value;
        m.stale +=
            in != source && SourceStateOf(in->entries[i].item->meta, in->entries[i].item->metaCount,
                                          sources[index]) == sourceStale;
    }
    free((void*)sources);
    for (uint32_t i = 0; i < source->count; i++)
    {
        if (source->entries[i].keep)
        {
            uint32_t index = IndexOf(keys, source->entries[i].key, source->entries[i].length);
            m.values[index] = &source->entries[i].item->value;
        }
    }
    return m;
}

static Bytes MessagesUnit(const CatalogInput* in, const CatalogInput* source,
                          const CatalogKeys* keys, FILE* report, int* okOut)
{
    Messages m = ChooseMessages(in, source, keys, report);
    Bytes head = {0};
    Bytes text = {0};
    Put32(&head, keys->count);
    Put32(&head, 0);
    Put64(&head, keys->setId);
    for (uint32_t i = 0; i < keys->count; i++)
    {
        Put32(&head, (uint32_t)text.size);
        if (m.values[i] == nullptr)
        {
            continue;
        }
        Bytes compiled = CompileMessage(m.values[i]);
        *okOut = *okOut && compiled.size > 0;
        Put(&text, compiled.data, compiled.size);
        free(compiled.data);
        m.present++;
    }
    Put32(&head, (uint32_t)text.size);
    Put(&head, text.data, text.size);
    free(text.data);
    fprintf(report, "%s: %s, %u of %u messages, %u keys left out, %u stale\n", in->path, in->locale,
            m.present, keys->count, m.orphans, m.stale);
    free((void*)m.values);
    return head;
}

// A field of the catalog: its tag, its length, the payload, and zero bytes
// to the next multiple of 8.
static void PutField(Bytes* b, uint32_t tag, const void* data, size_t size)
{
    static const uint8_t zeros[assetAlignment] = {0};
    Put32(b, tag);
    Put32(b, (uint32_t)size);
    Put(b, data, size);
    Put(b, zeros, (assetAlignment - size % assetAlignment) % assetAlignment);
}

// The catalog (catalog_tables.h): the header, then its locale, keys and
// messages.
static Bytes CatalogBytes(const CatalogInput* in, bool source, const Bytes* keys,
                          const Bytes* messages)
{
    Bytes b = {0};
    Put(&b, MLOC_CATALOG_MAGIC, 4);
    uint8_t versionAndFlags[4] = {(uint8_t)catalogFormatVersion, 0,
                                  (uint8_t)(source ? catalogSourceFlag : 0), 0};
    Put(&b, versionAndFlags, 4);
    Put32(&b, 0);
    Put32(&b, catalogFieldCount);
    PutField(&b, catalogTags[catalogLocale], in->locale, strlen(in->locale));
    PutField(&b, catalogTags[catalogKeys], keys->data, keys->size);
    PutField(&b, catalogTags[catalogMessages], messages->data, messages->size);
    for (size_t i = 0; i < 4; i++)
    {
        b.data[assetSize + i] = (uint8_t)(b.size >> (8 * i));
    }
    return b;
}

static int WriteCatalog(const char* directory, const CatalogInput* in, bool source,
                        const Bytes* keys, const Bytes* messages, FILE* report)
{
    Bytes bytes = CatalogBytes(in, source, keys, messages);
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.mlct", directory, in->locale);
    int ok = FileWrite(path, bytes.data, bytes.size);
    if (!ok)
    {
        fprintf(report, "%s: cannot write\n", path);
    }
    free(bytes.data);
    return ok;
}

// The keys of the source, as the table builder takes them.
static int BuildTable(const CatalogInput* source, KeyTable* table)
{
    const char** keys = (const char**)calloc(source->count + (size_t)1, sizeof(char*));
    size_t* lengths = calloc(source->count + (size_t)1, sizeof(size_t));
    if (keys == nullptr || lengths == nullptr)
    {
        exit(1);
    }
    for (uint32_t i = 0; i < source->count; i++)
    {
        keys[i] = source->entries[i].key;
        lengths[i] = source->entries[i].length;
    }
    int ok = KeyTableBuild(keys, lengths, source->count, table);
    free((void*)keys);
    free(lengths);
    return ok;
}

static int CompileAll(const char* directory, CatalogInput* inputs, size_t count, FILE* report)
{
    KeyTable table;
    if (!BuildTable(&inputs[0], &table))
    {
        fprintf(report, "%s: no perfect hash found for its keys\n", inputs[0].path);
        return 0;
    }
    uint32_t* byIndex = calloc(table.count + (size_t)1, sizeof(uint32_t));
    if (byIndex == nullptr)
    {
        exit(1);
    }
    Bytes keys = KeysUnit(&inputs[0], &table, byIndex);
    CatalogKeys view;
    int ok = mlocReadCatalogKeys((DataSpan){keys.data, (uint32_t)keys.size}, &view);
    for (size_t i = 0; ok && i < count; i++)
    {
        Bytes messages = MessagesUnit(&inputs[i], &inputs[0], &view, report, &ok);
        ok = ok && WriteCatalog(directory, &inputs[i], i == 0, &keys, &messages, report);
        free(messages.data);
    }
    free(keys.data);
    free(byIndex);
    KeyTableFree(&table);
    return ok;
}

int CatalogCompile(const char* directory, char* const* paths, size_t count, FILE* report)
{
    CatalogSet set;
    int ok = CatalogSetRead(&set, paths, count, report);
    ok = ok && CompileAll(directory, set.inputs, count, report);
    CatalogSetFree(&set);
    return ok;
}
