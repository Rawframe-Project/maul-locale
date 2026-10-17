// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Binding a message (maul-locale/message.h, record mloc-0045): the words,
// then the number functions' values prepared.

#include "message_prepare.h"

#include "bytes.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void Store32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

// Whether a slot is a number function's, and so prepared.
static bool Prepared(uint32_t slot)
{
    return slot >= messageFunctionNumber && slot <= messageFunctionUnit;
}

// The bytes kept for an expression's prepared value: a unit formatter's
// room for :unit, a currency formatter's for :currency and for :offset
// (whose value may be a currency's; an offset of a unit value is made in
// the call), a number formatter's for the rest.
static size_t RoomOf(uint32_t slot)
{
    return mlocPreparedSize(slot == messageFunctionUnit ? preparedUnit
                            : slot == messageFunctionCurrency || slot == messageFunctionOffset
                                ? preparedCurrency
                                : preparedNumber);
}

// The offset of an expression's prepared-value word in the words.
static size_t PreparedWord(const MessageView* v, uint32_t expression)
{
    return 4 * ((size_t)v->count[messageVariables] + 2 * (size_t)v->count[messageExpressions] +
                expression);
}

// Resolves each number function's expression with probe arguments and
// keeps what its value needs, at 8-aligned places of the buffer; an
// expression whose value changes from call to call, or whose formatter
// cannot be made, keeps none.
static void Prepare(void* buffer, const BoundHeader* h, const MessageFormatter* f)
{
    Resolution r;
    memset(&r, 0, sizeof r);
    if (!mlocReadBound(buffer, &r.h, &r.formatter))
    {
        return;
    }
    r.words = mlocBoundWords(buffer);
    r.budget = messageResolutionLimit;
    r.probe = true;
    uint8_t* bytes = buffer;
    uint64_t base = mlocBoundBase(&h->view);
    size_t at = (size_t)base + (size_t)((8 - ((uintptr_t)bytes + base) % 8) % 8);
    uint8_t* words = bytes + sizeof *h;
    PreparedNumber prepared;
    for (uint32_t e = 0; e < h->view.count[messageExpressions]; e++)
    {
        uint32_t slot =
            mlocLoad32(words + 4 * ((size_t)h->view.count[messageVariables] + e)) & 0xFFFFFFu;
        if (!Prepared(slot))
        {
            continue;
        }
        size_t room = RoomOf(slot);
        r.dynamic = false;
        Value v = mlocResolveExpression(&r, e);
        if (r.exhausted)
        {
            return;
        }
        if (!r.dynamic && v.kind == valueNumber && v.number.origin == e &&
            mlocPrepareNumber(&v.number, f->data, &f->locale, &prepared) &&
            mlocPreparedSize((PreparedKind)prepared.kind) <= room)
        {
            memcpy(bytes + at, &prepared, mlocPreparedSize((PreparedKind)prepared.kind));
            Store32(words + PreparedWord(&h->view, e), (uint32_t)at);
        }
        at += room;
    }
}

mlocResult mlocBindMessage(const mlocMessageFormatter* formatter, const void* message, size_t size,
                           void* buffer, size_t capacity, size_t* sizeOut)
{
    MessageFormatter f;
    BoundHeader h;
    if (sizeOut == NULL || (buffer == NULL && capacity > 0))
    {
        return mloc_errorInvalid;
    }
    mlocResult result = mlocOpenBind(formatter, message, size, &f, &h);
    if (result != mloc_success)
    {
        return result;
    }
    // The words, then room to align the prepared values, then theirs.
    uint64_t needed = mlocBoundBase(&h.view) + 7;
    for (uint32_t e = 0; e < h.view.count[messageExpressions]; e++)
    {
        uint32_t slot = mlocExpressionSlot(&h.view, &f, e);
        needed += Prepared(slot) ? RoomOf(slot) : 0;
    }
    if (needed > UINT32_MAX)
    {
        return mloc_errorInvalid;
    }
    if (buffer == NULL || capacity < needed)
    {
        *sizeOut = (size_t)needed;
        return mloc_errorCapacity;
    }
    result = mlocWriteBound(formatter, &f, &h, buffer, (uint32_t)needed);
    if (result != mloc_success)
    {
        return result;
    }
    Prepare(buffer, &h, &f);
    *sizeOut = (size_t)needed;
    return mloc_success;
}

const PreparedNumber* mlocPreparedFor(const Resolution* r, uint32_t expression)
{
    uint32_t at = mlocLoad32(r->words + PreparedWord(&r->h.view, expression));
    const uint8_t* place = r->words - sizeof(BoundHeader) + at;
    // A bound message moved to an address that leaves its values
    // unaligned makes its own (a copy would take a unit formatter's
    // 3.5 KB of stack at every write).
    return at == 0 || (uintptr_t)place % 8 != 0 ? NULL : (const PreparedNumber*)place;
}
