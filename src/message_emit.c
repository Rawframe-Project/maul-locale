// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_emit.h"

#include "bytes.h"

#include "maul-locale/base.h"
#include "maul-unicode/normalize.h"

#include <string.h>

static void Store32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

void mlocEmitCount(MessageEmit* emit)
{
    memset(emit, 0, sizeof *emit);
}

// The hash table's slots for up to count variables: at least twice as
// many, a power of two.
static uint64_t SlotCount(uint32_t count)
{
    uint64_t slots = 4;
    while (slots < (uint64_t)count * 2)
    {
        slots *= 2;
    }
    return slots;
}

// The bytes of the records before the variables.
static uint64_t RecordBytes(const MessageCounts* counts)
{
    uint64_t total = messageHeaderSize + (uint64_t)counts->keys * messageKeySize;
    for (uint32_t kind = 0; kind < messageVariables; kind++)
    {
        total += (uint64_t)counts->used[kind] * messageRecordSizes[kind];
    }
    return total;
}

size_t mlocEmitNeeded(const MessageCounts* counts)
{
    uint64_t total =
        RecordBytes(counts) +
        (uint64_t)counts->used[messageVariables] * messageRecordSizes[messageVariables] +
        counts->used[messagePool];
    total = (total + 3) & ~(uint64_t)3;
    total += SlotCount(counts->used[messageVariables]) * 4;
    return total > UINT32_MAX ? 0 : (size_t)total;
}

void mlocEmitWrite(MessageEmit* emit, uint8_t* out, const MessageCounts* counts)
{
    memset(emit, 0, sizeof *emit);
    emit->out = out;
    uint32_t at = messageHeaderSize;
    for (uint32_t kind = 0; kind < messageCount; kind++)
    {
        if (kind == messageVariants)
        {
            emit->keyStart = at;
            at += counts->keys * messageKeySize;
        }
        emit->start[kind] = at;
        at += counts->used[kind] * messageRecordSizes[kind];
    }
    emit->poolEnd = at;
    emit->slotStart = (at + 3) & ~3u;
    emit->slotCount = (uint32_t)SlotCount(counts->used[messageVariables]);
    memset(out + emit->slotStart, 0, (size_t)emit->slotCount * 4);
}

uint32_t mlocEmitFinish(MessageEmit* emit, uint32_t flags, uint32_t unicode)
{
    uint8_t* out = emit->out;
    uint32_t pool = emit->start[messageVariables] +
                    emit->used[messageVariables] * messageRecordSizes[messageVariables];
    memmove(out + pool, out + emit->start[messagePool], emit->used[messagePool]);
    uint32_t size = pool + emit->used[messagePool];
    memcpy(out, MLOC_MESSAGE_MAGIC, 4);
    out[messageFormat] = (uint8_t)messageFormatVersion;
    out[messageFormat + 1] = 0;
    out[messageFlags] = (uint8_t)flags;
    out[messageFlags + 1] = 0;
    Store32(out + messageSize, size);
    Store32(out + messageLibrary, (uint32_t)MLOC_VERSION_MAJOR << 16 |
                                      (uint32_t)MLOC_VERSION_MINOR << 8 | MLOC_VERSION_PATCH);
    Store32(out + messageUnicode, unicode);
    for (uint32_t kind = 0; kind < messageCount; kind++)
    {
        Store32(out + messageCounts + kind * 4, emit->used[kind]);
    }
    return size;
}

uint32_t mlocEmitRecord(MessageEmit* emit, uint32_t kind, const uint32_t* fields)
{
    uint32_t index = emit->used[kind]++;
    if (emit->out != NULL)
    {
        uint32_t words = messageRecordSizes[kind] / 4;
        uint8_t* p = emit->out + emit->start[kind] + index * messageRecordSizes[kind];
        for (uint32_t word = 0; word < words; word++)
        {
            Store32(p + word * 4, fields[word]);
        }
    }
    return index;
}

void mlocEmitKey(MessageEmit* emit, uint32_t kind, MessageString string)
{
    uint32_t index = emit->keys++;
    if (emit->out != NULL)
    {
        uint8_t* p = emit->out + emit->keyStart + index * messageKeySize;
        Store32(p, kind);
        Store32(p + 4, string.offset);
        Store32(p + 8, string.length);
    }
}

MessageString mlocEmitBeginString(const MessageEmit* emit)
{
    MessageString string = {emit->used[messagePool], 0};
    return string;
}

void mlocEmitBytes(MessageEmit* emit, const uint8_t* bytes, size_t length)
{
    if (emit->out != NULL && length > 0)
    {
        memcpy(emit->out + emit->start[messagePool] + emit->used[messagePool], bytes, length);
    }
    emit->used[messagePool] += (uint32_t)length;
}

bool mlocEmitNfc(MessageEmit* emit, const uint8_t* bytes, size_t length)
{
    size_t ascii = 0;
    while (ascii < length && bytes[ascii] < 0x80)
    {
        ascii++;
    }
    if (ascii == length)
    {
        mlocEmitBytes(emit, bytes, length);
        return true;
    }
    size_t needed = 0;
    char* to = NULL;
    size_t capacity = 0;
    if (emit->out != NULL)
    {
        to = (char*)emit->out + emit->start[messagePool] + emit->used[messagePool];
        // The counting pass measured this very text, so it fits.
        capacity = emit->poolEnd - emit->start[messagePool] - emit->used[messagePool];
    }
    muniTextResult result = muniNormalize((const char*)bytes, length, muni_nfc, muni_convertStrict,
                                          to, capacity, &needed);
    if (result.status != muni_success && result.status != muni_errorCapacity)
    {
        return false;
    }
    emit->used[messagePool] += (uint32_t)needed;
    return true;
}

MessageString mlocEmitEndString(const MessageEmit* emit, MessageString string)
{
    string.length = emit->used[messagePool] - string.offset;
    return string;
}

static uint32_t Hash(const uint8_t* bytes, uint32_t length)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < length; i++)
    {
        hash = (hash ^ bytes[i]) * 16777619u;
    }
    return hash;
}

bool mlocEmitVariable(MessageEmit* emit, const uint8_t* name, size_t length, uint32_t* indexOut)
{
    MessageString string = mlocEmitBeginString(emit);
    if (!mlocEmitNfc(emit, name, length))
    {
        return false;
    }
    string = mlocEmitEndString(emit, string);
    if (emit->out == NULL)
    {
        *indexOut = emit->used[messageVariables]++;
        return true;
    }
    const uint8_t* pool = emit->out + emit->start[messagePool];
    uint32_t mask = emit->slotCount - 1;
    uint32_t slot = Hash(pool + string.offset, string.length) & mask;
    for (;;)
    {
        uint8_t* p = emit->out + emit->slotStart + slot * 4;
        uint32_t entry = mlocLoad32(p);
        if (entry == 0)
        {
            uint32_t fields[3] = {string.offset, string.length, variableExternal};
            uint32_t index = mlocEmitRecord(emit, messageVariables, fields);
            Store32(p, index + 1);
            *indexOut = index;
            return true;
        }
        const uint8_t* record = emit->out + emit->start[messageVariables] +
                                (entry - 1) * messageRecordSizes[messageVariables];
        uint32_t offset = mlocLoad32(record);
        uint32_t known = mlocLoad32(record + 4);
        if (known == string.length && memcmp(pool + offset, pool + string.offset, known) == 0)
        {
            // Known: the name's copy is dropped from the pool.
            emit->used[messagePool] = string.offset;
            *indexOut = entry - 1;
            return true;
        }
        slot = (slot + 1) & mask;
    }
}
