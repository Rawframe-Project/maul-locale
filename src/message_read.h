// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Reads a compiled message (message_form.h) the caller gives, which may
// come from a file: the header against this library and its Unicode, the
// counts against the size, and every record's fields (kinds, indexes,
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

// Reads a message: mloc_success; mloc_errorData for bytes that are not
// one; mloc_errorDataVersion for one of another library or Unicode
// version.
mlocResult mlocReadMessage(const uint8_t* bytes, size_t size, MessageView* view);

// A record's 32-bit field.
static inline uint32_t mlocMessageField(const MessageView* view, uint32_t kind, uint32_t index,
                                        uint32_t field)
{
    return mlocLoad32(view->bytes + view->start[kind] + index * messageRecordSizes[kind] +
                      field * 4);
}

// The key of a variant for a selector: its kind and string.
uint32_t mlocMessageKey(const MessageView* view, uint32_t variant, uint32_t selector,
                        uint32_t* offsetOut, uint32_t* lengthOut);

#endif
