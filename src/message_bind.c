// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Message formatters and binding (maul-locale/message.h).

#include "message_bind.h"

#include "bytes.h"
#include "invariant.h"
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

mlocResult mlocInitMessageFormatter(const mlocDataSet* data, const mlocLocale* locale,
                                    const mlocMessageOptions* options,
                                    mlocMessageFormatter* formatterOut)
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
        {"mloc:case", messageFunctionCase},
        {"mloc:particle", messageFunctionParticle},
        {"mloc:gender", messageFunctionGender},
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

// Writes the words: the costs, then each expression's function slot
// below its chain length. A message past a bound is refused with
// mloc_errorFormat, as every check on stored bytes is: the compiler
// refuses such a message, so no compiled one has it.
static mlocResult WriteWords(const MessageView* v, const MessageFormatter* f, uint8_t* words)
{
    if (mlocWriteMessageCosts(v, words) != mloc_success)
    {
        return mloc_errorFormat;
    }
    uint8_t* expressionWords = words + (size_t)v->count[messageVariables] * 4;
    for (uint32_t e = 0; e < v->count[messageExpressions]; e++)
    {
        // The formatter's functions are fewer than messageFunctionLimit,
        // so a slot leaves the chain's byte alone.
        uint8_t* word = expressionWords + (size_t)e * 4;
        uint32_t slot = SlotOf(v, f, e);
        MLOC_ASSERT(slot < 1u << 24 && (mlocLoad32(word) & 0xFFFFFFu) == 0);
        Store32(word, mlocLoad32(word) | slot);
    }
    return mloc_success;
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
    return mlocMessageCountsFit(headerOut->view.count) ? mloc_success : mloc_errorFormat;
}

uint64_t mlocBoundBase(const MessageView* view)
{
    return sizeof(BoundHeader) + 4 * ((uint64_t)view->count[messageVariables] +
                                      3 * (uint64_t)view->count[messageExpressions]);
}

mlocResult mlocWriteBound(const mlocMessageFormatter* formatter, const MessageFormatter* f,
                          BoundHeader* h, void* buffer, uint32_t size)
{
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
