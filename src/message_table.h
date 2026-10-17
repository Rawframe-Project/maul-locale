// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A hash set of 32-bit entries in caller memory, for compiling messages
// in linear time: open addressing with linear probing, each slot a stamp
// and an entry. Advancing the stamp empties the set at once, so one table
// serves one group after another (the options of each expression).

#ifndef MLOC_SRC_MESSAGE_TABLE_H
#define MLOC_SRC_MESSAGE_TABLE_H

#include <stdbool.h>
#include <stdint.h>

// Whether a stored entry equals the candidate.
typedef bool (*MessageSame)(const void* context, uint32_t entry, uint32_t candidate);

typedef struct MessageTable
{
    uint8_t* slots;
    uint32_t mask;
    uint32_t stamp;
} MessageTable;

// The bytes of a table of slotCount slots.
static const uint32_t messageSlotSize = 8;

// The slots for up to count entries: at least twice as many, a power of
// two, at least 4.
uint64_t mlocTableSlots(uint32_t count);

// Starts an empty table over memory of slotCount slots (a power of two).
void mlocTableInit(MessageTable* table, uint8_t* memory, uint32_t slotCount);

// Empties the table.
void mlocTableClear(MessageTable* table);

// The entry equal to candidate (by same) if the table holds one; else
// adds candidate and returns UINT32_MAX. The table must have room.
uint32_t mlocTableFindOrAdd(MessageTable* table, uint32_t hash, uint32_t candidate,
                            MessageSame same, const void* context);

// FNV-1a over bytes, continuing from hash (2166136261 to start).
uint32_t mlocTableHash(uint32_t hash, const uint8_t* bytes, uint32_t length);

#endif
