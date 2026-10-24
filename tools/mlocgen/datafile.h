// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writes a data file in the layout of src/data_format.h: units are added
// in any order, each with its kind, its locale id (or none) and its
// payload; strings the payloads refer to go into the pool. Writing sorts
// the locales and units, stores identical payloads once, and gives the
// same bytes for the same strings added in the same order, whatever the
// order of the units.

#ifndef MLOCGEN_DATAFILE_H
#define MLOCGEN_DATAFILE_H

#include "map.h"

#include <stddef.h>
#include <stdint.h>

typedef struct DataUnit
{
    uint32_t kind;
    // The locale id, or null for a unit of no locale.
    char* locale;
    uint8_t* payload;
    size_t size;
} DataUnit;

typedef struct DataFile
{
    DataUnit* units;
    size_t unitCount;
    size_t unitCapacity;
    // The string pool; locale ids are added to it when written.
    char* pool;
    size_t poolSize;
    size_t poolCapacity;
    // Each pooled string to its offset plus one.
    Map strings;
} DataFile;

// The header's version fields, packed as src/data_format.h packs them.
typedef struct DataFileVersion
{
    uint32_t cldr;
    uint32_t unicode;
    uint32_t format;
    char tzdb[8];
    uint64_t tables;
} DataFileVersion;

// Adds a unit; the payload is copied. Fails the program when the same
// kind and locale come twice.
void DataFileAdd(DataFile* file, uint32_t kind, const char* locale, const void* payload,
                 size_t size);

// Adds a string to the pool, or finds it there, and returns its offset
// in the pool.
uint32_t DataFileAddString(DataFile* file, const char* text, size_t length);

// The file's bytes, which the caller frees. Fails the program when the
// file would pass 4 GiB.
uint8_t* DataFileWrite(DataFile* file, const DataFileVersion* version, size_t* sizeOut);

void DataFileFree(DataFile* file);

#endif
