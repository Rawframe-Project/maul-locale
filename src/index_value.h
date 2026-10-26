// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An index value's bytes, at any address, read by loads and copies alone:
//
//   0  u32 stamp, the bytes "MLIX"
//   4  u32 bucketCount, every bucket, shown or not, in boundary order
//   8  u32 shownCount
//  12  u32 poolSize
//  16  the collator at primary strength (sizeof(Collator) bytes)
//      bucketCount records of 36 bytes: u32 the label's offset in the
//      pool, u32 its length, u32 the lower boundary's offset, u32 its
//      length, u32 the type in bits 7..0 and in 31..8 the shown bucket it
//      counts as (its own number when shown), then the boundary's key:
//      indexKeyWeights u32 weights, u32 their count in bits 7..0 and 1 in
//      bit 8 when they are all of them
//      shownCount u32: each shown bucket's record
//      the pool, UTF-8

#ifndef MLOC_SRC_INDEX_VALUE_H
#define MLOC_SRC_INDEX_VALUE_H

#include "bytes.h"
#include "collator.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum
{
    // "MLIX" in memory, read as a little-endian word. A stamp, not a
    // format's magic: the bytes hold the collator's pointers into the
    // data, so no version is carried.
    indexStamp = 0x58494C4D,
    indexHead = 16,
    indexRecordSize = 36,
    // The primary weights a key holds at most.
    indexKeyWeights = 3,
    // mlocIndexKeyOrder's answer when the keys do not tell.
    indexKeyUnknown = 2,
};

static_assert(indexRecordSize == (5 + indexKeyWeights + 1) * 4, "a record holds its key");

// A text's primary weights as a compare reads them: all of them (whole),
// or the first indexKeyWeights of more.
typedef struct IndexKey
{
    uint32_t weights[indexKeyWeights];
    uint8_t count;
    bool whole;
} IndexKey;

// Two texts' order at the primary strength from their keys: -1, 0 or 1,
// or indexKeyUnknown when the keys do not tell (both cut alike, or equal
// with a case level after).
static inline int mlocIndexKeyOrder(const IndexKey* x, const IndexKey* y, bool caseLevel)
{
    uint32_t n = x->count < y->count ? x->count : y->count;
    for (uint32_t i = 0; i < n; i++)
    {
        if (x->weights[i] != y->weights[i])
        {
            return x->weights[i] < y->weights[i] ? -1 : 1;
        }
    }
    if (x->count != y->count)
    {
        // Only a key at its capacity is cut, so the shorter is whole: the
        // start of the longer.
        return x->count < y->count ? -1 : 1;
    }
    if (x->whole != y->whole)
    {
        // The cut one has more.
        return x->whole ? -1 : 1;
    }
    return x->whole && !caseLevel ? 0 : indexKeyUnknown;
}

static inline size_t mlocIndexRecords(void)
{
    return indexHead + sizeof(Collator);
}

typedef struct IndexView
{
    const uint8_t* bytes;
    uint32_t bucketCount;
    uint32_t shownCount;
    const uint8_t* records;
    const uint8_t* shown;
    const uint8_t* pool;
} IndexView;

// An index's view; false for bytes mlocBuildIndex did not write.
static inline bool mlocReadIndex(const void* index, IndexView* out)
{
    if (index == nullptr || mlocLoad32(index) != indexStamp)
    {
        return false;
    }
    const uint8_t* p = index;
    out->bytes = p;
    out->bucketCount = mlocLoad32(p + 4);
    out->shownCount = mlocLoad32(p + 8);
    out->records = p + mlocIndexRecords();
    out->shown = out->records + (size_t)out->bucketCount * indexRecordSize;
    out->pool = out->shown + (size_t)out->shownCount * 4;
    return true;
}

static inline uint32_t mlocIndexField(const IndexView* v, uint32_t record, uint32_t field)
{
    return mlocLoad32(v->records + (size_t)record * indexRecordSize + (size_t)field * 4);
}

// A bucket's boundary key, which its record holds after the five fields.
static inline IndexKey mlocIndexBoundaryKey(const IndexView* v, uint32_t record)
{
    IndexKey key;
    for (uint32_t i = 0; i < indexKeyWeights; i++)
    {
        key.weights[i] = mlocIndexField(v, record, 5 + i);
    }
    uint32_t last = mlocIndexField(v, record, 5 + indexKeyWeights);
    key.count = (uint8_t)last;
    key.whole = (last >> 8 & 1) != 0;
    return key;
}

#endif
