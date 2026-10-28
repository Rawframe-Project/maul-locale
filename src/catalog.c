// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalogs: opening one from its bytes, and the lookups.

#include "maul-locale/catalog.h"

#include "asset.h"
#include "bytes.h"
#include "catalog_tables.h"
#include "catalog_view.h"

#include <string.h>

static const AssetFormat catalogFormat = {
    .magic = MLOC_CATALOG_MAGIC,
    .version = catalogFormatVersion,
    .flags = catalogSourceFlag,
    .tags = catalogTags,
    .tagCount = catalogFieldCount,
    .required = (1u << catalogLocale) | (1u << catalogKeys) | (1u << catalogMessages),
};

static DataSpan SpanOf(const uint8_t* bytes, AssetField field)
{
    return (DataSpan){bytes + field.start, field.length};
}

mlocResult mlocOpenCatalog(mlocCatalog* catalogOut, const void* bytes, size_t size)
{
    if (catalogOut == nullptr || bytes == nullptr)
    {
        return mloc_errorInvalid;
    }
    const uint8_t* p = bytes;
    uint32_t flags = 0;
    AssetField fields[catalogFieldCount];
    mlocResult r = mlocReadAsset(p, size, &catalogFormat, &flags, fields);
    if (r != mloc_success)
    {
        return r;
    }
    DataSpan keysSpan = SpanOf(p, fields[catalogKeys]);
    DataSpan messagesSpan = SpanOf(p, fields[catalogMessages]);
    r = mlocValidCatalogKeys(keysSpan);
    if (r == mloc_success)
    {
        r = mlocValidCatalogMessages(messagesSpan);
    }
    if (r != mloc_success)
    {
        return r;
    }
    // Both parts are of one set, and the keys' id is theirs.
    CatalogKeys keys;
    if (!mlocReadCatalogKeys(keysSpan, &keys) || mlocLoad32(messagesSpan.bytes) != keys.count ||
        mlocLoad64(messagesSpan.bytes + 8) != keys.setId || mlocCatalogSetId(&keys) != keys.setId)
    {
        return mloc_errorFormat;
    }
    // A locale the library reads, so a catalog's info never fails.
    const char* id = (const char*)p + fields[catalogLocale].start;
    uint32_t length = fields[catalogLocale].length;
    mlocLocale parsed;
    if (length > MLOC_MAX_LOCALE_TEXT ||
        mlocParseLocale(id, length, &parsed).status != mloc_success)
    {
        return mloc_errorFormat;
    }
    Catalog c = {
        .keys = keysSpan.bytes,
        .messages = messagesSpan.bytes,
        .locale = id,
        .keysSize = keysSpan.size,
        .messagesSize = messagesSpan.size,
        .localeLength = length,
        .source = (flags & catalogSourceFlag) != 0,
        .stamp = catalogStamp,
    };
    memset(catalogOut, 0, sizeof(*catalogOut));
    memcpy(catalogOut, &c, sizeof(c));
    return mloc_success;
}

mlocResult mlocGetCatalogInfo(const mlocCatalog* catalog, mlocCatalogInfo* infoOut)
{
    Catalog c;
    if (!mlocReadCatalog(catalog, &c) || infoOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    CatalogKeys keys;
    if (!mlocCatalogKeysOf(&c, &keys))
    {
        return mloc_errorFormat;
    }
    uint32_t present = 0;
    const uint8_t* offsets = c.messages + catalogMessagesHead;
    for (uint32_t i = 0; i < keys.count; i++)
    {
        present += mlocLoad32(offsets + (size_t)i * 4 + 4) > mlocLoad32(offsets + (size_t)i * 4);
    }
    mlocCatalogInfo info = {
        .setId = keys.setId,
        .keyCount = keys.count,
        .messageCount = present,
        .source = c.source,
    };
    if (mlocParseLocale(c.locale, c.localeLength, &info.locale).status != mloc_success)
    {
        return mloc_errorFormat;
    }
    *infoOut = info;
    return mloc_success;
}

mlocResult mlocFindCatalogKey(const mlocCatalog* catalog, const char* key, size_t length,
                              uint32_t* indexOut)
{
    Catalog c;
    if (!mlocReadCatalog(catalog, &c) || (key == nullptr && length > 0) || indexOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    CatalogKeys keys;
    if (!mlocCatalogKeysOf(&c, &keys))
    {
        return mloc_errorFormat;
    }
    if (keys.count == 0)
    {
        return mloc_errorNotFound;
    }
    const uint8_t* bytes = length > 0 ? (const uint8_t*)key : (const uint8_t*)"";
    uint32_t index = mlocCatalogIndexOf(&keys, bytes, length);
    uint32_t stored = 0;
    const uint8_t* found = mlocCatalogKeyAt(&keys, index, &stored);
    if (stored != length || (length > 0 && memcmp(found, bytes, length) != 0))
    {
        return mloc_errorNotFound;
    }
    *indexOut = index;
    return mloc_success;
}

mlocResult mlocGetCatalogMessage(const mlocCatalog* catalog, uint32_t index,
                                 const void** messageOut, size_t* sizeOut)
{
    Catalog c;
    if (!mlocReadCatalog(catalog, &c) || messageOut == nullptr || sizeOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    uint32_t count = mlocLoad32(c.messages);
    if (index >= count)
    {
        return mloc_errorInvalid;
    }
    const uint8_t* offsets = c.messages + catalogMessagesHead;
    uint32_t start = mlocLoad32(offsets + (size_t)index * 4);
    uint32_t end = mlocLoad32(offsets + (size_t)index * 4 + 4);
    if (end == start)
    {
        return mloc_errorNotFound;
    }
    *messageOut = offsets + ((size_t)count + 1) * 4 + start;
    *sizeOut = end - start;
    return mloc_success;
}
