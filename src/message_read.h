// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reads a compiled message (message_form.h) the caller gives, which may
// come from a file: the header against this format version, the fields
// against the size (a Unicode version, when one is there, against this
// library's), and every record's fields (kinds, indexes,
// ranges, strings in the pool as well-formed UTF-8, names as MF2 names),
// so that nothing read through the view leaves the bytes and every
// string can be written back as MF2 text; the strings the records name
// must fit the pool, as the compiler never shares one, and each
// expression and markup is named once in order, the ranges fitting
// their records, so that reading and writing a message back cost no
// more than its size. The data model's rules are
// the compiler's and are not checked again.

#ifndef MLOC_SRC_MESSAGE_READ_H
#define MLOC_SRC_MESSAGE_READ_H

#include "invariant.h"
#include "message_form.h"

#include "maul-locale/base.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct MessageView
{
    const uint8_t* bytes;
    uint32_t flags;
    uint32_t count[messageCount];
    uint32_t start[messageCount];
    uint32_t keyStart;
    const uint8_t* pool;
} MessageView;

// Reads a message: mloc_success; mloc_errorFormat for bytes that are not
// one; mloc_errorVersion for one a later version wrote (another format
// version, a flag or field this one does not know) or one that names
// another Unicode version.
mlocResult mlocReadMessage(const uint8_t* bytes, size_t size, MessageView* view);

// Reads a message's header and fields alone, not their records: what
// mlocReadMessage checks first, with its results.
mlocResult mlocReadMessageLayout(const uint8_t* bytes, size_t size, MessageView* view);

// A record's 32-bit field, or 0 for a record past the kind's count. A
// bound message's bytes were read once, at the bind, and the caller
// keeps them as they were; bytes changed since may name any record or
// string, so every read through the view stays within it whatever they
// hold (mlocMessageString, mlocMessageRange).
static inline uint32_t mlocMessageField(const MessageView* view, uint32_t kind, uint32_t index,
                                        uint32_t field)
{
    MLOC_ASSERT(field < messageRecordSizes[kind] / 4);
    if (index >= view->count[kind])
    {
        return 0;
    }
    return mlocLoad32(view->bytes + view->start[kind] + (size_t)index * messageRecordSizes[kind] +
                      field * 4);
}

// A string in the pool: its bytes and length.
typedef struct MessageText
{
    const uint8_t* text;
    uint32_t length;
} MessageText;

// The pool's string at an offset and length, or an empty one where they
// leave the pool.
static inline MessageText mlocMessageSpan(const MessageView* view, uint32_t offset, uint32_t length)
{
    uint32_t pool = view->count[messagePool];
    if (offset > pool || length > pool - offset)
    {
        return (MessageText){view->pool, 0};
    }
    return (MessageText){view->pool + offset, length};
}

// The string a record names: its offset at a field, its length at the
// next.
static inline MessageText mlocMessageString(const MessageView* view, uint32_t kind, uint32_t index,
                                            uint32_t field)
{
    return mlocMessageSpan(view, mlocMessageField(view, kind, index, field),
                           mlocMessageField(view, kind, index, field + 1));
}

// The run of records of a kind that a record names, its first at a field
// and its count at the next: the count, cut to the records there are.
static inline uint32_t mlocMessageRange(const MessageView* view, uint32_t kind, uint32_t index,
                                        uint32_t field, uint32_t of, uint32_t* firstOut)
{
    uint32_t first = mlocMessageField(view, kind, index, field);
    uint32_t count = mlocMessageField(view, kind, index, field + 1);
    uint32_t total = view->count[of];
    *firstOut = first;
    return first > total ? 0 : count < total - first ? count : total - first;
}

// The key of a variant for a selector: its kind and string, as stored.
uint32_t mlocMessageKey(const MessageView* view, uint32_t variant, uint32_t selector,
                        uint32_t* offsetOut, uint32_t* lengthOut);

#endif
