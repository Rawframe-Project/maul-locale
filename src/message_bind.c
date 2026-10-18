// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Message formatters and binding (maul-locale/message.h, record
// mloc-0045).

#include "message_bind.h"

#include "bytes.h"
#include "message_text.h"

#include "maul-locale/locale.h"
#include "maul-unicode/encoding.h"

#include <assert.h>
#include <string.h>

enum
{
    formatterStamp = 0x4D,
};

// "MLBD", read as a little-endian word.
static const uint32_t boundMagic = 0x44424C4D;

static_assert(sizeof(MessageFormatter) <= sizeof(mlocMessageFormatter),
              "the message formatter fits its public value");

static void Store32(uint8_t* p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

// Whether a function's name is an MF2 identifier with a namespace other
// than the library's.
static bool GoodFunctionName(const mlocMessageFunction* function)
{
    const uint8_t* name = (const uint8_t*)function->name;
    size_t length = function->nameLength;
    if (name == NULL || length == 0 ||
        muniValidateUtf8(function->name, length).status != muni_success)
    {
        return false;
    }
    const uint8_t* colon = memchr(name, ':', length);
    if (colon == NULL)
    {
        return false;
    }
    size_t space = (size_t)(colon - name);
    if ((space == 1 && name[0] == 'u') || (space == 4 && memcmp(name, "mloc", 4) == 0))
    {
        return false;
    }
    // Each side one name: a name-start, then name characters.
    bool start = true;
    for (size_t at = 0; at < length;)
    {
        size_t size;
        uint32_t c = mlocMessageDecode(name, at, length, &size);
        if (at == space)
        {
            if (start)
            {
                return false;
            }
            start = true;
        }
        else if (start ? !mlocMessageIsNameStart(c) : !mlocMessageIsNameChar(c))
        {
            return false;
        }
        else
        {
            start = false;
        }
        at += size;
    }
    return !start;
}

// Whether a zone is one the library made: it has an offset.
static bool GoodZone(const mlocTimeZone* zone)
{
    mlocInstant epoch = {0, 0};
    int32_t offset = 0;
    return mlocGetTimeZoneOffset(zone, &epoch, &offset) == mloc_success;
}

// Whether the program's functions are well made: each with a resolve
// entry and a good name, no name twice.
static bool GoodFunctions(const mlocMessageFunction* functions, size_t count)
{
    if ((functions == NULL && count > 0) || count >= messageFunctionLimit)
    {
        return false;
    }
    for (size_t i = 0; i < count; i++)
    {
        if (functions[i].resolve == NULL || !GoodFunctionName(&functions[i]))
        {
            return false;
        }
        for (size_t j = 0; j < i; j++)
        {
            if (functions[j].nameLength == functions[i].nameLength &&
                memcmp(functions[j].name, functions[i].name, functions[i].nameLength) == 0)
            {
                return false;
            }
        }
    }
    return true;
}

mlocResult mlocInitMessageFormatter(mlocMessageFormatter* formatterOut, const mlocDataSet* data,
                                    const mlocLocale* locale, const mlocMessageOptions* options)
{
    if (formatterOut == NULL || data == NULL || locale == NULL)
    {
        return mloc_errorInvalid;
    }
    if (options != NULL && ((uint32_t)options->isolation > mloc_messageIsolationNone ||
                            !GoodFunctions(options->functions, options->functionCount) ||
                            (options->timeZone != NULL && !GoodZone(options->timeZone))))
    {
        return mloc_errorInvalid;
    }
    MessageFormatter f;
    memset(&f, 0, sizeof f);
    f.stamp = formatterStamp;
    f.strict = options != NULL && options->strict;
    f.isolate = options == NULL || options->isolation == mloc_messageIsolationDefault;
    // A locale without a direction (no script, no likely one) leaves the
    // message's unknown.
    f.direction = mloc_directionUnknown;
    if (mlocGetLocaleDirection(data, locale, &f.direction) != mloc_success)
    {
        f.direction = mloc_directionUnknown;
    }
    if (options != NULL)
    {
        f.functions = options->functions;
        f.functionCount = options->functionCount;
        f.hasZone = options->timeZone != NULL;
        if (f.hasZone)
        {
            f.zone = *options->timeZone;
        }
    }
    f.data = data;
    f.locale = *locale;
    memset(formatterOut, 0, sizeof *formatterOut);
    memcpy(formatterOut, &f, sizeof f);
    return mloc_success;
}

bool mlocReadMessageFormatter(const mlocMessageFormatter* formatter, MessageFormatter* out)
{
    if (formatter == NULL)
    {
        return false;
    }
    memcpy(out, formatter, sizeof *out);
    return out->stamp == formatterStamp;
}

bool mlocReadBound(const void* bound, BoundHeader* headerOut, MessageFormatter* formatterOut)
{
    if (bound == NULL)
    {
        return false;
    }
    memcpy(headerOut, bound, sizeof *headerOut);
    return headerOut->magic == boundMagic &&
           mlocReadMessageFormatter(headerOut->formatter, formatterOut);
}

const uint8_t* mlocBoundWords(const void* bound)
{
    return (const uint8_t*)bound + sizeof(BoundHeader);
}

static uint32_t Field(const MessageView* v, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(v, kind, index, field);
}

static uint32_t Saturate(uint64_t count)
{
    return count > messageResolutionLimit ? messageResolutionLimit + 1 : (uint32_t)count;
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

// An expression's function slot: none, a default function, a program
// function by its name, or unknown.
static uint32_t SlotOf(const MessageView* v, const MessageFormatter* f, uint32_t e)
{
    uint32_t length = Field(v, messageExpressions, e, 4);
    if (length == 0)
    {
        return messageFunctionNone;
    }
    const uint8_t* name = v->pool + Field(v, messageExpressions, e, 3);
    static const struct
    {
        const char* name;
        uint32_t slot;
    } defaults[] = {
        {"string", messageFunctionString},
        {"number", messageFunctionNumber},
        {"integer", messageFunctionInteger},
        {"offset", messageFunctionOffset},
        {"percent", messageFunctionPercent},
        {"currency", messageFunctionCurrency},
        {"unit", messageFunctionUnit},
        {"date", messageFunctionDate},
        {"time", messageFunctionTime},
        {"datetime", messageFunctionDateTime},
        {"mloc:spellout", messageFunctionSpellout},
        {"mloc:relative", messageFunctionRelative},
        {"mloc:duration", messageFunctionDuration},
        {"mloc:list", messageFunctionList},
    };
    for (size_t i = 0; i < sizeof defaults / sizeof defaults[0]; i++)
    {
        if (length == strlen(defaults[i].name) && memcmp(name, defaults[i].name, length) == 0)
        {
            return defaults[i].slot;
        }
    }
    for (size_t i = 0; i < f->functionCount; i++)
    {
        if (f->functions[i].nameLength == length && memcmp(f->functions[i].name, name, length) == 0)
        {
            return messageFunctionProgram + (uint32_t)i;
        }
    }
    return messageFunctionUnknown;
}

// Writes the words: each variable's declaration, then each expression's
// function slot and chain length, then its resolutions;
// mloc_errorInvalid past the chain or resolution limit (not a capacity
// error: no buffer would do), mloc_errorData for a declaration naming a
// later one.
static mlocResult WriteWords(const MessageView* v, const MessageFormatter* f, uint8_t* words)
{
    for (uint32_t i = 0; i < v->count[messageVariables]; i++)
    {
        Store32(words + (size_t)i * 4, messageUnbound);
    }
    for (uint32_t d = 0; d < v->count[messageDeclarations]; d++)
    {
        Store32(words + (size_t)Field(v, messageDeclarations, d, 1) * 4, d);
    }
    uint8_t* expressionWords = words + (size_t)v->count[messageVariables] * 4;
    uint8_t* costWords = expressionWords + (size_t)v->count[messageExpressions] * 4;
    for (uint32_t e = 0; e < v->count[messageExpressions]; e++)
    {
        Cost cost = CostOf(v, words, e);
        if (cost.chain == 0)
        {
            return mloc_errorData;
        }
        if (cost.chain > messageChainLimit)
        {
            return mloc_errorInvalid;
        }
        Store32(expressionWords + (size_t)e * 4, SlotOf(v, f, e) | cost.chain << 24);
        Store32(costWords + (size_t)e * 4, cost.resolutions);
    }
    return MessageCost(v, words) > messageResolutionLimit ? mloc_errorInvalid : mloc_success;
}

uint32_t mlocExpressionSlot(const MessageView* view, const MessageFormatter* formatter,
                            uint32_t expression)
{
    return SlotOf(view, formatter, expression);
}

mlocResult mlocOpenBind(const mlocMessageFormatter* formatter, const void* message, size_t size,
                        MessageFormatter* formatterOut, BoundHeader* headerOut)
{
    if (!mlocReadMessageFormatter(formatter, formatterOut) || message == NULL)
    {
        return mloc_errorInvalid;
    }
    memset(headerOut, 0, sizeof *headerOut);
    mlocResult result = mlocReadMessage(message, size, &headerOut->view);
    if (result != mloc_success)
    {
        return result;
    }
    const MessageView* v = &headerOut->view;
    return v->count[messageSelectors] > messageSelectorLimit ||
                   v->count[messageDeclarations] > messageDeclarationLimit ||
                   v->count[messageExpressions] > messageIndexLimit ||
                   v->count[messageMarkups] > messageIndexLimit
               ? mloc_errorInvalid
               : mloc_success;
}

uint64_t mlocBoundBase(const MessageView* view)
{
    return sizeof(BoundHeader) + 4 * ((uint64_t)view->count[messageVariables] +
                                      3 * (uint64_t)view->count[messageExpressions]);
}

mlocResult mlocWriteBound(const mlocMessageFormatter* formatter, const MessageFormatter* f,
                          BoundHeader* h, void* buffer, uint32_t size)
{
    // The old header goes first: a bind that fails below must not leave
    // an earlier bound message's header over words that are no longer
    // its own.
    memset(buffer, 0, sizeof *h);
    uint8_t* words = (uint8_t*)buffer + sizeof *h;
    mlocResult result = WriteWords(&h->view, f, words);
    if (result != mloc_success)
    {
        return result;
    }
    size_t expressions = h->view.count[messageExpressions];
    memset(words + 4 * ((size_t)h->view.count[messageVariables] + 2 * expressions), 0,
           4 * expressions);
    h->magic = boundMagic;
    h->size = size;
    h->formatter = formatter;
    memcpy(buffer, h, sizeof *h);
    return mloc_success;
}
