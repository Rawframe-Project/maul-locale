// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Catalog chains: catalogs of one set in fallback order, their messages
// bound once into the caller's cache. The cache is a head (the arena's
// bytes used), a slot of eight bytes per key index, and the arena the
// bound messages are written into, each at a multiple of eight. A cache
// whose every slot is bound is only read, so threads may share it.

#include "catalog_tables.h"
#include "catalog_view.h"
#include "invariant.h"

#include "maul-locale/catalog.h"

#include <string.h>

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

// Where binding writes: the arena's room, or nothing when measuring.
typedef struct Binder
{
    uint8_t* arena;
    size_t room;
    size_t used;
} Binder;

static bool ReadChain(const mlocCatalogChain* chain, Chain* out)
{
    if (chain == nullptr)
    {
        return false;
    }
    memcpy(out, chain, sizeof(*out));
    return out->stamp == chainStamp;
}

// The cache is this process's memory, so it is read and written in the
// machine's own order, by copies for any alignment.
static Slot LoadSlot(const Chain* c, uint32_t index)
{
    Slot slot;
    memcpy(&slot, c->cache + cacheHead + (size_t)index * slotSize, sizeof(slot));
    return slot;
}

static void StoreSlot(const Chain* c, uint32_t index, Slot slot)
{
    memcpy(c->cache + cacheHead + (size_t)index * slotSize, &slot, sizeof(slot));
}

static size_t LoadUsed(const Chain* c)
{
    uint64_t used = 0;
    memcpy(&used, c->cache, sizeof(used));
    return (size_t)used;
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

// Binds one message at the arena's end: mloc_errorCapacity when it does
// not fit, another error when it does not bind.
static mlocResult BindAt(Binder* b, const mlocMessageFormatter* formatter, const void* message,
                         size_t size, uint32_t* offsetOut)
{
    size_t need = 0;
    uint8_t* at = b->arena != nullptr ? b->arena + b->used : nullptr;
    size_t room = b->arena != nullptr ? b->room - b->used : 0;
    mlocResult r = mlocBindMessage(formatter, message, size, at, room, &need);
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
           mlocGetCatalogMessage(&c->catalogs[first], index, &message, &size) != mloc_success)
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
        mlocGetCatalogMessage(&c->catalogs[c->source], index, &message, &size) == mloc_success)
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
        if (mlocGetCatalogInfo(&catalogs[i], &info) != mloc_success ||
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

mlocResult mlocInitCatalogChain(mlocCatalogChain* chainOut, const mlocCatalog* catalogs,
                                const mlocMessageFormatter* formatters, size_t count, void* cache,
                                size_t cacheSize)
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

// The arena: its offsets are 32 bits, so room past 4 GiB goes unused.
static Binder BinderOf(const Chain* c)
{
    size_t start = SlotsEnd(c->keyCount);
    size_t room = c->cacheSize - start;
    return (Binder){c->cache + start, room < UINT32_MAX ? room : UINT32_MAX, LoadUsed(c)};
}

mlocResult mlocBindCatalogChain(const mlocCatalogChain* chain)
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
    uint32_t length = 0;
    const uint8_t* key = mlocCatalogKeyAt(&keys, index, &length);
    return WriteKey(key, length, output);
}

static void Report(mlocCatalogResult* resultOut, mlocCatalogReason reason, uint32_t position)
{
    if (resultOut != nullptr)
    {
        *resultOut = (mlocCatalogResult){reason, position};
    }
}

static bool ValidOutput(const mlocOutput* output)
{
    return output != nullptr && (output->buffer != nullptr || output->capacity == 0);
}

mlocResult mlocFormatCatalogMessage(const mlocCatalogChain* chain, uint32_t index,
                                    const mlocMessageArgument* arguments, size_t argumentCount,
                                    mlocOutput* output, mlocMessageFaults* faults,
                                    mlocCatalogResult* resultOut)
{
    Chain c;
    if (!ReadChain(chain, &c) || index >= c.keyCount || !ValidOutput(output))
    {
        return mloc_errorInvalid;
    }
    Slot slot = LoadSlot(&c, index);
    if (slot.state == slotUnbound)
    {
        Binder b = BinderOf(&c);
        if (BindIndex(&c, &b, index, &slot) != mloc_success)
        {
            Report(resultOut, mloc_catalogCacheFull, c.count);
            return WriteKeyAt(&c, index, output);
        }
        StoreSlot(&c, index, slot);
        StoreUsed(&c, b.used);
    }
    Report(resultOut, (mlocCatalogReason)slot.reason, slot.position);
    if (slot.state == slotKey)
    {
        return WriteKeyAt(&c, index, output);
    }
    // A bound slot's message lies in the arena the chain has used.
    MLOC_ASSERT(slot.state == slotBound && slot.offset < LoadUsed(&c));
    const uint8_t* bound = c.cache + SlotsEnd(c.keyCount) + slot.offset;
    return mlocFormatMessage(bound, arguments, argumentCount, output, faults);
}

mlocResult mlocFormatCatalogKey(const mlocCatalogChain* chain, const char* key, size_t length,
                                const mlocMessageArgument* arguments, size_t argumentCount,
                                mlocOutput* output, mlocMessageFaults* faults,
                                mlocCatalogResult* resultOut)
{
    Chain c;
    if (!ReadChain(chain, &c) || (key == nullptr && length > 0) || !ValidOutput(output))
    {
        return mloc_errorInvalid;
    }
    uint32_t index = 0;
    mlocResult r = mlocFindCatalogKey(&c.catalogs[0], key, length, &index);
    if (r == mloc_errorNotFound)
    {
        Report(resultOut, mloc_catalogMissingKey, c.count);
        return WriteKey((const uint8_t*)key, length, output);
    }
    if (r != mloc_success)
    {
        return r;
    }
    return mlocFormatCatalogMessage(chain, index, arguments, argumentCount, output, faults,
                                    resultOut);
}
