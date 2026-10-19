// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A compiled catalog's units (record mloc-0054), shared by the library,
// which reads them, and the catalog tool, which writes them; the readers
// are inline, so that the tool needs none of the library's internals. A catalog is
// a data file with one locale and two units of it. All numbers are
// little-endian.
//
// The keys (kindCatalogKeys):
//   0  u32 count, the keys
//   4  u32 buckets, at least 1
//   8  u32 places, count or more: the table the pilots spread keys over
//  12  u32 zero
//  16  u64 seed of the key hash and the pilots' mix
//  24  u64 set id (mlocCatalogSetId), so the catalogs of one set agree
//  32  u16 pilots[buckets], then zero bytes to a multiple of 4
//      u32 remap[places - count]: the index of a place past the last
//      u32 offsets[count + 1] into the key bytes, from 0, rising
//      the key bytes, each key NFC UTF-8
//
// The messages (kindCatalogMessages):
//   0  u32 count, the keys
//   4  u32 flags: bit 0, the set's source catalog; the rest zero
//   8  u64 set id
//  16  u32 offsets[count + 1] into the message bytes, from 0, rising;
//      an absent message is an empty one
//      the compiled messages (mlocCompileMessage's form)

#ifndef MLOC_CATALOG_TABLES_H
#define MLOC_CATALOG_TABLES_H

#include "bytes.h"
#include "catalog_hash.h"
#include "data_view.h"

#include <stdint.h>

enum
{
    catalogKeysHead = 32,
    catalogMessagesHead = 16,
    catalogSourceFlag = 1,
};

// A key index's place in the key table (the offsets after the pilots and
// the remap), and the like for the messages: read from a valid unit.
typedef struct CatalogKeys
{
    const uint8_t* bytes;
    uint32_t count;
    uint32_t buckets;
    uint32_t places;
    uint64_t seed;
    uint64_t setId;
    const uint8_t* pilots;
    const uint8_t* remap;
    const uint8_t* offsets;
    const uint8_t* text;
    uint32_t textSize;
} CatalogKeys;

// Reads a keys unit's head and parts; false when they do not fit its
// size.
static inline bool mlocReadCatalogKeys(DataSpan payload, CatalogKeys* out)
{
    if (payload.size < catalogKeysHead)
    {
        return false;
    }
    const uint8_t* p = payload.bytes;
    *out = (CatalogKeys){
        .bytes = p,
        .count = mlocLoad32(p),
        .buckets = mlocLoad32(p + 4),
        .places = mlocLoad32(p + 8),
        .seed = mlocLoad64(p + 16),
        .setId = mlocLoad64(p + 24),
    };
    uint64_t pilots = ((uint64_t)out->buckets * 2 + 3) / 4 * 4;
    uint64_t remap = out->places >= out->count ? (uint64_t)(out->places - out->count) * 4 : 0;
    uint64_t offsets = ((uint64_t)out->count + 1) * 4;
    uint64_t text = catalogKeysHead + pilots + remap + offsets;
    if (mlocLoad32(p + 12) != 0 || out->places < out->count || out->buckets == 0 ||
        text > payload.size)
    {
        return false;
    }
    out->pilots = p + catalogKeysHead;
    out->remap = out->pilots + pilots;
    out->offsets = out->remap + remap;
    out->text = out->offsets + offsets;
    out->textSize = payload.size - (uint32_t)text;
    return true;
}

// The index a key would have, before its bytes are compared.
static inline uint32_t mlocCatalogIndexOf(const CatalogKeys* keys, const uint8_t* key,
                                          size_t length)
{
    uint64_t hash = mlocXxh64(key, length, keys->seed);
    uint32_t bucket = mlocCatalogBucket(hash, keys->buckets);
    uint32_t pilot = mlocLoad16(keys->pilots + (size_t)bucket * 2);
    uint32_t place = mlocCatalogPlace(hash, pilot, keys->seed, keys->places);
    return place < keys->count ? place
                               : mlocLoad32(keys->remap + (size_t)(place - keys->count) * 4);
}

// A key's bytes by index, in a valid unit.
static inline const uint8_t* mlocCatalogKeyAt(const CatalogKeys* keys, uint32_t index,
                                              uint32_t* lengthOut)
{
    uint32_t start = mlocLoad32(keys->offsets + (size_t)index * 4);
    *lengthOut = mlocLoad32(keys->offsets + (size_t)index * 4 + 4) - start;
    return keys->text + start;
}

// The set id of a valid keys unit: from the count, each key in index
// order hashed (XXH64) with the id so far plus its length as the seed.
static inline uint64_t mlocCatalogSetId(const CatalogKeys* keys)
{
    uint64_t id = keys->count;
    for (uint32_t i = 0; i < keys->count; i++)
    {
        uint32_t length = 0;
        const uint8_t* key = mlocCatalogKeyAt(keys, i, &length);
        id = mlocXxh64(key, length, id + length);
    }
    return id;
}

bool mlocValidCatalogKeys(const DataView* view, DataSpan payload);
bool mlocValidCatalogMessages(const DataView* view, DataSpan payload);

#endif
