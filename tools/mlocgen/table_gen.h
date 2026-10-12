// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The table shape of the number part's units (src/number_tables.h): a
// string pool and fixed-size records that index it, built and written
// here for the number and currency generators. The generator stops with
// a message on anything a table cannot hold.

#ifndef MLOCGEN_TABLE_GEN_H
#define MLOCGEN_TABLE_GEN_H

#include "datafile.h"
#include "map.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    maxTable = 8192,
    // The longest string a table takes, in bytes.
    maxTableString = 512,
    // The largest record size.
    maxTableRecord = 32,
};

typedef struct Table
{
    Map strings;
    char* texts[maxTable];
    size_t lengths[maxTable];
    int stringCount;
    Map records;
    uint8_t* recordBytes;
    int recordCount;
    int recordSize;
    // Whether equal records are kept once.
    bool distinctRecords;
} Table;

[[noreturn]] void GenDie(const char* message, const char* detail);

// Zeroed memory, or the program stops.
void* GenAllocate(size_t size);

// Stores a little-endian integer of 1 to 8 bytes.
void GenPut(uint8_t* p, uint64_t value, int bytes);

// The UTF-8 length of the character at text, or 0 when it is malformed.
int GenCharLength(const unsigned char* text);

// The index of a string in a table, added when new.
uint16_t TableAddString(Table* t, const char* text, size_t length);

// The index of a record (recordSize bytes): an equal one's when the
// table keeps records distinct, else a new one.
uint16_t TableAddRecord(Table* t, const uint8_t* record);

// Adds a table as a unit of a kind, under a locale key or none.
void TableAddUnit(const Table* t, uint32_t kind, const char* key, DataFile* file);

// Frees a table's contents and empties it, keeping its record settings.
void TableClear(Table* t);

#endif
