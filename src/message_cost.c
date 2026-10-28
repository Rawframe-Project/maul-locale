// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A message's bounds and costs.

#include "message_cost.h"

#include "bytes.h"

static uint32_t Field(const MessageView* v, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(v, kind, index, field);
}

static uint32_t Saturate(uint64_t count)
{
    return count > MLOC_MAX_MESSAGE_RESOLUTIONS ? MLOC_MAX_MESSAGE_RESOLUTIONS + 1
                                                : (uint32_t)count;
}

// What an expression costs: the chain it starts, one more than the
// longest chain of a declaration whose variable it names (as operand or
// option value), and the resolutions it makes, its own and each named
// declaration's again, an option's twice (checked, then read). Each
// saturates past its limit.
typedef struct Cost
{
    uint32_t chain;
    uint32_t resolutions;
} Cost;

// An expression's cost. Declarations name only earlier ones, so theirs
// are known; one naming a later expression (only in crafted bytes, which
// could loop) gives a chain of 0.
static Cost CostOf(const MessageView* v, const uint8_t* words, uint32_t e)
{
    const uint8_t* expressionWords = words + (size_t)v->count[messageVariables] * 4;
    const uint8_t* costWords = expressionWords + (size_t)v->count[messageExpressions] * 4;
    Cost cost = {0, 1};
    uint64_t resolutions = 1;
    uint32_t first = Field(v, messageExpressions, e, 5);
    uint32_t count = Field(v, messageExpressions, e, 6);
    for (uint32_t i = 0; i <= count; i++)
    {
        uint32_t variable;
        if (i == count)
        {
            if (Field(v, messageExpressions, e, 0) != operandVariable)
            {
                continue;
            }
            variable = Field(v, messageExpressions, e, 1);
        }
        else if (Field(v, messageParameters, first + i, 0) == parameterOptionVariable)
        {
            variable = Field(v, messageParameters, first + i, 3);
        }
        else
        {
            continue;
        }
        uint32_t d = mlocLoad32(words + (size_t)variable * 4);
        if (d == messageUnbound)
        {
            continue;
        }
        uint32_t named = Field(v, messageDeclarations, d, 2);
        if (named == e && Field(v, messageDeclarations, d, 0) == declarationInput && i == count)
        {
            // An input's own operand is the argument.
            continue;
        }
        if (named >= e)
        {
            cost.chain = 0;
            return cost;
        }
        uint32_t length = mlocLoad32(expressionWords + (size_t)named * 4) >> 24;
        cost.chain = length > cost.chain ? length : cost.chain;
        resolutions += (uint64_t)(i == count ? 1 : 2) * mlocLoad32(costWords + (size_t)named * 4);
        resolutions = Saturate(resolutions);
    }
    cost.chain++;
    cost.resolutions = (uint32_t)resolutions;
    return cost;
}

// The resolutions a variable's reference makes.
static uint64_t VariableCost(const MessageView* v, const uint8_t* words, uint32_t variable)
{
    uint32_t d = mlocLoad32(words + (size_t)variable * 4);
    if (d == messageUnbound)
    {
        return 0;
    }
    const uint8_t* costWords =
        words + ((size_t)v->count[messageVariables] + v->count[messageExpressions]) * 4;
    return mlocLoad32(costWords + (size_t)Field(v, messageDeclarations, d, 2) * 4);
}

// The resolutions a pattern's expressions and markups make.
static uint64_t PatternCost(const MessageView* v, const uint8_t* words, uint32_t first,
                            uint32_t count)
{
    const uint8_t* costWords =
        words + ((size_t)v->count[messageVariables] + v->count[messageExpressions]) * 4;
    uint64_t total = 0;
    for (uint32_t i = first; i < first + count; i++)
    {
        uint32_t kind = Field(v, messagePieces, i, 0);
        uint32_t index = Field(v, messagePieces, i, 1);
        if (kind == pieceExpression)
        {
            total += mlocLoad32(costWords + (size_t)index * 4);
        }
        else if (kind == pieceMarkup)
        {
            // A markup's options are resolved for their faults.
            uint32_t parameter = Field(v, messageMarkups, index, 3);
            uint32_t end = parameter + Field(v, messageMarkups, index, 4);
            for (; parameter < end; parameter++)
            {
                if (Field(v, messageParameters, parameter, 0) == parameterOptionVariable)
                {
                    total += VariableCost(v, words, Field(v, messageParameters, parameter, 3));
                }
            }
        }
    }
    return total;
}

// The most resolutions a format call makes: its selectors', then the
// costliest pattern's.
static uint64_t MessageCost(const MessageView* v, const uint8_t* words)
{
    uint64_t total = 0;
    for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
    {
        total += VariableCost(v, words, Field(v, messageSelectors, s, 0));
    }
    if (v->count[messageVariants] == 0)
    {
        return total + PatternCost(v, words, 0, v->count[messagePieces]);
    }
    uint64_t costliest = 0;
    for (uint32_t i = 0; i < v->count[messageVariants]; i++)
    {
        uint64_t cost =
            PatternCost(v, words, Field(v, messageVariants, i, 0), Field(v, messageVariants, i, 1));
        costliest = cost > costliest ? cost : costliest;
    }
    return total + costliest;
}

bool mlocMessageCountsFit(const uint32_t* count)
{
    return count[messageSelectors] <= MLOC_MAX_MESSAGE_SELECTORS &&
           count[messageDeclarations] <= MLOC_MAX_MESSAGE_DECLARATIONS &&
           count[messageExpressions] <= MLOC_MAX_MESSAGE_EXPRESSIONS &&
           count[messageMarkups] <= MLOC_MAX_MESSAGE_EXPRESSIONS;
}

uint64_t mlocMessageCostWords(const MessageView* view)
{
    return (uint64_t)view->count[messageVariables] + 2 * (uint64_t)view->count[messageExpressions];
}

mlocResult mlocWriteMessageCosts(const MessageView* v, uint8_t* words)
{
    for (uint32_t i = 0; i < v->count[messageVariables]; i++)
    {
        mlocStore32(words + (size_t)i * 4, messageUnbound);
    }
    for (uint32_t d = 0; d < v->count[messageDeclarations]; d++)
    {
        mlocStore32(words + (size_t)Field(v, messageDeclarations, d, 1) * 4, d);
    }
    uint8_t* expressionWords = words + (size_t)v->count[messageVariables] * 4;
    uint8_t* costWords = expressionWords + (size_t)v->count[messageExpressions] * 4;
    for (uint32_t e = 0; e < v->count[messageExpressions]; e++)
    {
        Cost cost = CostOf(v, words, e);
        if (cost.chain == 0)
        {
            return mloc_errorFormat;
        }
        if (cost.chain > MLOC_MAX_MESSAGE_CHAIN)
        {
            return mloc_errorLimit;
        }
        mlocStore32(expressionWords + (size_t)e * 4, cost.chain << 24);
        mlocStore32(costWords + (size_t)e * 4, cost.resolutions);
    }
    return MessageCost(v, words) > MLOC_MAX_MESSAGE_RESOLUTIONS ? mloc_errorLimit : mloc_success;
}
