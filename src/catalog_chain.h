// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A catalog chain's fields and its cache's slots (catalog_chain.c), which
// a white-box test forges.

#ifndef MLOC_CATALOG_CHAIN_H
#define MLOC_CATALOG_CHAIN_H

#include "maul-locale/catalog.h"
#include "maul-locale/message.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // Marks a chain the library made.
    chainStamp = 0x4B,
    cacheHead = 16,
    slotSize = 8,
    // A slot's states: no bind tried yet, a bound message, the key.
    slotUnbound = 0,
    slotBound = 1,
    slotKey = 2,
};

// The opaque chain's fields.
typedef struct Chain
{
    const mlocCatalog* catalogs;
    const mlocMessageFormatter* formatters;
    uint8_t* cache;
    size_t cacheSize;
    uint32_t keyCount;
    uint32_t count;
    // The source's position, or count when the chain lacks it.
    uint32_t source;
    uint8_t stamp;
} Chain;

static_assert(sizeof(Chain) <= sizeof(mlocCatalogChain), "a chain fits its public struct");

// A key index's slot: where its bound message is, from which position and
// why, or the reason its key is written.
typedef struct Slot
{
    uint32_t offset;
    uint8_t state;
    uint8_t position;
    uint8_t reason;
} Slot;

static_assert(sizeof(Slot) == slotSize, "a slot is eight bytes");

#endif
