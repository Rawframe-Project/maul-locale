// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The minimal perfect hash of a catalog's keys, built as PTHash builds
// one: keys hashed into buckets of about four, buckets placed largest
// first, each with the first 16-bit pilot that puts all its keys on free
// places of a table 2% larger than the keys; places past the last key
// remapped to the free ones below. The same keys give the same table.

#ifndef MLOCCAT_TABLE_H
#define MLOCCAT_TABLE_H

#include <stddef.h>
#include <stdint.h>

typedef struct KeyTable
{
    uint32_t count;
    uint32_t buckets;
    uint32_t places;
    uint64_t seed;
    uint16_t* pilots;
    uint32_t* remap;
    // Each key's index, by the order the keys were given.
    uint32_t* index;
} KeyTable;

// Builds the table of count keys; 0 when no seed of the first 256 gives
// one (which keys that are not distinct also cause).
int KeyTableBuild(const char* const* keys, const size_t* lengths, uint32_t count, KeyTable* out);

void KeyTableFree(KeyTable* table);

#endif
