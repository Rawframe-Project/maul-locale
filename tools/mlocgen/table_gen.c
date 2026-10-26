// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's table shape (table_gen.h).

#include "table_gen.h"

#include "currency_tables.h"
#include "number_tables.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <dirent.h>
#endif

void GenDie(const char* message, const char* detail)
{
    fprintf(stderr, "mlocgen: %s: %s\n", message, detail);
    exit(1);
}

void* GenAllocate(size_t size)
{
    void* p = calloc(1, size);
    if (p == nullptr)
    {
        GenDie("out of memory", "numbers");
    }
    return p;
}

bool GenCopy(char* out, size_t capacity, const char* text, size_t length)
{
    if (length >= capacity)
    {
        if (capacity > 0)
        {
            out[0] = '\0';
        }
        return false;
    }
    memcpy(out, text, length);
    out[length] = '\0';
    return true;
}

bool GenPrint(char* text, size_t capacity, size_t* size, const char* format, ...)
{
    if (*size >= capacity)
    {
        return false;
    }
    va_list arguments;
    va_start(arguments, format);
    int n = vsnprintf(text + *size, capacity - *size, format, arguments);
    va_end(arguments);
    if (n < 0 || (size_t)n >= capacity - *size)
    {
        text[*size] = '\0';
        return false;
    }
    *size += (size_t)n;
    return true;
}

void GenPut(uint8_t* p, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

int GenCharLength(const unsigned char* text)
{
    unsigned c = text[0];
    int n = c < 0x80             ? 1
            : (c & 0xE0) == 0xC0 ? 2
            : (c & 0xF0) == 0xE0 ? 3
            : (c & 0xF8) == 0xF0 ? 4
                                 : 0;
    for (int i = 1; i < n; i++)
    {
        if ((text[i] & 0xC0) != 0x80)
        {
            return 0;
        }
    }
    return n;
}

uint16_t TableAddString(Table* t, const char* text, size_t length)
{
    char key[maxTableString + 1];
    if (length >= sizeof(key))
    {
        GenDie("a number string too long", text);
    }
    memcpy(key, text, length);
    key[length] = '\0';
    void* found = MapGet(&t->strings, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (t->stringCount == maxTable)
    {
        GenDie("too many number strings", key);
    }
    t->texts[t->stringCount] = GenAllocate(length + 1);
    memcpy(t->texts[t->stringCount], text, length);
    t->lengths[t->stringCount] = length;
    MapPut(&t->strings, key, (void*)(uintptr_t)(t->stringCount + 1));
    return (uint16_t)t->stringCount++;
}

uint16_t TableAddRecord(Table* t, const uint8_t* record)
{
    char key[2 * maxTableRecord + 1];
    if (t->recordSize > maxTableRecord)
    {
        GenDie("a number record too long", "records");
    }
    for (int i = 0; i < t->recordSize; i++)
    {
        snprintf(key + 2 * i, 3, "%02X", record[i]);
    }
    void* found = t->distinctRecords ? MapGet(&t->records, key) : nullptr;
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (t->recordCount == maxTable)
    {
        GenDie("too many number records", key);
    }
    if (t->recordBytes == nullptr)
    {
        t->recordBytes = GenAllocate((size_t)maxTable * (size_t)t->recordSize);
    }
    memcpy(t->recordBytes + (size_t)t->recordCount * (size_t)t->recordSize, record,
           (size_t)t->recordSize);
    if (t->distinctRecords)
    {
        MapPut(&t->records, key, (void*)(uintptr_t)(t->recordCount + 1));
    }
    return (uint16_t)t->recordCount++;
}

void TableAddUnit(const Table* t, uint32_t kind, const char* key, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        text += t->lengths[i];
    }
    size_t offsets = (size_t)(t->stringCount + 1) * 2;
    size_t records = (size_t)t->recordCount * (size_t)t->recordSize;
    size_t size = numberTableHeaderSize + offsets + records + text;
    if (text > 0xFFFF || t->recordCount > 0xFFFF)
    {
        GenDie("a number table too large", "text or records");
    }
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)t->stringCount, 2);
    GenPut(unit + 2, (uint64_t)t->recordCount, 2);
    GenPut(unit + 4, text, 4);
    uint8_t* textAt = unit + numberTableHeaderSize + offsets + records;
    size_t at = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        GenPut(unit + numberTableHeaderSize + 2 * (size_t)i, at, 2);
        memcpy(textAt + at, t->texts[i], t->lengths[i]);
        at += t->lengths[i];
    }
    GenPut(unit + numberTableHeaderSize + 2 * (size_t)t->stringCount, at, 2);
    if (records > 0)
    {
        memcpy(unit + numberTableHeaderSize + offsets, t->recordBytes, records);
    }
    DataFileAdd(file, kind, key, unit, size);
    free(unit);
}

