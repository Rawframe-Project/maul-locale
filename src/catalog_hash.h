// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The catalog's hashes: XXH64 as its specification defines it (xxHash's
// xxhash_spec.md), and the minimal perfect hash's slot of a key's hash
// under its bucket's pilot. Shared by the library, which looks keys up,
// and the catalog tool, which builds the tables; inline, so that the tool
// needs none of the library's internals.

#ifndef MLOC_CATALOG_HASH_H
#define MLOC_CATALOG_HASH_H

#include "bytes.h"

#include <stddef.h>
#include <stdint.h>

// The bucket of a key's hash among count buckets.
static inline uint32_t mlocCatalogBucket(uint64_t hash, uint32_t count)
{
    return (uint32_t)(((hash >> 32) * count) >> 32);
}

static const uint64_t catalogPrime1 = 0x9E3779B185EBCA87U;
static const uint64_t catalogPrime2 = 0xC2B2AE3D27D4EB4FU;
static const uint64_t catalogPrime3 = 0x165667B19E3779F9U;
static const uint64_t catalogPrime4 = 0x85EBCA77C2B2AE63U;
static const uint64_t catalogPrime5 = 0x27D4EB2F165667C5U;

static inline uint64_t mlocCatalogRotate(uint64_t x, int bits)
{
    return x << bits | x >> (64 - bits);
}

static inline uint64_t mlocCatalogRound(uint64_t accumulator, uint64_t lane)
{
    return mlocCatalogRotate(accumulator + lane * catalogPrime2, 31) * catalogPrime1;
}

static inline uint64_t mlocCatalogMerge(uint64_t hash, uint64_t accumulator)
{
    return (hash ^ mlocCatalogRound(0, accumulator)) * catalogPrime1 + catalogPrime4;
}

static inline uint64_t mlocCatalogAvalanche(uint64_t hash)
{
    hash ^= hash >> 33;
    hash *= catalogPrime2;
    hash ^= hash >> 29;
    hash *= catalogPrime3;
    return hash ^ hash >> 32;
}

// XXH64's stripes of 32 bytes, four accumulators, merged.
static inline uint64_t mlocCatalogStripes(const uint8_t* bytes, size_t length, uint64_t seed,
                                          size_t* usedOut)
{
    uint64_t a[4] = {seed + catalogPrime1 + catalogPrime2, seed + catalogPrime2, seed,
                     seed - catalogPrime1};
    size_t at = 0;
    for (; at + 32 <= length; at += 32)
    {
        for (size_t i = 0; i < 4; i++)
        {
            a[i] = mlocCatalogRound(a[i], mlocLoad64(bytes + at + i * 8));
        }
    }
    uint64_t hash = mlocCatalogRotate(a[0], 1) + mlocCatalogRotate(a[1], 7) +
                    mlocCatalogRotate(a[2], 12) + mlocCatalogRotate(a[3], 18);
    for (size_t i = 0; i < 4; i++)
    {
        hash = mlocCatalogMerge(hash, a[i]);
    }
    *usedOut = at;
    return hash;
}

// XXH64 of length bytes with a seed.
static inline uint64_t mlocXxh64(const uint8_t* bytes, size_t length, uint64_t seed)
{
    size_t at = 0;
    uint64_t hash =
        length >= 32 ? mlocCatalogStripes(bytes, length, seed, &at) : seed + catalogPrime5;
    hash += (uint64_t)length;
    for (; at + 8 <= length; at += 8)
    {
        hash = mlocCatalogRotate(hash ^ mlocCatalogRound(0, mlocLoad64(bytes + at)), 27) *
                   catalogPrime1 +
               catalogPrime4;
    }
    if (at + 4 <= length)
    {
        hash = mlocCatalogRotate(hash ^ (uint64_t)mlocLoad32(bytes + at) * catalogPrime1, 23) *
                   catalogPrime2 +
               catalogPrime3;
        at += 4;
    }
    for (; at < length; at++)
    {
        hash = mlocCatalogRotate(hash ^ bytes[at] * catalogPrime5, 11) * catalogPrime1;
    }
    return mlocCatalogAvalanche(hash);
}

// A key's place in a table of size places under its bucket's pilot: the
// whole hash mixed with the pilot's own mix, then reduced by
// multiplying.
static inline uint32_t mlocCatalogPlace(uint64_t hash, uint32_t pilot, uint64_t seed, uint32_t size)
{
    // The whole hash is mixed again: the bucket took its high half, so
    // two keys of one bucket may share it, and must still part.
    uint64_t mixed =
        (hash ^ mlocCatalogAvalanche(seed ^ ((uint64_t)pilot + 1) * catalogPrime1)) * catalogPrime2;
    mixed ^= mixed >> 29;
    return (uint32_t)(((mixed >> 32) * size) >> 32);
}

#endif
