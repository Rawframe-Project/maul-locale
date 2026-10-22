// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An index value's bytes (record mloc-0075), at any address, read by
// loads and copies alone:
//
//   0  u32 stamp
//   4  u32 bucketCount, every bucket, shown or not, in boundary order
//   8  u32 shownCount
//  12  u32 poolSize
//  16  the collator at primary strength (sizeof(Collator) bytes)
//      bucketCount records of 20 bytes: u32 the label's offset in the
//      pool, u32 its length, u32 the lower boundary's offset, u32 its
//      length, u32 the type in bits 7..0 and in 31..8 the shown bucket it
//      counts as (its own number when shown)
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
    indexStamp = 0x58444E49,
    indexHead = 16,
    indexRecordSize = 20,
};

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

// The index's collator: its head and the general path's parts copied
// out (the fast compare never serves an index's primary strength).
static inline void mlocIndexCollator(const IndexView* v, Collator* out)
{
    (void)mlocReadCollatorHead(v->bytes + indexHead, out);
    mlocReadCollatorRest(v->bytes + indexHead, out);
}

static inline uint32_t mlocIndexField(const IndexView* v, uint32_t record, uint32_t field)
{
    return mlocLoad32(v->records + (size_t)record * indexRecordSize + (size_t)field * 4);
}

#endif
