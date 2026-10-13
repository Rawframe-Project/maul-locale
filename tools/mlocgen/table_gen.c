// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number part's table shape (table_gen.h).

#include "table_gen.h"

#include "currency_tables.h"
#include "number_tables.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
