// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalogs (record mloc-0054): the units' validators and the lookups.

#include "maul-locale/catalog.h"

#include "bytes.h"
#include "catalog_tables.h"
#include "kinds.h"

#include <string.h>

enum
{
    // Marks a catalog the library opened.
    catalogStamp = 0x43,
};

// The opaque catalog's fields.
typedef struct Catalog
{
    const uint8_t* keys;
    const uint8_t* messages;
    const char* locale;
    uint32_t keysSize;
    uint32_t messagesSize;
    uint32_t localeLength;
    uint8_t stamp;
} Catalog;

static_assert(sizeof(Catalog) <= sizeof(mlocCatalog), "a catalog fits its public struct");

static bool ReadCatalog(const mlocCatalog* catalog, Catalog* out)
{
    if (catalog == nullptr)
    {
        return false;
    }
    memcpy(out, catalog, sizeof(*out));
    return out->stamp == catalogStamp;
}

// A catalog's keys; false only for a catalog whose data was not
// validated, which mlocOpenCatalog does not make.
static bool KeysOf(const Catalog* c, CatalogKeys* keys)
{
    return mlocReadCatalogKeys((DataSpan){c->keys, c->keysSize}, keys);
}

// The one locale with both units, as locale index and spans.
static mlocResult FindUnits(const DataView* view, uint32_t* localeOut, DataSpan* keys,
                            DataSpan* messages)
{
    uint32_t found = 0;
    for (uint32_t i = 0; i < view->localeCount; i++)
    {
        DataSpan k;
        DataSpan m;
        bool hasKeys = mlocFindDataUnit(view, kindCatalogKeys, i, &k);
        bool hasMessages = mlocFindDataUnit(view, kindCatalogMessages, i, &m);
        if (hasKeys != hasMessages)
        {
            return mloc_errorData;
        }
        if (hasKeys)
        {
            found++;
            *localeOut = i;
            *keys = k;
            *messages = m;
        }
    }
    return found == 0 ? mloc_errorNotFound : found > 1 ? mloc_errorData : mloc_success;
}

mlocResult mlocOpenCatalog(mlocCatalog* catalogOut, const mlocData* data)
{
    if (catalogOut == nullptr || data == nullptr)
    {
        return mloc_errorInvalid;
    }
    DataView view = mlocViewOf(data);
    uint32_t locale = 0;
    DataSpan keysSpan;
    DataSpan messagesSpan;
    mlocResult r = FindUnits(&view, &locale, &keysSpan, &messagesSpan);
    if (r != mloc_success)
    {
        return r;
    }
    CatalogKeys keys;
    if (!mlocReadCatalogKeys(keysSpan, &keys) || mlocLoad32(messagesSpan.bytes) != keys.count ||
        mlocLoad64(messagesSpan.bytes + 8) != keys.setId || mlocCatalogSetId(&keys) != keys.setId)
    {
        return mloc_errorData;
    }
    const char* id = nullptr;
    size_t length = 0;
    (void)mlocGetDataLocale(data, locale, &id, &length);
    Catalog c = {keysSpan.bytes,    messagesSpan.bytes, id,          keysSpan.size,
                 messagesSpan.size, (uint32_t)length,   catalogStamp};
    memset(catalogOut, 0, sizeof(*catalogOut));
    memcpy(catalogOut, &c, sizeof(c));
    return mloc_success;
}

mlocResult mlocGetCatalogInfo(const mlocCatalog* catalog, mlocCatalogInfo* infoOut)
{
    Catalog c;
    if (!ReadCatalog(catalog, &c) || infoOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    CatalogKeys keys;
    if (!KeysOf(&c, &keys))
    {
        return mloc_errorData;
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
        .source = (mlocLoad32(c.messages + 4) & catalogSourceFlag) != 0,
    };
    if (mlocParseLocale(c.locale, c.localeLength, &info.locale).status != mloc_success)
    {
        return mloc_errorData;
    }
    *infoOut = info;
    return mloc_success;
}

mlocResult mlocFindCatalogKey(const mlocCatalog* catalog, const char* key, size_t length,
                              uint32_t* indexOut)
{
    Catalog c;
    if (!ReadCatalog(catalog, &c) || (key == nullptr && length > 0) || indexOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    CatalogKeys keys;
    if (!KeysOf(&c, &keys))
    {
        return mloc_errorData;
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
    if (!ReadCatalog(catalog, &c) || messageOut == nullptr || sizeOut == nullptr)
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
