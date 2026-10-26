// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The data file writer (datafile.h).

#include "datafile.h"

#include "data_format.h"
#include "table_gen.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[noreturn]] static void Die(const char* message)
{
    fprintf(stderr, "mlocgen: %s\n", message);
    exit(1);
}

static void* Grow(void* block, size_t* capacity, size_t needed, size_t elementSize)
{
    if (needed <= *capacity)
    {
        return block;
    }
    size_t next = *capacity == 0 ? 64 : *capacity;
    while (next < needed)
    {
        next *= 2;
    }
    void* grown = realloc(block, next * elementSize);
    if (grown == nullptr)
    {
        Die("out of memory");
    }
    *capacity = next;
    return grown;
}

static char* Copy(const char* text)
{
    size_t length = strlen(text) + 1;
    char* copy = malloc(length);
    if (copy == nullptr)
    {
        Die("out of memory");
    }
    return memcpy(copy, text, length);
}

void DataFileAdd(DataFile* file, uint32_t kind, const char* locale, const void* payload,
                 size_t size)
{
    file->units = Grow(file->units, &file->unitCapacity, file->unitCount + 1, sizeof(DataUnit));
    uint8_t* copy = malloc(size == 0 ? 1 : size);
    if (copy == nullptr)
    {
        Die("out of memory");
    }
    if (size > 0)
    {
        memcpy(copy, payload, size);
    }
    file->units[file->unitCount++] = (DataUnit){
        .kind = kind,
        .locale = locale == nullptr ? nullptr : Copy(locale),
        .payload = copy,
        .size = size,
    };
}

uint32_t DataFileAddString(DataFile* file, const char* text, size_t length)
{
    char* key = malloc(length + 1);
    if (key == nullptr)
    {
        Die("out of memory");
    }
    memcpy(key, text, length);
    key[length] = '\0';
    if (strlen(key) != length)
    {
        Die("a pooled string holds a NUL");
    }
    uintptr_t known = (uintptr_t)MapGet(&file->strings, key);
    if (known != 0)
    {
        free(key);
        return (uint32_t)(known - 1);
    }
    if (file->poolSize + length > UINT32_MAX)
    {
        Die("the string pool passes 4 GiB");
    }
    file->pool = Grow(file->pool, &file->poolCapacity, file->poolSize + length, 1);
    memcpy(file->pool + file->poolSize, text, length);
    uint32_t offset = (uint32_t)file->poolSize;
    file->poolSize += length;
    MapPut(&file->strings, key, (void*)(uintptr_t)(offset + 1));
    free(key);
    return offset;
}

// Orders ids by their bytes, a prefix first, as the library does.
static int CompareLocaleIds(const void* a, const void* b)
{
    return strcmp(*(char* const*)a, *(char* const*)b);
}

// The sorted, distinct locale ids of the units.
static char** Locales(const DataFile* file, size_t* countOut)
{
    char** ids = malloc((file->unitCount + 1) * sizeof(char*));
    if (ids == nullptr)
    {
        Die("out of memory");
    }
    size_t count = 0;
    for (size_t i = 0; i < file->unitCount; i++)
    {
        if (file->units[i].locale != nullptr)
        {
            ids[count++] = file->units[i].locale;
        }
    }
    GenSort(ids, count, sizeof(char*), CompareLocaleIds);
    size_t distinct = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (distinct == 0 || strcmp(ids[distinct - 1], ids[i]) != 0)
        {
            ids[distinct++] = ids[i];
        }
    }
    *countOut = distinct;
    return ids;
}

typedef struct Placed
{
    uint32_t kind;
    uint32_t locale;
    const DataUnit* unit;
} Placed;

static int ComparePlaced(const void* a, const void* b)
{
    const Placed* x = a;
    const Placed* y = b;
    uint64_t kx = (uint64_t)x->kind << 32 | x->locale;
    uint64_t ky = (uint64_t)y->kind << 32 | y->locale;
    return (kx > ky) - (kx < ky);
}

static uint32_t LocaleIndex(char** ids, size_t count, const char* locale)
{
    if (locale == nullptr)
    {
        return dataNoLocale;
    }
    char** found = bsearch(&locale, ids, count, sizeof(char*), CompareLocaleIds);
    return (uint32_t)(found - ids);
}

