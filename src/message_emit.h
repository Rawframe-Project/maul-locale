// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Writes a compiled message's records (message_form.h) in two passes
// over the same parse: the first counts them and the bytes of their
// strings, the second writes them where the counts place them. Past the
// message, the caller's buffer holds scratch while compiling: a hash
// table (variables once per name, then message_check's duplicates) and
// two words a variable for message_check.

#ifndef MLOC_SRC_MESSAGE_EMIT_H
#define MLOC_SRC_MESSAGE_EMIT_H

#include "message_form.h"
#include "message_table.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A string in the pool.
typedef struct MessageString
{
    uint32_t offset;
    uint32_t length;
} MessageString;

typedef struct MessageEmit
{
    // The buffer while writing; null while counting.
    uint8_t* out;
    // Records written of each kind (bytes for the pool), and keys.
    uint32_t used[messageCount];
    uint32_t keys;
    // While writing: each kind's byte offset, the keys', the pool's end,
    // and the scratch: the table's slots, then the variables' words.
    uint32_t start[messageCount];
    uint32_t keyStart;
    uint32_t poolEnd;
    uint32_t slotCount;
    uint32_t wordStart;
    MessageTable table;
} MessageEmit;

// The counts of a counting pass, as upper bounds for writing.
typedef struct MessageCounts
{
    uint32_t used[messageCount];
    uint32_t keys;
} MessageCounts;

// Starts a counting pass.
void mlocEmitCount(MessageEmit* emit);

// The capacity a writing pass needs for what a counting pass counted:
// the message at its largest and the scratch; 0 past 2^32 - 1.
size_t mlocEmitNeeded(const MessageCounts* counts);

// Starts a writing pass into out, which holds mlocEmitNeeded bytes.
void mlocEmitWrite(MessageEmit* emit, uint8_t* out, const MessageCounts* counts);

// Ends a writing pass: moves the pool down over the variables it did not
// need, writes the header, and returns the message's size. The scratch
// stays where it was, for message_check.
uint32_t mlocEmitFinish(MessageEmit* emit, uint32_t flags, uint32_t unicode);

// Appends a record of a kind (not the pool), its fields as 32-bit words,
// and returns its index.
uint32_t mlocEmitRecord(MessageEmit* emit, uint32_t kind, const uint32_t* fields);

// Appends a key record.
void mlocEmitKey(MessageEmit* emit, uint32_t kind, MessageString string);

// Starts a string at the pool's end.
MessageString mlocEmitBeginString(const MessageEmit* emit);

// Appends bytes to the pool as they are.
void mlocEmitBytes(MessageEmit* emit, const uint8_t* bytes, size_t length);

// Appends bytes to the pool in NFC; false when Maul Unicode refuses them
// (a run of combining marks too long to normalize).
bool mlocEmitNfc(MessageEmit* emit, const uint8_t* bytes, size_t length);

// Ends a string begun by mlocEmitBeginString.
MessageString mlocEmitEndString(const MessageEmit* emit, MessageString string);

// The variable of a name (NFC), added when new; false as mlocEmitNfc.
bool mlocEmitVariable(MessageEmit* emit, const uint8_t* name, size_t length, uint32_t* indexOut);

#endif
