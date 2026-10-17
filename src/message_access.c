// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// What a formatted message's parts name (maul-locale/message.h, record
// mloc-0045): an expression's function or a markup's name, and their
// options and attributes, read from the compiled message by the index a
// part's kind holds.

#include "message_bind.h"
#include "message_form.h"
#include "message_read.h"

#include <string.h>

// A part's record: its kind among the records (expressions or markups)
// and its index; false for a part that names none, or an index past
// them.
static bool Record(const void* bound, uint32_t part, BoundHeader* h, uint32_t* kindOut,
                   uint32_t* indexOut)
{
    MessageFormatter formatter;
    if (bound == NULL || !mlocReadBound(bound, h, &formatter))
    {
        return false;
    }
    uint32_t piece = part & 0xFF;
    *indexOut = part >> 8;
    if (piece == mloc_messagePartPlaceholder || piece == mloc_messagePartFallback)
    {
        *kindOut = messageExpressions;
    }
    else if (piece >= mloc_messagePartMarkupOpen && piece <= mloc_messagePartMarkupClose)
    {
        *kindOut = messageMarkups;
    }
    else
    {
        return false;
    }
    return *indexOut < h->view.count[*kindOut];
}

// A record's first parameter and their count: an expression's are its
// fields 5 and 6, a markup's 3 and 4.
static void Parameters(const BoundHeader* h, uint32_t kind, uint32_t index, uint32_t* firstOut,
                       uint32_t* countOut)
{
    uint32_t field = kind == messageExpressions ? 5 : 3;
    *firstOut = mlocMessageField(&h->view, kind, index, field);
    *countOut = mlocMessageField(&h->view, kind, index, field + 1);
}

mlocResult mlocGetMessagePartInfo(const void* bound, uint32_t part, mlocMessagePartInfo* infoOut)
{
    BoundHeader h;
    uint32_t kind = 0;
    uint32_t index = 0;
    if (infoOut == NULL || !Record(bound, part, &h, &kind, &index))
    {
        return mloc_errorInvalid;
    }
    // An expression's function is its fields 3 and 4, a markup's name
    // its fields 1 and 2.
    uint32_t field = kind == messageExpressions ? 3 : 1;
    infoOut->nameLength = mlocMessageField(&h.view, kind, index, field + 1);
    infoOut->name = infoOut->nameLength == 0
                        ? NULL
                        : (const char*)h.view.pool + mlocMessageField(&h.view, kind, index, field);
    uint32_t first = 0;
    Parameters(&h, kind, index, &first, &infoOut->parameterCount);
    return mloc_success;
}

// Reads a parameter record.
static void ReadParameter(const BoundHeader* h, uint32_t p, mlocMessageParameter* out)
{
    const MessageView* v = &h->view;
    uint32_t kind = mlocMessageField(v, messageParameters, p, 0);
    out->attribute = kind == parameterAttribute || kind == parameterAttributeLiteral;
    out->variable = kind == parameterOptionVariable;
    out->name = (const char*)v->pool + mlocMessageField(v, messageParameters, p, 1);
    out->nameLength = mlocMessageField(v, messageParameters, p, 2);
    out->value = NULL;
    out->valueLength = 0;
    if (out->variable)
    {
        uint32_t variable = mlocMessageField(v, messageParameters, p, 3);
        out->value = (const char*)v->pool + mlocMessageField(v, messageVariables, variable, 0);
        out->valueLength = mlocMessageField(v, messageVariables, variable, 1);
    }
    else if (kind != parameterAttribute)
    {
        out->value = (const char*)v->pool + mlocMessageField(v, messageParameters, p, 3);
        out->valueLength = mlocMessageField(v, messageParameters, p, 4);
    }
}

mlocResult mlocGetMessagePartParameter(const void* bound, uint32_t part, uint32_t index,
                                       mlocMessageParameter* parameterOut)
{
    BoundHeader h;
    uint32_t kind = 0;
    uint32_t record = 0;
    if (parameterOut == NULL || !Record(bound, part, &h, &kind, &record))
    {
        return mloc_errorInvalid;
    }
    uint32_t first = 0;
    uint32_t count = 0;
    Parameters(&h, kind, record, &first, &count);
    if (index >= count)
    {
        return mloc_errorInvalid;
    }
    ReadParameter(&h, first + index, parameterOut);
    return mloc_success;
}

mlocResult mlocFindMessagePartOption(const void* bound, uint32_t part, const char* name,
                                     size_t length, mlocMessageParameter* parameterOut)
{
    BoundHeader h;
    uint32_t kind = 0;
    uint32_t record = 0;
    if (parameterOut == NULL || (name == NULL && length > 0) ||
        !Record(bound, part, &h, &kind, &record))
    {
        return mloc_errorInvalid;
    }
    uint32_t first = 0;
    uint32_t count = 0;
    Parameters(&h, kind, record, &first, &count);
    for (uint32_t p = first; p < first + count; p++)
    {
        mlocMessageParameter parameter;
        ReadParameter(&h, p, &parameter);
        if (!parameter.attribute && parameter.nameLength == length &&
            (length == 0 || memcmp(parameter.name, name, length) == 0))
        {
            *parameterOut = parameter;
            return mloc_success;
        }
    }
    return mloc_errorNotFound;
}
