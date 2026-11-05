// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalog chains: catalogs of one set in fallback order, their messages
// bound once into the caller's cache. The cache is a head (the arena's
// bytes used), a slot of eight bytes per key index, and the arena the
// bound messages are written into, each at a multiple of eight. A cache
// whose every slot is bound is only read, so threads may share it.

#include "catalog_chain.h"

#include "catalog_tables.h"
#include "catalog_view.h"
#include "invariant.h"

#include "maul-locale/catalog.h"

#include <string.h>

// Where binding writes: the arena's room, or nothing when measuring.
typedef struct Binder
{
    uint8_t* arena;
    size_t room;
    size_t used;
} Binder;

// The cache is this process's memory, so it is read and written in the
// machine's own order, by copies for any alignment. Every caller's index
// is below the set's keys, whose slots mlocInitCatalogChain made room for.
static Slot LoadSlot(const Chain* c, uint32_t index)
{
    MLOC_ASSERT(index < c->keyCount);
    Slot slot;
    memcpy(&slot, c->cache + cacheHead + (size_t)index * slotSize, sizeof(slot));
    return slot;
}

static void StoreSlot(const Chain* c, uint32_t index, Slot slot)
{
    MLOC_ASSERT(index < c->keyCount);
    memcpy(c->cache + cacheHead + (size_t)index * slotSize, &slot, sizeof(slot));
}

static uint64_t LoadUsed(const Chain* c)
{
    uint64_t used = 0;
    memcpy(&used, c->cache, sizeof(used));
    return used;
}

static void StoreUsed(const Chain* c, size_t used)
{
    uint64_t value = used;
    memcpy(c->cache, &value, sizeof(value));
}

static size_t SlotsEnd(uint32_t keyCount)
{
    return cacheHead + (size_t)keyCount * slotSize;
}

// The arena's room after a cache's slots: its offsets are 32 bits, so room
// past 4 GiB goes unused.
static size_t ArenaRoom(const Chain* c)
{
    size_t room = c->cacheSize - SlotsEnd(c->keyCount);
    return room < UINT32_MAX ? room : UINT32_MAX;
}

// A chain the library made, copied out; false for any other, and for one
// whose counts or cache, which it keeps, pass their bounds: every
// count and the arena's bytes used are checked before anything is read
// through them.
MLOC_NODISCARD static bool ReadChain(const mlocCatalogChain* chain, Chain* out)
{
    if (chain == nullptr)
    {
        return false;
    }
    memcpy(out, chain, sizeof(*out));
    return out->stamp == chainStamp && out->catalogs != nullptr && out->formatters != nullptr &&
           out->cache != nullptr && out->count >= 1 && out->count <= MLOC_MAX_CATALOG_CHAIN &&
           out->source <= out->count &&
           cacheHead + (uint64_t)out->keyCount * slotSize <= out->cacheSize &&
           LoadUsed(out) <= ArenaRoom(out);
}

// Binds one message at the arena's end: mloc_errorCapacity when it does
// not fit, another error when it does not bind.
static mlocResult BindAt(Binder* b, const mlocMessageFormatter* formatter, const void* message,
                         size_t size, uint32_t* offsetOut)
{
    size_t need = 0;
    uint8_t* at = b->arena != nullptr ? b->arena + b->used : nullptr;
    size_t room = b->arena != nullptr ? b->room - b->used : 0;
    mlocResult r = mlocMessageFormatter_Bind(formatter, message, size, at, room, &need);
    if (b->arena == nullptr && r == mloc_errorCapacity)
    {
        r = mloc_success;
    }
    if (r != mloc_success)
    {
        return r;
    }
    *offsetOut = (uint32_t)b->used;
    size_t step = (need + 7) & ~(size_t)7;
    if (b->arena != nullptr && step > b->room - b->used)
    {
        step = b->room - b->used;
    }
    b->used += step;
    // The arena's offsets are 32 bits (BinderOf).
    MLOC_ASSERT(b->arena == nullptr || b->used <= b->room);
    return mloc_success;
}

