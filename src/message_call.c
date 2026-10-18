// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The accessors a program function reads its call's options and locale
// with (maul-locale/message.h, record mloc-0045); the u: options hidden.

#include "message_operand.h"
#include "message_resolve.h"

#include <string.h>

// The call a function's entry received, or false.
static bool ReadCall(const mlocMessageCall* call, CallState* out)
{
    if (call == NULL)
    {
        return false;
    }
    memcpy(out, call, sizeof *out);
    return out->stamp == callStamp && out->r != NULL;
}

// The parameter of a call's index-th option, or UINT32_MAX.
static uint32_t OptionAt(const CallState* s, size_t index)
{
    const Resolution* r = s->r;
    uint32_t first = mlocResolutionField(r, messageExpressions, s->expression, 5);
    uint32_t count = mlocResolutionField(r, messageExpressions, s->expression, 6);
    size_t seen = 0;
    for (uint32_t p = first; p < first + count; p++)
    {
        uint32_t kind = mlocResolutionField(r, messageParameters, p, 0);
        if ((kind != parameterOptionLiteral && kind != parameterOptionVariable) ||
            mlocUnicodeOption(r, p))
        {
            continue;
        }
        if (seen++ == index)
        {
            return p;
        }
    }
    return UINT32_MAX;
}

size_t mlocGetMessageOptionCount(const mlocMessageCall* call)
{
    CallState s;
    if (!ReadCall(call, &s))
    {
        return 0;
    }
    size_t count = 0;
    while (OptionAt(&s, count) != UINT32_MAX)
    {
        count++;
    }
    return count;
}

// An option's value, resolved now without reporting faults again.
static void ReadOption(const CallState* s, uint32_t p, mlocMessageOperand* valueOut,
                       mlocMessageResolved* storage)
{
    Value v =
        mlocOptionValue(s->r, p, mlocResolutionField(s->r, messageExpressions, s->expression, 7));
    mlocToOperand(&v, valueOut, storage);
}

mlocResult mlocGetMessageOption(const mlocMessageCall* call, size_t index, const char** nameOut,
                                size_t* lengthOut, mlocMessageOperand* valueOut,
                                mlocMessageResolved* storage)
{
    CallState s;
    if (!ReadCall(call, &s) || nameOut == NULL || lengthOut == NULL || valueOut == NULL ||
        storage == NULL)
    {
        return mloc_errorInvalid;
    }
    uint32_t p = OptionAt(&s, index);
    if (p == UINT32_MAX)
    {
        return mloc_errorInvalid;
    }
    *nameOut =
        (const char*)mlocResolutionPool(s.r, mlocResolutionField(s.r, messageParameters, p, 1));
    *lengthOut = mlocResolutionField(s.r, messageParameters, p, 2);
    ReadOption(&s, p, valueOut, storage);
    return mloc_success;
}

mlocResult mlocFindMessageOption(const mlocMessageCall* call, const char* name, size_t length,
                                 mlocMessageOperand* valueOut, mlocMessageResolved* storage)
{
    CallState s;
    if (!ReadCall(call, &s) || name == NULL || valueOut == NULL || storage == NULL)
    {
        return mloc_errorInvalid;
    }
    for (size_t i = 0;; i++)
    {
        uint32_t p = OptionAt(&s, i);
        if (p == UINT32_MAX)
        {
            return mloc_errorNotFound;
        }
        if (mlocResolutionField(s.r, messageParameters, p, 2) == length &&
            memcmp(mlocResolutionPool(s.r, mlocResolutionField(s.r, messageParameters, p, 1)), name,
                   length) == 0)
        {
            ReadOption(&s, p, valueOut, storage);
            return mloc_success;
        }
    }
}

mlocResult mlocGetMessageLocale(const mlocMessageCall* call, mlocLocale* localeOut)
{
    CallState s;
    if (!ReadCall(call, &s) || localeOut == NULL)
    {
        return mloc_errorInvalid;
    }
    *localeOut = s.r->formatter.locale;
    return mloc_success;
}
