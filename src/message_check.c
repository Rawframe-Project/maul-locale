// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_check.h"

#include "bytes.h"
#include "invariant.h"
#include "message_form.h"
#include "message_read.h"

#include <stdbool.h>
#include <string.h>

typedef struct Checker
{
    uint8_t* message;
    uint32_t start[messageCount];
    uint32_t keyStart;
    uint32_t count[messageCount];
    const uint8_t* pool;
    MessageTable* table;
    // Per variable: the first declaration naming it, and the one binding
    // it; UINT32_MAX for none.
    uint8_t* firstUse;
    uint8_t* boundBy;
} Checker;

static const uint32_t none = UINT32_MAX;

static void Store32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint8_t* Record(const Checker* c, uint32_t kind, uint32_t index)
{
    return c->message + c->start[kind] + index * messageRecordSizes[kind];
}

static uint32_t Get(const Checker* c, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocLoad32(Record(c, kind, index) + field * 4);
}

static MessageCheck Error(mlocMessageError error, uint32_t at)
{
    MessageCheck check = {error, at};
    return check;
}

static bool SamePool(const Checker* c, uint32_t offsetA, uint32_t lengthA, uint32_t offsetB,
                     uint32_t lengthB)
{
    return lengthA == lengthB && memcmp(c->pool + offsetA, c->pool + offsetB, lengthA) == 0;
}

static bool SameParameterName(const void* context, uint32_t entry, uint32_t candidate)
{
    const Checker* c = context;
    return SamePool(c, Get(c, messageParameters, entry, 1), Get(c, messageParameters, entry, 2),
                    Get(c, messageParameters, candidate, 1),
                    Get(c, messageParameters, candidate, 2));
}

// Whether parameters [first, first + count) name an option twice.
static bool RepeatsOption(const Checker* c, uint32_t first, uint32_t count)
{
    mlocTableClear(c->table);
    for (uint32_t i = first; i < first + count; i++)
    {
        uint32_t kind = Get(c, messageParameters, i, 0);
        if (kind != parameterOptionLiteral && kind != parameterOptionVariable)
        {
            continue;
        }
        const uint8_t* name = c->pool + Get(c, messageParameters, i, 1);
        uint32_t hash = mlocTableHash(2166136261u, name, Get(c, messageParameters, i, 2));
        if (mlocTableFindOrAdd(c->table, hash, i, SameParameterName, c) != none)
        {
            return true;
        }
    }
    return false;
}

static MessageCheck CheckOptions(const Checker* c)
{
    for (uint32_t e = 0; e < c->count[messageExpressions]; e++)
    {
        if (RepeatsOption(c, Get(c, messageExpressions, e, 5), Get(c, messageExpressions, e, 6)))
        {
            return Error(mloc_messageErrorDuplicateOptionName, Get(c, messageExpressions, e, 7));
        }
    }
    for (uint32_t m = 0; m < c->count[messageMarkups]; m++)
    {
        if (RepeatsOption(c, Get(c, messageMarkups, m, 3), Get(c, messageMarkups, m, 4)))
        {
            return Error(mloc_messageErrorDuplicateOptionName, Get(c, messageMarkups, m, 5));
        }
    }
    return Error(mloc_messageErrorNone, 0);
}

static uint32_t Word(const uint8_t* words, uint32_t variable)
{
    return mlocLoad32(words + (size_t)variable * 4);
}

static void MarkUse(const Checker* c, uint32_t variable, uint32_t declaration)
{
    if (Word(c->firstUse, variable) == none)
    {
        Store32(c->firstUse + (size_t)variable * 4, declaration);
    }
}

// Whether an expression names a variable: its operand when asked, and
// its options' values.
static bool Names(const Checker* c, uint32_t expression, uint32_t variable, bool operand)
{
    if (operand && Get(c, messageExpressions, expression, 0) == operandVariable &&
        Get(c, messageExpressions, expression, 1) == variable)
    {
        return true;
    }
    uint32_t first = Get(c, messageExpressions, expression, 5);
    uint32_t count = Get(c, messageExpressions, expression, 6);
    for (uint32_t i = first; i < first + count; i++)
    {
        if (Get(c, messageParameters, i, 0) == parameterOptionVariable &&
            Get(c, messageParameters, i, 3) == variable)
        {
            return true;
        }
    }
    return false;
}

// Every variable an expression names, marked as used by a declaration.
static void MarkExpression(const Checker* c, uint32_t expression, uint32_t declaration)
{
    if (Get(c, messageExpressions, expression, 0) == operandVariable)
    {
        MarkUse(c, Get(c, messageExpressions, expression, 1), declaration);
    }
    uint32_t first = Get(c, messageExpressions, expression, 5);
    uint32_t count = Get(c, messageExpressions, expression, 6);
    for (uint32_t i = first; i < first + count; i++)
    {
        if (Get(c, messageParameters, i, 0) == parameterOptionVariable)
        {
            MarkUse(c, Get(c, messageParameters, i, 3), declaration);
        }
    }
}

