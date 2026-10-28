// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Binding a message (maul-locale/message.h): the words, then the number
// functions' values prepared.

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

// Whether a slot is a number function's or a date function's, and so
// prepared.
static bool Prepared(uint32_t slot)
{
    return slot >= messageFunctionNumber && slot <= messageFunctionParticle;
}

static bool DateSlot(uint32_t slot)
{
    return slot >= messageFunctionDate && slot <= messageFunctionDateTime;
}

// The bytes kept for an expression's prepared value: its date function's
// formatter's for a date function, a unit formatter's
// room for :unit, a currency formatter's for :currency and for :offset
// (whose value may be a currency's; an offset of a unit value is made in
// the call), a number formatter's for the rest.
static size_t RoomOf(uint32_t slot)
{
    if (DateSlot(slot))
    {
        return mlocPreparedDateSize((DateFunction)(slot - messageFunctionDate));
    }
    if (slot == messageFunctionDuration)
    {
        return sizeof(PreparedDuration);
    }
    if (slot == messageFunctionList)
    {
        return sizeof(PreparedList);
    }
    if (slot == messageFunctionCase || slot == messageFunctionParticle)
    {
        return sizeof(PreparedGrammar);
    }
    return mlocPreparedSize(slot == messageFunctionUnit       ? preparedUnit
                            : slot == messageFunctionSpellout ? preparedSpellout
                            : slot == messageFunctionRelative ? preparedRelative
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

// What an argument stands for in an expression of a slot: an instant
// for the date functions, which every one takes with every option, a
// duration of nothing, a list of no items, else an integer.
static const mlocMessageValue* ProbeFor(uint32_t slot)
{
    static const mlocMessageValue number = {mloc_messageValueInteger, 0, {.integer = 0}};
    static const mlocMessageValue dateTime = {
        mloc_messageValueInstant, 0, {.instant = {946684800, 0}}};
    static const mlocDurationValues none = {0};
    static const mlocMessageValue duration = {mloc_messageValueDuration, 0, {.duration = &none}};
    static const mlocMessageValue list = {mloc_messageValueList, 0, {.list = {NULL, 0}}};
    static const mlocMessageValue text = {mloc_messageValueString, 0, {.string = {"a", 1}}};
    return DateSlot(slot)                                                   ? &dateTime
           : slot == messageFunctionDuration                                ? &duration
           : slot == messageFunctionList                                    ? &list
           : slot == messageFunctionCase || slot == messageFunctionParticle ? &text
                                                                            : &number;
}

// A value prepared at bind, of any function.
typedef union PreparedValue
{
    PreparedNumber number;
    PreparedDate date;
    PreparedDuration duration;
    PreparedList list;
    PreparedGrammar grammar;
} PreparedValue;

// Prepares the value an expression made; its size, or 0 when it keeps
// none (another expression's value, or a formatter that cannot be made).
static size_t PrepareValue(const Value* v, uint32_t expression, const MessageFormatter* f,
                           PreparedValue* out)
{
    if (v->kind == valueNumber && v->number.origin == expression &&
        (mlocWordsValue(&v->number)
             ? mlocPrepareWords(&v->number, f->data, &f->locale, &out->number)
             : mlocPrepareNumber(&v->number, f->data, &f->locale, &out->number)))
    {
        return mlocPreparedSize((PreparedKind)out->number.kind);
    }
    if (v->kind == valueDate && v->date.origin == expression &&
        mlocPrepareDate(&v->date, f->data, &f->locale, &out->date))
    {
        return mlocPreparedDateSize((DateFunction)v->date.function);
    }
    if (v->kind == valueDuration && v->duration.origin == expression &&
        mlocPrepareDuration(&v->duration, f->data, &f->locale, &out->duration))
    {
        return sizeof(PreparedDuration);
    }
    if (v->kind == valueList && v->list.origin == expression &&
        mlocPrepareList(&v->list, f->data, &f->locale, &out->list))
    {
        return sizeof(PreparedList);
    }
    if (v->kind == valueGrammar && v->grammar.origin == expression &&
        mlocPrepareGrammar(f->data, &f->locale, &out->grammar))
    {
        return sizeof(PreparedGrammar);
    }
    return 0;
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
    r.budget = MLOC_MAX_MESSAGE_RESOLUTIONS;
    r.probe = true;
    uint8_t* bytes = buffer;
    uint64_t base = mlocBoundBase(&h->view);
    size_t at = (size_t)base + (size_t)((8 - ((uintptr_t)bytes + base) % 8) % 8);
    uint8_t* words = bytes + sizeof *h;
    PreparedValue prepared;
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
        r.probeArgument = ProbeFor(slot);
        Value v = mlocResolveExpression(&r, e);
        if (r.exhausted)
        {
            return;
        }
        size_t size = r.dynamic ? 0 : PrepareValue(&v, e, f, &prepared);
        if (size > 0 && size <= room)
        {
            memcpy(bytes + at, &prepared, size);
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
    if (needed > MLOC_MAX_BOUND_MESSAGE_SIZE)
    {
        return mloc_errorLimit;
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

const void* mlocPreparedFor(const Resolution* r, uint32_t expression)
{
    uint32_t at = mlocLoad32(r->words + PreparedWord(&r->h.view, expression));
    const uint8_t* place = r->words - sizeof(BoundHeader) + at;
    // A bound message moved to an address that leaves its values
    // unaligned makes its own (a copy would take a unit formatter's
    // 3.5 KB of stack at every write).
    return at == 0 || (uintptr_t)place % 8 != 0 ? NULL : place;
}