// Binds a key index's message as the chain does: the first catalog that
// has it, else the source's when that one does not bind; the slot says
// which, or why the key stands instead. mloc_errorCapacity when a message
// does not fit.
static mlocResult BindIndex(const Chain* c, Binder* b, uint32_t index, Slot* out)
{
    // A slot's position is a byte.
    MLOC_ASSERT(c->count <= MLOC_MAX_CATALOG_CHAIN && index < c->keyCount);
    const void* message = nullptr;
    size_t size = 0;
    uint32_t first = 0;
    while (first < c->count &&
           mlocCatalog_GetMessage(&c->catalogs[first], index, &message, &size) != mloc_success)
    {
        first++;
    }
    if (first == c->count)
    {
        *out = (Slot){0, slotKey, (uint8_t)c->count, mloc_catalogAbsent};
        return mloc_success;
    }
    uint32_t offset = 0;
    mlocResult r = BindAt(b, &c->formatters[first], message, size, &offset);
    if (r == mloc_success)
    {
        mlocCatalogReason reason = first == 0 ? mloc_catalogFound : mloc_catalogFallback;
        *out = (Slot){offset, slotBound, (uint8_t)first, (uint8_t)reason};
        return mloc_success;
    }
    if (r == mloc_errorCapacity)
    {
        return r;
    }
    if (c->source < c->count && c->source != first &&
        mlocCatalog_GetMessage(&c->catalogs[c->source], index, &message, &size) == mloc_success)
    {
        r = BindAt(b, &c->formatters[c->source], message, size, &offset);
        if (r == mloc_success)
        {
            *out = (Slot){offset, slotBound, (uint8_t)c->source, mloc_catalogSource};
            return mloc_success;
        }
        if (r == mloc_errorCapacity)
        {
            return r;
        }
    }
    *out = (Slot){0, slotKey, (uint8_t)c->count, mloc_catalogFailed};
    return mloc_success;
}

// Checks the catalogs of a chain: of one set, the source's position.
static mlocResult CheckCatalogs(const mlocCatalog* catalogs, const mlocMessageFormatter* formatters,
                                size_t count, uint32_t* keyCountOut, uint32_t* sourceOut)
{
    if (catalogs == nullptr || formatters == nullptr || count == 0)
    {
        return mloc_errorInvalid;
    }
    if (count > MLOC_MAX_CATALOG_CHAIN)
    {
        return mloc_errorLimit;
    }
    uint32_t source = (uint32_t)count;
    mlocCatalogInfo first = {0};
    for (uint32_t i = 0; i < count; i++)
    {
        mlocCatalogInfo info;
        if (mlocCatalog_GetInfo(&catalogs[i], &info) != mloc_success ||
            (i > 0 && info.setId != first.setId))
        {
            return mloc_errorInvalid;
        }
        if (i == 0)
        {
            first = info;
        }
        if (info.source && source == count)
        {
            source = i;
        }
    }
    *keyCountOut = first.keyCount;
    *sourceOut = source;
    return mloc_success;
}

mlocResult mlocMeasureCatalogChain(const mlocCatalog* catalogs,
                                   const mlocMessageFormatter* formatters, size_t count,
                                   size_t* sizeOut)
{
    Chain c = {.catalogs = catalogs, .formatters = formatters, .count = (uint32_t)count};
    mlocResult r = sizeOut == nullptr
                       ? mloc_errorInvalid
                       : CheckCatalogs(catalogs, formatters, count, &c.keyCount, &c.source);
    if (r != mloc_success)
    {
        return r;
    }
    Binder b = {0};
    for (uint32_t i = 0; i < c.keyCount; i++)
    {
        Slot slot;
        (void)BindIndex(&c, &b, i, &slot);
    }
    *sizeOut = SlotsEnd(c.keyCount) + b.used;
    return mloc_success;
}

mlocResult mlocInitCatalogChain(const mlocCatalog* catalogs, const mlocMessageFormatter* formatters,
                                size_t count, void* cache, size_t cacheSize,
                                mlocCatalogChain* chainOut)
{
    Chain c = {catalogs, formatters, cache, cacheSize, 0, (uint32_t)count, 0, chainStamp};
    mlocResult r = chainOut == nullptr || cache == nullptr
                       ? mloc_errorInvalid
                       : CheckCatalogs(catalogs, formatters, count, &c.keyCount, &c.source);
    if (r != mloc_success)
    {
        return r;
    }
    if (cacheSize < SlotsEnd(c.keyCount))
    {
        return mloc_errorCapacity;
    }
    memset(cache, 0, SlotsEnd(c.keyCount));
    memset(chainOut, 0, sizeof(*chainOut));
    memcpy(chainOut, &c, sizeof(c));
    return mloc_success;
}

// The arena of a chain ReadChain read: its slots fit the cache, and its
// bytes used the room.
static Binder BinderOf(const Chain* c)
{
    return (Binder){c->cache + SlotsEnd(c->keyCount), ArenaRoom(c), (size_t)LoadUsed(c)};
}

mlocResult mlocCatalogChain_Bind(const mlocCatalogChain* chain)
{
    Chain c;
    if (!ReadChain(chain, &c))
    {
        return mloc_errorInvalid;
    }
    Binder b = BinderOf(&c);
    mlocResult r = mloc_success;
    for (uint32_t i = 0; i < c.keyCount; i++)
    {
        Slot slot = LoadSlot(&c, i);
        if (slot.state == slotUnbound)
        {
            mlocResult bound = BindIndex(&c, &b, i, &slot);
            if (bound == mloc_success)
            {
                StoreSlot(&c, i, slot);
            }
            else
            {
                r = bound;
            }
        }
    }
    StoreUsed(&c, b.used);
    return r;
}

