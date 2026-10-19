// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog's perfect hash builder (table.h).

#include "table.h"

#include "catalog_hash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    // Keys a bucket on average.
    keysPerBucket = 4,
    // Seeds tried before giving up.
    seedTries = 256,
    pilotCount = 65536,
};

static void* Allocate(size_t count, size_t size)
{
    void* p = calloc(count == 0 ? 1 : count, size);
    if (p == nullptr)
    {
        fputs("mloccat: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

typedef struct Builder
{
    const uint64_t* hashes;
    uint32_t count;
    uint32_t buckets;
    uint32_t places;
    uint64_t seed;
    // Each bucket's keys, by bucket: starts[b] to starts[b + 1].
    uint32_t* starts;
    uint32_t* members;
    uint8_t* taken;
    uint32_t* placeOf;
} Builder;

static int CompareBySize(const void* a, const void* b, const uint32_t* starts)
{
    uint32_t x = *(const uint32_t*)a;
    uint32_t y = *(const uint32_t*)b;
    uint32_t sx = starts[x + 1] - starts[x];
    uint32_t sy = starts[y + 1] - starts[y];
    return sx != sy ? (sx < sy) - (sx > sy) : (x > y) - (x < y);
}

static const uint32_t* s_sortStarts;

static int CompareBuckets(const void* a, const void* b)
{
    return CompareBySize(a, b, s_sortStarts);
}

// Groups the keys by bucket.
static void Group(Builder* b)
{
    uint32_t* sizes = Allocate((size_t)b->buckets + 1, sizeof(uint32_t));
    for (uint32_t i = 0; i < b->count; i++)
    {
        sizes[mlocCatalogBucket(b->hashes[i], b->buckets) + 1]++;
    }
    for (uint32_t i = 0; i < b->buckets; i++)
    {
        sizes[i + 1] += sizes[i];
    }
    memcpy(b->starts, sizes, ((size_t)b->buckets + 1) * sizeof(uint32_t));
    for (uint32_t i = 0; i < b->count; i++)
    {
        uint32_t bucket = mlocCatalogBucket(b->hashes[i], b->buckets);
        b->members[sizes[bucket]++] = i;
    }
    free(sizes);
}

// Tries a pilot for a bucket: its keys on free places, distinct.
static int Fits(Builder* b, uint32_t bucket, uint32_t pilot)
{
    uint32_t first = b->starts[bucket];
    uint32_t last = b->starts[bucket + 1];
    for (uint32_t i = first; i < last; i++)
    {
        uint32_t place = mlocCatalogPlace(b->hashes[b->members[i]], pilot, b->seed, b->places);
        if (b->taken[place])
        {
            for (uint32_t j = first; j < i; j++)
            {
                b->taken[b->placeOf[b->members[j]]] = 0;
            }
            return 0;
        }
        b->taken[place] = 1;
        b->placeOf[b->members[i]] = place;
    }
    return 1;
}

// Places every bucket, largest first; 0 when one finds no pilot.
static int Place(Builder* b, uint16_t* pilots)
{
    uint32_t* order = Allocate(b->buckets, sizeof(uint32_t));
    for (uint32_t i = 0; i < b->buckets; i++)
    {
        order[i] = i;
    }
    s_sortStarts = b->starts;
    qsort(order, b->buckets, sizeof(uint32_t), CompareBuckets);
    int ok = 1;
    for (uint32_t k = 0; ok && k < b->buckets; k++)
    {
        uint32_t bucket = order[k];
        uint32_t pilot = 0;
        while (pilot < pilotCount && !Fits(b, bucket, pilot))
        {
            pilot++;
        }
        ok = pilot < pilotCount;
        pilots[bucket] = (uint16_t)(ok ? pilot : 0);
    }
    free(order);
    return ok;
}

// Gives each key below the count its place, and each key past it a free
// place below the count, which the remap records.
static void Remap(const Builder* b, KeyTable* out)
{
    uint32_t vacant = 0;
    for (uint32_t i = 0; i < b->count; i++)
    {
        uint32_t place = b->placeOf[i];
        out->index[i] = place;
        if (place < b->count)
        {
            continue;
        }
        while (b->taken[vacant])
        {
            vacant++;
        }
        out->remap[place - b->count] = vacant;
        out->index[i] = vacant;
        vacant++;
    }
}

int KeyTableBuild(const char* const* keys, const size_t* lengths, uint32_t count, KeyTable* out)
{
    *out = (KeyTable){.count = count};
    out->buckets = count / keysPerBucket + 1;
    out->places = count + count / 50 + (count > 0 ? 1 : 0);
    uint64_t* hashes = Allocate(count, sizeof(uint64_t));
    Builder b = {
        .hashes = hashes,
        .count = count,
        .buckets = out->buckets,
        .places = out->places,
        .starts = Allocate((size_t)out->buckets + 1, sizeof(uint32_t)),
        .members = Allocate(count, sizeof(uint32_t)),
        .taken = Allocate(out->places, 1),
        .placeOf = Allocate(count, sizeof(uint32_t)),
    };
    out->pilots = Allocate(out->buckets, sizeof(uint16_t));
    out->remap = Allocate((size_t)out->places - count, sizeof(uint32_t));
    out->index = Allocate(count, sizeof(uint32_t));
    int ok = 0;
    for (uint64_t seed = 0; !ok && seed < seedTries; seed++)
    {
        b.seed = seed;
        for (uint32_t i = 0; i < count; i++)
        {
            hashes[i] = mlocXxh64((const uint8_t*)keys[i], lengths[i], seed);
        }
        memset(b.taken, 0, out->places);
        Group(&b);
        ok = Place(&b, out->pilots);
    }
    if (ok)
    {
        out->seed = b.seed;
        Remap(&b, out);
    }
    free(hashes);
    free(b.starts);
    free(b.members);
    free(b.taken);
    free(b.placeOf);
    return ok;
}

void KeyTableFree(KeyTable* table)
{
    free(table->pilots);
    free(table->remap);
    free(table->index);
    *table = (KeyTable){0};
}
