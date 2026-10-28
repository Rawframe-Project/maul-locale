// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_emit.h"

#include "bytes.h"
#include "invariant.h"
#include "message_text.h"

#include "maul-locale/message.h"
#include "maul-unicode/normalize.h"
#include "maul-unicode/properties.h"

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

// The table's slots: enough for the variables, for the parameters of an
// expression or a markup, and for the variants.
static uint64_t SlotCount(const MessageCounts* counts)
{
    uint32_t most = counts->used[messageVariables];
    most = counts->used[messageParameters] > most ? counts->used[messageParameters] : most;
    most = counts->used[messageVariants] > most ? counts->used[messageVariants] : most;
    return mlocTableSlots(most);
}

// A field's payload length: the records counted, the keys', or, for the
// Unicode version, its room.
static uint64_t LengthOf(const MessageCounts* counts, uint32_t field)
{
    if (field == messageFieldKeys)
    {
        return (uint64_t)counts->keys * messageKeySize;
    }
    if (field == messageFieldUnicode)
    {
        return messageUnicodeSize;
    }
    return (uint64_t)counts->used[field] * messageRecordSizes[field];
}

// Where each field's head goes (UINT64_MAX for none), the variables and
// the pool at their most and the Unicode version's room kept: the bytes
// the message takes at most.
static uint64_t Layout(const MessageCounts* counts, uint64_t* heads)
{
    uint64_t at = assetHeaderSize;
    for (uint32_t place = 0; place < messageFieldCount; place++)
    {
        uint64_t length = LengthOf(counts, messageFieldKinds[place]);
        heads[place] = length == 0 ? UINT64_MAX : at;
        at += length == 0 ? 0 : mlocAssetFieldBytes(length);
    }
    return at;
}

size_t mlocEmitNeeded(const MessageCounts* counts)
{
    uint64_t heads[messageFieldCount];
    uint64_t total = Layout(counts, heads);
    // The words: two a variable for the check, then one a variable and
    // two an expression for the costs.
    uint64_t variables = counts->used[messageVariables];
    uint64_t costs = variables + 2 * (uint64_t)counts->used[messageExpressions];
    total +=
        SlotCount(counts) * messageSlotSize + 4 * (costs > 2 * variables ? costs : 2 * variables);
    return total > MLOC_MAX_MESSAGE_SIZE ? 0 : (size_t)total;
}

void mlocEmitWrite(MessageEmit* emit, uint8_t* out, const MessageCounts* counts)
{
    memset(emit, 0, sizeof *emit);
    emit->out = out;
    uint64_t heads[messageFieldCount];
    // mlocEmitNeeded kept the whole within 32 bits.
    uint32_t end = (uint32_t)Layout(counts, heads);
    for (uint32_t place = 0; place < messageFieldCount; place++)
    {
        uint32_t field = messageFieldKinds[place];
        emit->head[place] = heads[place] == UINT64_MAX ? UINT32_MAX : (uint32_t)heads[place];
        uint32_t payload = emit->head[place] + assetFieldHead;
        if (heads[place] == UINT64_MAX || field == messageFieldUnicode)
        {
            continue;
        }
        if (field == messageFieldKeys)
        {
            emit->keyStart = payload;
        }
        else
        {
            emit->start[field] = payload;
        }
    }
    emit->poolEnd = emit->start[messagePool] + counts->used[messagePool];
    emit->slotCount = (uint32_t)SlotCount(counts);
    emit->wordStart = end + emit->slotCount * messageSlotSize;
    mlocTableInit(&emit->table, out + end, emit->slotCount);
}

// Writes a field's head at at and zeros its padding; where the next
// field goes.
static uint32_t PutField(uint8_t* out, uint32_t at, uint32_t tag, uint32_t length)
{
    Store32(out + at, tag);
    Store32(out + at + 4, length);
    uint32_t payloadEnd = at + assetFieldHead + length;
    uint32_t end = at + (uint32_t)mlocAssetFieldBytes(length);
    memset(out + payloadEnd, 0, end - payloadEnd);
    return end;
}

