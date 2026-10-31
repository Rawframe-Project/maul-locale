// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_table.h"

#include "bytes.h"
#include "invariant.h"

#include <stddef.h>
#include <string.h>

static void Store32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

uint64_t mlocTableSlots(uint32_t count)
{
    uint64_t slots = 4;
    while (slots < (uint64_t)count * 2)
    {
        slots *= 2;
    }
    return slots;
}

void mlocTableInit(MessageTable* table, uint8_t* memory, uint32_t slotCount)
{
    // mlocTableSlots: a power of two, 4 or more.
    MLOC_ASSERT(slotCount >= 4 && (slotCount & (slotCount - 1)) == 0);
    table->slots = memory;
    table->mask = slotCount - 1;
    table->stamp = 1;
    memset(memory, 0, (size_t)slotCount * messageSlotSize);
}

void mlocTableClear(MessageTable* table)
{
    table->stamp++;
    if (table->stamp == 0)
    {
        // Once in 2^32 clears: the stamps start again from empty slots.
        memset(table->slots, 0, ((size_t)table->mask + 1) * messageSlotSize);
        table->stamp = 1;
    }
}

uint32_t mlocTableFindOrAdd(MessageTable* table, uint32_t hash, uint32_t candidate,
                            MessageSame same, const void* context)
{
    uint32_t slot = hash & table->mask;
    // The slots are at least twice the entries added between clears (the
    // variables, an expression's or a markup's parameters, or the
    // variants), so a probe finds an empty slot before it wraps.
    uint32_t probes = 0;
    for (;; probes++)
    {
        MLOC_ASSERT(probes <= table->mask);
        uint8_t* p = table->slots + (size_t)slot * messageSlotSize;
        if (mlocLoad32(p) != table->stamp)
        {
            Store32(p, table->stamp);
            Store32(p + 4, candidate);
            return UINT32_MAX;
        }
        uint32_t entry = mlocLoad32(p + 4);
        if (same(context, entry, candidate))
        {
            return entry;
        }
        slot = (slot + 1) & table->mask;
    }
}

uint32_t mlocTableHash(uint32_t hash, const uint8_t* bytes, uint32_t length)
{
    for (uint32_t i = 0; i < length; i++)
    {
        hash = (hash ^ bytes[i]) * 16777619u;
    }
    return hash;
}