// A declaration binds no variable an earlier declaration named, and none
// its own expression names (an .input's operand being its own).
static MessageCheck CheckDeclarations(const Checker* c)
{
    memset(c->firstUse, 0xFF, (size_t)c->count[messageVariables] * 4);
    memset(c->boundBy, 0xFF, (size_t)c->count[messageVariables] * 4);
    for (uint32_t d = 0; d < c->count[messageDeclarations]; d++)
    {
        uint32_t kind = Get(c, messageDeclarations, d, 0);
        uint32_t variable = Get(c, messageDeclarations, d, 1);
        uint32_t expression = Get(c, messageDeclarations, d, 2);
        // The compiler has just written them.
        MLOC_ASSERT(variable < c->count[messageVariables] &&
                    expression < c->count[messageExpressions]);
        if (Word(c->firstUse, variable) != none ||
            Names(c, expression, variable, kind == declarationLocal))
        {
            return Error(mloc_messageErrorDuplicateDeclaration,
                         Get(c, messageExpressions, expression, 7));
        }
        Store32(c->boundBy + (size_t)variable * 4, d);
        Store32(Record(c, messageVariables, variable) + 8,
                kind == declarationLocal ? variableLocal : variableInput);
        MarkUse(c, variable, d);
        MarkExpression(c, expression, d);
    }
    return Error(mloc_messageErrorNone, 0);
}

// Whether a variable leads to a declaration with a function, through
// local declarations of a bare variable. Each step goes to an earlier
// declaration (a later one binding it would be a duplicate), so it ends.
static bool Annotated(const Checker* c, uint32_t variable)
{
    uint32_t later = none;
    for (;;)
    {
        uint32_t d = Word(c->boundBy, variable);
        if (d == none)
        {
            return false;
        }
        MLOC_ASSERT(d < later);
        later = d;
        (void)later;
        uint32_t expression = Get(c, messageDeclarations, d, 2);
        if (Get(c, messageExpressions, expression, 4) > 0)
        {
            return true;
        }
        if (Get(c, messageDeclarations, d, 0) != declarationLocal ||
            Get(c, messageExpressions, expression, 0) != operandVariable)
        {
            return false;
        }
        variable = Get(c, messageExpressions, expression, 1);
    }
}

static MessageCheck CheckSelectors(const Checker* c)
{
    for (uint32_t s = 0; s < c->count[messageSelectors]; s++)
    {
        if (!Annotated(c, Get(c, messageSelectors, s, 0)))
        {
            return Error(mloc_messageErrorMissingSelectorAnnotation,
                         Get(c, messageSelectors, s, 1));
        }
    }
    return Error(mloc_messageErrorNone, 0);
}

static const uint8_t* Key(const Checker* c, uint32_t variant, uint32_t key)
{
    uint32_t index = variant * c->count[messageSelectors] + key;
    return c->message + c->keyStart + index * messageKeySize;
}

static bool SameKeys(const void* context, uint32_t entry, uint32_t candidate)
{
    const Checker* c = context;
    for (uint32_t k = 0; k < c->count[messageSelectors]; k++)
    {
        const uint8_t* a = Key(c, entry, k);
        const uint8_t* b = Key(c, candidate, k);
        if (mlocLoad32(a) != mlocLoad32(b) || !SamePool(c, mlocLoad32(a + 4), mlocLoad32(a + 8),
                                                        mlocLoad32(b + 4), mlocLoad32(b + 8)))
        {
            return false;
        }
    }
    return true;
}

static MessageCheck CheckVariants(const Checker* c)
{
    mlocTableClear(c->table);
    for (uint32_t v = 0; v < c->count[messageVariants]; v++)
    {
        uint32_t hash = 2166136261u;
        for (uint32_t k = 0; k < c->count[messageSelectors]; k++)
        {
            const uint8_t* key = Key(c, v, k);
            uint8_t kind = (uint8_t)mlocLoad32(key);
            hash = mlocTableHash(hash, &kind, 1);
            hash = mlocTableHash(hash, c->pool + mlocLoad32(key + 4), mlocLoad32(key + 8));
        }
        if (mlocTableFindOrAdd(c->table, hash, v, SameKeys, c) != none)
        {
            return Error(mloc_messageErrorDuplicateVariant, Get(c, messageVariants, v, 2));
        }
    }
    return Error(mloc_messageErrorNone, 0);
}

MessageCheck mlocCheckMessage(uint8_t* message, uint32_t size, MessageTable* table, uint8_t* words)
{
    // The compiler has just written the fields.
    MessageView view;
    mlocResult layout = mlocReadMessageLayout(message, size, &view);
    MLOC_ASSERT(layout == mloc_success);
    (void)layout;
    Checker c;
    c.message = message;
    memcpy(c.start, view.start, sizeof c.start);
    memcpy(c.count, view.count, sizeof c.count);
    c.keyStart = view.keyStart;
    c.pool = view.pool;
    c.table = table;
    c.firstUse = words;
    c.boundBy = words + (size_t)c.count[messageVariables] * 4;
    MessageCheck check = CheckDeclarations(&c);
    if (check.error == mloc_messageErrorNone)
    {
        check = CheckOptions(&c);
    }
    if (check.error == mloc_messageErrorNone)
    {
        check = CheckSelectors(&c);
    }
    if (check.error == mloc_messageErrorNone)
    {
        check = CheckVariants(&c);
    }
    return check;
}