uint32_t mlocEmitFinish(MessageEmit* emit, uint32_t flags, uint32_t unicode)
{
    uint8_t* out = emit->out;
    MessageCounts written;
    memcpy(written.used, emit->used, sizeof written.used);
    written.keys = emit->keys;
    uint32_t at = assetHeaderSize;
    uint32_t fields = 0;
    for (uint32_t place = 0; place < messageFieldCount; place++)
    {
        uint32_t field = messageFieldKinds[place];
        uint32_t length = (uint32_t)LengthOf(&written, field);
        if (field == messageFieldUnicode)
        {
            if (!emit->unassigned)
            {
                continue;
            }
            Store32(out + at + assetFieldHead, unicode);
        }
        else if (length == 0)
        {
            continue;
        }
        else if (field == messagePool)
        {
            // A name bound once shrank the variables: the pool moves down.
            memmove(out + at + assetFieldHead, out + emit->start[messagePool], length);
        }
        else
        {
            // The counts placed every field before the pool.
            MLOC_ASSERT(at == emit->head[place]);
        }
        at = PutField(out, at, messageTags[place], length);
        fields++;
    }
    memcpy(out, MLOC_MESSAGE_MAGIC, 4);
    out[assetFormat] = (uint8_t)messageFormatVersion;
    out[assetFormat + 1] = 0;
    out[assetFlags] = (uint8_t)flags;
    out[assetFlags + 1] = 0;
    Store32(out + assetSize, at);
    Store32(out + assetFields, fields);
    return at;
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

// Whether well-formed text holds a code point this Unicode version leaves
// unassigned. Noncharacters are unassigned for good, so their NFC never
// changes.
static bool HasUnassigned(const uint8_t* text, size_t length)
{
    for (size_t at = 0; at < length;)
    {
        size_t size;
        uint32_t c = mlocMessageDecode(text, at, length, &size);
        bool noncharacter = (c & 0xFFFE) == 0xFFFE || (c >= 0xFDD0 && c <= 0xFDEF);
        if (!noncharacter && muniGetGeneralCategory(c) == muni_gcCn)
        {
            return true;
        }
        at += size;
    }
    return false;
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
    MLOC_ASSERT(emit->out == NULL || result.status == muni_success);
    if (emit->out != NULL && HasUnassigned((const uint8_t*)to, needed))
    {
        emit->unassigned = true;
    }
    emit->used[messagePool] += (uint32_t)needed;
    return true;
}

MessageString mlocEmitEndString(const MessageEmit* emit, MessageString string)
{
    string.length = emit->used[messagePool] - string.offset;
    return string;
}

// A variable's name, while its record is not written yet.
typedef struct NameSearch
{
    const MessageEmit* emit;
    MessageString name;
} NameSearch;

static bool SameName(const void* context, uint32_t entry, uint32_t candidate)
{
    (void)candidate;
    const NameSearch* search = context;
    const MessageEmit* emit = search->emit;
    const uint8_t* pool = emit->out + emit->start[messagePool];
    const uint8_t* record =
        emit->out + emit->start[messageVariables] + entry * messageRecordSizes[messageVariables];
    uint32_t length = mlocLoad32(record + 4);
    return length == search->name.length &&
           memcmp(pool + mlocLoad32(record), pool + search->name.offset, length) == 0;
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
    NameSearch search = {emit, string};
    uint32_t next = emit->used[messageVariables];
    uint32_t found = mlocTableFindOrAdd(
        &emit->table, mlocTableHash(2166136261u, pool + string.offset, string.length), next,
        SameName, &search);
    if (found != UINT32_MAX)
    {
        // Known: the name's copy is dropped from the pool.
        emit->used[messagePool] = string.offset;
        *indexOut = found;
        return true;
    }
    uint32_t fields[3] = {string.offset, string.length, variableExternal};
    *indexOut = mlocEmitRecord(emit, messageVariables, fields);
    return true;
}
