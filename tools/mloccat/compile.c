// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog compiler (compile.h).

#include "compile.h"

#include "catalog_tables.h"
#include "data_format.h"
#include "datafile.h"
#include "file.h"
#include "kinds.h"
#include "resource.h"
#include "resource_check.h"
#include "table.h"

#include "maul-locale/locale.h"
#include "maul-locale/message.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum
{
    keyRoom = 1024,
    localeRoom = 128,
};

// An entry that compiles: its key and the item it came from.
typedef struct Entry
{
    char* key;
    size_t length;
    const ResItem* item;
    // Whether it or its section says @translate no.
    int keep;
} Entry;

typedef struct Input
{
    const char* path;
    char* text;
    Resource resource;
    Entry* entries;
    uint32_t count;
    char locale[localeRoom];
} Input;

static void* Grow(void* items, size_t count, size_t size)
{
    void* grown = realloc(items, (count + 1) * size);
    if (grown == nullptr)
    {
        fputs("mloccat: out of memory\n", stderr);
        exit(1);
    }
    return grown;
}

static int HasMeta(const ResMeta* meta, size_t count, const char* key, const char* value)
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
    return HasMeta(item->meta, item->metaCount, "translate", "no") ||
           HasMeta(item->meta, item->metaCount, "do-not-translate", nullptr) ||
           HasMeta(s->meta, s->metaCount, "translate", "no") ||
           HasMeta(s->meta, s->metaCount, "do-not-translate", nullptr);
}

static void CollectEntries(Input* in)
{
    for (size_t s = 0; s < in->resource.sectionCount; s++)
    {
        const ResSection* section = &in->resource.sections[s];
        for (size_t i = 0; i < section->itemCount; i++)
        {
            const ResItem* item = &section->items[i];
            char key[keyRoom];
            if (item->kind != resEntry ||
                HasMeta(item->meta, item->metaCount, "obsolete", nullptr) ||
                !ResourceKey(&section->id, &item->id, key, sizeof(key)))
            {
                continue;
            }
            in->entries = Grow(in->entries, in->count, sizeof(in->entries[0]));
            Entry* e = &in->entries[in->count++];
            e->length = strlen(key);
            e->key = malloc(e->length + 1);
            if (e->key == nullptr)
            {
                exit(1);
            }
            memcpy(e->key, key, e->length + 1);
            e->item = item;
            e->keep = KeepsSource(section, item);
        }
    }
}

// The locale's canonical id, from @locale (which the check parsed).
static void CanonicalLocale(Input* in)
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

static int ReadInput(Input* in, const char* path, FILE* report)
{
    *in = (Input){.path = path};
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
    return in->locale[0] != '\0';
}

static void FreeInput(Input* in)
{
    for (uint32_t i = 0; i < in->count; i++)
    {
        free(in->entries[i].key);
    }
    free(in->entries);
    ResourceFree(&in->resource);
    free(in->text);
}

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
static Bytes KeysUnit(const Input* source, const KeyTable* t, uint32_t* byIndex)
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
} Messages;

// Each index's message text for one input: its own, the source's where
// the source keeps it, none where it has none.
static Messages ChooseMessages(const Input* in, const Input* source, const CatalogKeys* keys,
                               FILE* report)
{
    Messages m = {(const ResText**)calloc(keys->count == 0 ? 1 : keys->count, sizeof(ResText*)), 0,
                  0};
    if (m.values == nullptr)
    {
        exit(1);
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
    }
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

static Bytes MessagesUnit(const Input* in, const Input* source, const CatalogKeys* keys,
                          FILE* report, int* okOut)
{
    Messages m = ChooseMessages(in, source, keys, report);
    Bytes head = {0};
    Bytes text = {0};
    Put32(&head, keys->count);
    Put32(&head, in == source ? catalogSourceFlag : 0);
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
    fprintf(report, "%s: %s, %u of %u messages, %u keys left out\n", in->path, in->locale,
            m.present, keys->count, m.orphans);
    free((void*)m.values);
    return head;
}

static int WriteCatalog(const char* directory, const Input* in, const Bytes* keys,
                        const Bytes* messages, FILE* report)
{
    mlocDataVersion own;
    mlocGetDataVersion(&own);
    DataFileVersion version = {
        .cldr = mlocPackVersion(own.cldr.major, own.cldr.minor, own.cldr.patch),
        .unicode = mlocPackVersion(own.unicode.major, own.unicode.minor, own.unicode.patch),
        .library = mlocPackVersion(own.library.major, own.library.minor, own.library.patch),
        .tables = own.tables,
    };
    memcpy(version.tzdb, own.tzdb, sizeof(version.tzdb));
    DataFile file = {0};
    DataFileAdd(&file, kindCatalogKeys, in->locale, keys->data, keys->size);
    DataFileAdd(&file, kindCatalogMessages, in->locale, messages->data, messages->size);
    size_t size = 0;
    uint8_t* bytes = DataFileWrite(&file, &version, &size);
    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.mloc", directory, in->locale);
    int ok = FileWrite(path, bytes, size);
    if (!ok)
    {
        fprintf(report, "%s: cannot write\n", path);
    }
    free(bytes);
    DataFileFree(&file);
    return ok;
}

// The keys of the source, as the table builder takes them.
static int BuildTable(const Input* source, KeyTable* table)
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

static int CompileAll(const char* directory, Input* inputs, size_t count, FILE* report)
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
        ok = ok && WriteCatalog(directory, &inputs[i], &keys, &messages, report);
        free(messages.data);
    }
    free(keys.data);
    free(byIndex);
    KeyTableFree(&table);
    return ok;
}

int CatalogCompile(const char* directory, char* const* paths, size_t count, FILE* report)
{
    Input* inputs = calloc(count + 1, sizeof(Input));
    if (inputs == nullptr)
    {
        exit(1);
    }
    int ok = count > 0;
    for (size_t i = 0; i < count; i++)
    {
        ok = ReadInput(&inputs[i], paths[i], report) && ok;
        for (size_t j = 0; ok && j < i; j++)
        {
            if (strcmp(inputs[i].locale, inputs[j].locale) == 0)
            {
                fprintf(report, "%s: a second catalog of %s\n", paths[i], inputs[i].locale);
                ok = 0;
            }
        }
    }
    ok = ok && CompileAll(directory, inputs, count, report);
    for (size_t i = 0; i < count; i++)
    {
        FreeInput(&inputs[i]);
    }
    free(inputs);
    return ok;
}
