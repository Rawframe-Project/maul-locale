// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A compiled catalog's units read and checked (catalog_tables.h).

#include "catalog_tables.h"

#include "bytes.h"
#include "catalog_hash.h"
#include "message_read.h"

#include <string.h>

// Whether count + 1 offsets rise from 0 and end within size.
static bool RisingOffsets(const uint8_t* offsets, uint32_t count, uint32_t size)
{
    uint32_t previous = 0;
    if (mlocLoad32(offsets) != 0)
    {
        return false;
    }
    for (uint32_t i = 1; i <= count; i++)
    {
        uint32_t offset = mlocLoad32(offsets + (size_t)i * 4);
        if (offset < previous || offset > size)
        {
            return false;
        }
        previous = offset;
    }
    return true;
}

mlocResult mlocValidCatalogKeys(DataSpan payload)
{
    CatalogKeys keys;
    if (!mlocReadCatalogKeys(payload, &keys) ||
        !RisingOffsets(keys.offsets, keys.count, keys.textSize))
    {
        return mloc_errorFormat;
    }
    for (uint32_t i = 0; i < keys.places - keys.count; i++)
    {
        if (mlocLoad32(keys.remap + (size_t)i * 4) >= keys.count)
        {
            return mloc_errorFormat;
        }
    }
    // Every key is found at its own index, and is NUL-free: the lookups
    // need no other check than the comparison.
    for (uint32_t i = 0; i < keys.count; i++)
    {
        uint32_t length = 0;
        const uint8_t* key = mlocCatalogKeyAt(&keys, i, &length);
        if (memchr(key, 0, length) != nullptr || mlocCatalogIndexOf(&keys, key, length) != i)
        {
            return mloc_errorFormat;
        }
    }
    return mloc_success;
}

mlocResult mlocValidCatalogMessages(DataSpan payload)
{
    if (payload.size < catalogMessagesHead)
    {
        return mloc_errorFormat;
    }
    const uint8_t* p = payload.bytes;
    uint32_t count = mlocLoad32(p);
    uint64_t text = catalogMessagesHead + ((uint64_t)count + 1) * 4;
    if (mlocLoad32(p + 4) != 0 || text > payload.size ||
        !RisingOffsets(p + catalogMessagesHead, count, payload.size - (uint32_t)text))
    {
        return mloc_errorFormat;
    }
    for (uint32_t i = 0; i < count; i++)
    {
        uint32_t start = mlocLoad32(p + catalogMessagesHead + (size_t)i * 4);
        uint32_t end = mlocLoad32(p + catalogMessagesHead + (size_t)i * 4 + 4);
        MessageView message;
        mlocResult read =
            end > start ? mlocReadMessage(p + text + start, end - start, &message) : mloc_success;
        if (read != mloc_success)
        {
            return read;
        }
    }
    return mloc_success;
}