// Writes the key as the text: one fallback part.
static mlocResult WriteKey(const uint8_t* key, size_t length, mlocOutput* output)
{
    output->length = length;
    if (length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (length > 0)
    {
        memcpy(output->buffer, key, length);
    }
    if (output->part != nullptr && length > 0)
    {
        output->part(output->context, mloc_messagePartFallback, 0, length);
    }
    return mloc_success;
}

static mlocResult WriteKeyAt(const Chain* c, uint32_t index, mlocOutput* output)
{
    Catalog catalog;
    CatalogKeys keys;
    if (!mlocReadCatalog(&c->catalogs[0], &catalog) || !mlocCatalogKeysOf(&catalog, &keys))
    {
        return mloc_errorFormat;
    }
    // A defense: the array must stay as it was (catalog.h), but a first
    // catalog changed against that must not send the read past its keys.
    if (index >= keys.count)
    {
        return mloc_errorInvalid;
    }
    uint32_t length = 0;
    const uint8_t* key = mlocCatalogKeyAt(&keys, index, &length);
    return WriteKey(key, length, output);
}

// Reports a reason and position once the text is written: a failure
// leaves the result as it was.
static mlocResult Report(mlocResult written, mlocCatalogResult* resultOut, mlocCatalogReason reason,
                         uint32_t position)
{
    if (written >= 0 && resultOut != nullptr)
    {
        *resultOut = (mlocCatalogResult){reason, position};
    }
    return written;
}

MLOC_NODISCARD static bool ValidOutput(const mlocOutput* output)
{
    return output != nullptr && (output->buffer != nullptr || output->capacity == 0);
}

mlocResult mlocCatalogChain_FormatMessage(const mlocCatalogChain* chain, uint32_t index,
                                          const mlocMessageArgument* arguments,
                                          size_t argumentCount, mlocOutput* textOut,
                                          mlocMessageFaults* faultsOut,
                                          mlocCatalogResult* resultOut)
{
    Chain c;
    if (!ReadChain(chain, &c) || index >= c.keyCount || !ValidOutput(textOut))
    {
        return mloc_errorInvalid;
    }
    Slot slot = LoadSlot(&c, index);
    if (slot.state == slotUnbound)
    {
        Binder b = BinderOf(&c);
        if (BindIndex(&c, &b, index, &slot) != mloc_success)
        {
            return Report(WriteKeyAt(&c, index, textOut), resultOut, mloc_catalogCacheFull,
                          c.count);
        }
        StoreSlot(&c, index, slot);
        StoreUsed(&c, b.used);
    }
    // A slot's reason and position reach the result: a cache changed so
    // that they name none is refused.
    if (slot.reason > mloc_catalogCacheFull || slot.position > c.count)
    {
        return mloc_errorInvalid;
    }
    mlocCatalogReason reason = (mlocCatalogReason)slot.reason;
    if (slot.state == slotKey)
    {
        return Report(WriteKeyAt(&c, index, textOut), resultOut, reason, slot.position);
    }
    // A bound slot's message lies in the arena the chain has used; the
    // cache must stay as the chain's calls wrote it (catalog.h), but one
    // changed against that is refused rather than read.
    if (slot.state != slotBound || slot.offset >= LoadUsed(&c))
    {
        return mloc_errorInvalid;
    }
    const uint8_t* bound = c.cache + SlotsEnd(c.keyCount) + slot.offset;
    return Report(mlocFormatMessage(bound, arguments, argumentCount, textOut, faultsOut), resultOut,
                  reason, slot.position);
}

mlocResult mlocCatalogChain_FormatKey(const mlocCatalogChain* chain, const char* key, size_t length,
                                      const mlocMessageArgument* arguments, size_t argumentCount,
                                      mlocOutput* textOut, mlocMessageFaults* faultsOut,
                                      mlocCatalogResult* resultOut)
{
    Chain c;
    if (!ReadChain(chain, &c) || (key == nullptr && length > 0) || !ValidOutput(textOut))
    {
        return mloc_errorInvalid;
    }
    uint32_t index = 0;
    mlocResult r = mlocCatalog_FindKey(&c.catalogs[0], key, length, &index);
    if (r == mloc_errorNotFound)
    {
        return Report(WriteKey((const uint8_t*)key, length, textOut), resultOut,
                      mloc_catalogMissingKey, c.count);
    }
    if (r != mloc_success)
    {
        return r;
    }
    return mlocCatalogChain_FormatMessage(chain, index, arguments, argumentCount, textOut,
                                          faultsOut, resultOut);
}
