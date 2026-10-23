// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog set read: the source and its translations, each read, checked
// and its entries collected (the obsolete left out) with its canonical
// locale. The compiler and the report read sets the same way.

#ifndef MLOCCAT_CATALOG_SET_H
#define MLOCCAT_CATALOG_SET_H

#include "resource.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum
{
    keyRoom = 1024,
    localeRoom = 128,
};

// An entry that compiles: its key and the item it came from.
typedef struct CatalogEntry
{
    char* key;
    size_t length;
    const ResItem* item;
    const ResSection* section;
    // Whether it or its section says @translate no.
    int keep;
} CatalogEntry;

typedef struct CatalogInput
{
    const char* path;
    char* text;
    Resource resource;
    CatalogEntry* entries;
    uint32_t count;
    // The entries sorted by key, for CatalogFindEntry.
    const CatalogEntry** byKey;
    char locale[localeRoom];
} CatalogInput;

// A set: inputs[0] the source, the others its translations.
typedef struct CatalogSet
{
    CatalogInput* inputs;
    size_t count;
} CatalogSet;

// Reads every resource; writes each problem to report. Returns 0 when a
// resource did not read or check, has no locale, or repeats another's.
int CatalogSetRead(CatalogSet* set, char* const* paths, size_t count, FILE* report);

void CatalogSetFree(CatalogSet* set);

// Whether metadata holds key (with value, unless value is null).
int CatalogHasMeta(const ResMeta* meta, size_t count, const char* key, const char* value);

// An input's entry of a key, or null.
const CatalogEntry* CatalogFindEntry(const CatalogInput* in, const char* key);

// The first metadata of key, or null.
const ResMeta* CatalogFindMeta(const ResMeta* meta, size_t count, const char* key);

// Grows an array of count items by one; exits when memory runs out.
void* CatalogGrow(void* items, size_t count, size_t size);

#endif