static void Put16(uint8_t* p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void Put32(uint8_t* p, uint32_t v)
{
    Put16(p, v);
    Put16(p + 2, v >> 16);
}

static void Put64(uint8_t* p, uint64_t v)
{
    Put32(p, (uint32_t)v);
    Put32(p + 4, (uint32_t)(v >> 32));
}

static size_t Align(size_t offset)
{
    return (offset + dataAlignment - 1) / dataAlignment * dataAlignment;
}

static uint64_t Hash(const uint8_t* bytes, size_t size)
{
    uint64_t hash = 0xCBF29CE484222325u;
    for (size_t i = 0; i < size; i++)
    {
        hash = (hash ^ bytes[i]) * 0x100000001B3u;
    }
    return hash;
}

// Places every payload after the tables, an identical one where its
// twin went; returns the end of the payloads and fills offsets.
static size_t PlacePayloads(const Placed* placed, size_t count, size_t start, size_t* offsets)
{
    // A payload's size and hash to the first unit placed with them.
    Map seen = {0};
    size_t end = start;
    for (size_t i = 0; i < count; i++)
    {
        const DataUnit* unit = placed[i].unit;
        char key[48];
        snprintf(key, sizeof(key), "%zu:%016llx", unit->size,
                 (unsigned long long)Hash(unit->payload, unit->size));
        uintptr_t first = (uintptr_t)MapGet(&seen, key);
        const DataUnit* twin = first == 0 ? nullptr : placed[first - 1].unit;
        if (twin != nullptr && memcmp(twin->payload, unit->payload, unit->size) == 0)
        {
            offsets[i] = offsets[first - 1];
            continue;
        }
        if (twin == nullptr)
        {
            MapPut(&seen, key, (void*)(uintptr_t)(i + 1));
        }
        offsets[i] = end;
        end = Align(end + unit->size);
    }
    MapFree(&seen);
    return end;
}

static void WriteHeader(uint8_t* out, const DataFileVersion* version, size_t size, size_t unitCount,
                        size_t localeCount, size_t poolOffset, size_t poolSize)
{
    memcpy(out + headerMagic, MLOC_DATA_MAGIC, 4);
    Put16(out + headerFormat, dataFormatVersion);
    Put16(out + headerHeaderSize, dataHeaderSize);
    Put32(out + headerFileSize, (uint32_t)size);
    Put32(out + headerCldr, version->cldr);
    Put32(out + headerUnicode, version->unicode);
    Put32(out + headerDataFormat, version->format);
    memcpy(out + headerTzdb, version->tzdb, sizeof(version->tzdb));
    Put64(out + headerTables, version->tables);
    Put32(out + headerUnitCount, (uint32_t)unitCount);
    Put32(out + headerLocaleCount, (uint32_t)localeCount);
    Put32(out + headerPoolOffset, (uint32_t)poolOffset);
    Put32(out + headerPoolSize, (uint32_t)poolSize);
}

uint8_t* DataFileWrite(DataFile* file, const DataFileVersion* version, size_t* sizeOut)
{
    size_t localeCount = 0;
    char** ids = Locales(file, &localeCount);
    uint32_t* idOffsets = malloc((localeCount + 1) * sizeof(uint32_t));
    Placed* placed = malloc((file->unitCount + 1) * sizeof(Placed));
    size_t* offsets = malloc((file->unitCount + 1) * sizeof(size_t));
    if (idOffsets == nullptr || placed == nullptr || offsets == nullptr)
    {
        Die("out of memory");
    }
    for (size_t i = 0; i < localeCount; i++)
    {
        if (strlen(ids[i]) > dataMaxLocaleId)
        {
            Die("a locale id is longer than the format allows");
        }
        idOffsets[i] = DataFileAddString(file, ids[i], strlen(ids[i]));
    }
    for (size_t i = 0; i < file->unitCount; i++)
    {
        const DataUnit* unit = &file->units[i];
        placed[i] = (Placed){unit->kind, LocaleIndex(ids, localeCount, unit->locale), unit};
    }
    GenSort(placed, file->unitCount, sizeof(Placed), ComparePlaced);
    for (size_t i = 1; i < file->unitCount; i++)
    {
        if (ComparePlaced(&placed[i - 1], &placed[i]) == 0)
        {
            Die("a unit's kind and locale come twice");
        }
    }
    size_t tablesEnd =
        dataHeaderSize + file->unitCount * dataUnitSize + localeCount * dataLocaleSize;
    size_t poolOffset = PlacePayloads(placed, file->unitCount, Align(tablesEnd), offsets);
    size_t size = poolOffset + file->poolSize;
    if (size > UINT32_MAX)
    {
        Die("the data file passes 4 GiB");
    }
    uint8_t* out = calloc(size, 1);
    if (out == nullptr)
    {
        Die("out of memory");
    }
    WriteHeader(out, version, size, file->unitCount, localeCount, poolOffset, file->poolSize);
    for (size_t i = 0; i < file->unitCount; i++)
    {
        uint8_t* entry = out + dataHeaderSize + i * dataUnitSize;
        Put32(entry, placed[i].kind);
        Put32(entry + 4, placed[i].locale);
        Put32(entry + 8, (uint32_t)offsets[i]);
        Put32(entry + 12, (uint32_t)placed[i].unit->size);
        memcpy(out + offsets[i], placed[i].unit->payload, placed[i].unit->size);
    }
    for (size_t i = 0; i < localeCount; i++)
    {
        uint8_t* entry = out + dataHeaderSize + file->unitCount * dataUnitSize + i * dataLocaleSize;
        Put32(entry, idOffsets[i]);
        Put32(entry + 4, (uint32_t)strlen(ids[i]));
    }
    // A file without strings, as a zones file is, has no pool.
    if (file->poolSize > 0)
    {
        memcpy(out + poolOffset, file->pool, file->poolSize);
    }
    free(ids);
    free(idOffsets);
    free(placed);
    free(offsets);
    *sizeOut = size;
    return out;
}

void DataFileFree(DataFile* file)
{
    for (size_t i = 0; i < file->unitCount; i++)
    {
        free(file->units[i].locale);
        free(file->units[i].payload);
    }
    free(file->units);
    free(file->pool);
    MapFree(&file->strings);
    *file = (DataFile){0};
}
