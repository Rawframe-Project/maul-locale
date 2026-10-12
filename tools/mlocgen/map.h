// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A hash map from strings to pointers for the generator. Keys are
// copied; values are the caller's.

#ifndef MLOCGEN_MAP_H
#define MLOCGEN_MAP_H

#include <stddef.h>

typedef struct Map
{
    char** keys;
    void** values;
    size_t capacity;
    size_t count;
} Map;

// Sets key to value, replacing an earlier value.
void MapPut(Map* map, const char* key, void* value);

// The value of key, or null.
void* MapGet(const Map* map, const char* key);

// Calls visit for every key and value, in no particular order.
void MapEach(const Map* map, void (*visit)(const char* key, void* value, void* context),
             void* context);

// Frees the keys and the table; the values are the caller's.
void MapFree(Map* map);

#endif