void TableClear(Table* t)
{
    for (int i = 0; i < t->stringCount; i++)
    {
        free(t->texts[i]);
    }
    free(t->recordBytes);
    MapFree(&t->strings);
    MapFree(&t->records);
    int recordSize = t->recordSize;
    bool distinct = t->distinctRecords;
    memset(t, 0, sizeof(*t));
    t->recordSize = recordSize;
    t->distinctRecords = distinct;
}

// Writes a unit of the sparse shape (currency_tables.h): the table's
// strings, and per entry its code and row; with flags, a byte per string
// after the text.
void TableAddSparseUnit(const Table* t, const uint16_t* codes, uint8_t* const* rows,
                        const size_t* rowSizes, int count, const uint8_t* flags, uint32_t kind,
                        const char* key, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        text += t->lengths[i];
    }
    size_t rowBytes = 0;
    for (int i = 0; i < count; i++)
    {
        rowBytes += rowSizes[i];
    }
    if (text > 0xFFFF || rowBytes > 0xFFFF)
    {
        GenDie("a sparse unit too large", key);
    }
    size_t strings = 2 * ((size_t)t->stringCount + 1);
    size_t entries = 2 * (size_t)count + 2 * ((size_t)count + 1);
    size_t size = sparseHeaderSize + strings + entries + rowBytes + text +
                  (flags != nullptr ? (size_t)t->stringCount : 0);
    uint8_t* unit = GenAllocate(size);
    GenPut(unit, (uint64_t)t->stringCount, 2);
    GenPut(unit + 2, (uint64_t)count, 2);
    GenPut(unit + 4, text, 4);
    GenPut(unit + 8, rowBytes, 4);
    uint8_t* at = unit + sparseHeaderSize;
    uint8_t* textAt = at + strings + entries + rowBytes;
    size_t offset = 0;
    for (int i = 0; i < t->stringCount; i++)
    {
        GenPut(at + 2 * (size_t)i, offset, 2);
        memcpy(textAt + offset, t->texts[i], t->lengths[i]);
        offset += t->lengths[i];
    }
    GenPut(at + 2 * (size_t)t->stringCount, offset, 2);
    at += strings;
    uint8_t* rowOffsets = at + 2 * (size_t)count;
    uint8_t* rowAt = rowOffsets + 2 * ((size_t)count + 1);
    offset = 0;
    for (int i = 0; i < count; i++)
    {
        GenPut(at + 2 * (size_t)i, codes[i], 2);
        GenPut(rowOffsets + 2 * (size_t)i, offset, 2);
        memcpy(rowAt + offset, rows[i], rowSizes[i]);
        offset += rowSizes[i];
    }
    GenPut(rowOffsets + 2 * (size_t)count, offset, 2);
    if (flags != nullptr)
    {
        memcpy(textAt + text, flags, (size_t)t->stringCount);
    }
    DataFileAdd(file, kind, key, unit, size);
    free(unit);
}

static int CompareNames(const void* a, const void* b)
{
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

static void AddName(char*** names, int* count, int* capacity, const char* name)
{
    size_t length = strlen(name);
    if (length < 5 || strcmp(name + length - 4, ".xml") != 0)
    {
        return;
    }
    if (length - 4 >= genMaxId)
    {
        GenDie("a file name longer than the generator holds", name);
    }
    if (*count == *capacity)
    {
        *capacity = *capacity * 2 + 64;
        char** grown = (char**)realloc((void*)*names, sizeof(char*) * (size_t)*capacity);
        if (grown == nullptr)
        {
            GenDie("out of memory", "listing files");
        }
        *names = grown;
    }
    (*names)[*count] = GenAllocate(length - 3);
    memcpy((*names)[*count], name, length - 4);
    (*count)++;
}

char** GenListXml(const char* commonDirectory, const char* subdirectory, int* countOut)
{
    char** names = nullptr;
    int count = 0;
    int capacity = 0;
    char path[4096];
#ifdef _WIN32
    snprintf(path, sizeof(path), "%s/%s/*.xml", commonDirectory, subdirectory);
    struct _finddata_t found;
    intptr_t handle = _findfirst(path, &found);
    for (int more = handle != -1; more; more = _findnext(handle, &found) == 0)
    {
        AddName(&names, &count, &capacity, found.name);
    }
    if (handle != -1)
    {
        _findclose(handle);
    }
#else
    snprintf(path, sizeof(path), "%s/%s", commonDirectory, subdirectory);
    DIR* directory = opendir(path);
    if (directory == nullptr)
    {
        GenDie("cannot list", path);
    }
    for (struct dirent* item = readdir(directory); item != nullptr; item = readdir(directory))
    {
        AddName(&names, &count, &capacity, item->d_name);
    }
    closedir(directory);
#endif
    if (count > 0)
    {
        qsort((void*)names, (size_t)count, sizeof(char*), CompareNames);
    }
    *countOut = count;
    return names;
}

void GenFreeList(char** names, int count)
{
    for (int i = 0; i < count; i++)
    {
        free(names[i]);
    }
    free((void*)names);
}
