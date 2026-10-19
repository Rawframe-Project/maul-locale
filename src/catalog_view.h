// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// An opened catalog's fields (record mloc-0054), read by the catalog's
// lookups and its chains.

#ifndef MLOC_CATALOG_VIEW_H
#define MLOC_CATALOG_VIEW_H

#include "catalog_tables.h"

#include "maul-locale/catalog.h"

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

static inline bool mlocReadCatalog(const mlocCatalog* catalog, Catalog* out)
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
static inline bool mlocCatalogKeysOf(const Catalog* c, CatalogKeys* keys)
{
    return mlocReadCatalogKeys((DataSpan){c->keys, c->keysSize}, keys);
}

#endif
