// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The generator's hash map (map.h): open addressing with linear probing,
// FNV-1a hashes, grown at three quarters full.

#include "map.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t Hash(const char* key)
{
    uint64_t hash = 14695981039346656037u;
    for (; *key != '\0'; key++)
    {
        hash = (hash ^ (unsigned char)*key) * 1099511628211u;
    }
    return hash;
}

static void* Zeroed(size_t count, size_t size)
{
    void* memory = calloc(count, size);
    if (memory == nullptr)
    {
        fputs("mlocgen: out of memory\n", stderr);
        exit(1);
    }
    return memory;
}

static size_t Slot(const Map* map, const char* key)
{
    size_t mask = map->capacity - 1;
    size_t i = (size_t)Hash(key) & mask;
    while (map->keys[i] != nullptr && strcmp(map->keys[i], key) != 0)
    {
        i = (i + 1) & mask;
    }
    return i;
}

static void Resize(Map* map, size_t capacity)
{
    Map grown = {Zeroed(capacity, sizeof(char*)), Zeroed(capacity, sizeof(void*)), capacity,
                 map->count};
    for (size_t i = 0; i < map->capacity; i++)
    {
        if (map->keys[i] != nullptr)
        {
            size_t slot = Slot(&grown, map->keys[i]);
            grown.keys[slot] = map->keys[i];
            grown.values[slot] = map->values[i];
        }
    }
    free(map->keys);
    free(map->values);
    *map = grown;
}

void MapPut(Map* map, const char* key, void* value)
{
    if ((map->count + 1) * 4 > map->capacity * 3)
    {
        Resize(map, map->capacity == 0 ? 64 : map->capacity * 2);
    }
    size_t slot = Slot(map, key);
    if (map->keys[slot] == nullptr)
    {
        size_t length = strlen(key) + 1;
        map->keys[slot] = Zeroed(length, 1);
        memcpy(map->keys[slot], key, length);
        map->count++;
    }
    map->values[slot] = value;
}

void* MapGet(const Map* map, const char* key)
{
    if (map->capacity == 0)
    {
        return nullptr;
    }
    size_t slot = Slot(map, key);
    return map->keys[slot] == nullptr ? nullptr : map->values[slot];
}

void MapEach(const Map* map, void (*visit)(const char* key, void* value, void* context),
             void* context)
{
    for (size_t i = 0; i < map->capacity; i++)
    {
        if (map->keys[i] != nullptr)
        {
            visit(map->keys[i], map->values[i], context);
        }
    }
}

void MapFree(Map* map)
{
    for (size_t i = 0; i < map->capacity; i++)
    {
        free(map->keys[i]);
    }
    free(map->keys);
    free(map->values);
    *map = (Map){0};
}
