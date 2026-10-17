// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formatting a bound message (maul-locale/message.h, record mloc-0045):
// the pattern selected, then each piece written, each placeholder
// resolved where it stands (a variable through its declaration, on the
// stack, at most the bound message's chain limit deep).

#include "message_format.h"

#include "message_bind.h"

#include "maul-unicode/normalize.h"

#include <string.h>

// A resolved value: text (a literal's, or a string argument's), an
// argument of another kind, or a fallback with the string it shows.
typedef enum ValueKind
{
    valueText,
    valueArgument,
    valueFallbackVariable,
    valueFallbackLiteral,
    valueFallbackFunction,
} ValueKind;

typedef struct Value
{
    ValueKind kind;
    // Text, a fallback's literal, or a fallback function's identifier.
    const uint8_t* text;
    size_t length;
    // An argument; a fallback variable's index.
    const mlocMessageValue* argument;
    uint32_t variable;
} Value;

// The bytes that fit go to the buffer; length counts them all.
typedef struct Writer
{
    char* buffer;
    size_t capacity;
    size_t length;
} Writer;

typedef struct Formatting
{
    BoundHeader h;
    MessageFormatter formatter;
    const uint8_t* words;
    const mlocMessageArgument* arguments;
    size_t argumentCount;
    Writer w;
    mlocMessageFaults* faults;
    size_t faultCount;
} Formatting;

static uint32_t Field(const Formatting* f, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(&f->h.view, kind, index, field);
}

static uint32_t Word(const uint8_t* words, uint32_t index)
{
    const uint8_t* p = words + (size_t)index * 4;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t BoundBy(const Formatting* f, uint32_t variable)
{
    return Word(f->words, variable);
}

static uint32_t SlotOf(const Formatting* f, uint32_t expression)
{
    return Word(f->words, f->h.view.count[messageVariables] + expression) & 0xFFFFFF;
}

static void Put(Writer* w, const void* bytes, size_t length)
{
    if (length > 0 && w->buffer != NULL && w->length < w->capacity)
    {
        size_t room = w->capacity - w->length;
        memcpy(w->buffer + w->length, bytes, length < room ? length : room);
    }
    w->length = length > SIZE_MAX - w->length ? SIZE_MAX : w->length + length;
}

static void Fault(Formatting* f, mlocMessageFaultKind kind, uint32_t at)
{
    if (f->faults != NULL && f->faultCount < f->faults->capacity)
    {
        f->faults->faults[f->faultCount].kind = kind;
        f->faults->faults[f->faultCount].at = at;
    }
    f->faultCount++;
}

static const uint8_t* Pool(const Formatting* f, uint32_t offset)
{
    return f->h.view.pool + offset;
}

// Whether an argument's name is the variable's (NFC, from the compiled
// message): byte for byte when the argument's is NFC, as it usually is,
// else normalized on the stack first; a name longer than the stack's
// buffer that is not NFC is not found.
static bool SameName(const mlocMessageArgument* a, const uint8_t* name, size_t length)
{
    if (a->name == NULL)
    {
        return false;
    }
    if (a->nameLength == length && memcmp(a->name, name, length) == 0)
    {
        return true;
    }
    muniQuickCheck answer = muni_quickCheckNo;
    if (muniCheckNormalization(a->name, a->nameLength, muni_nfc, &answer).status != muni_success ||
        answer == muni_quickCheckYes)
    {
        return false;
    }
    char normal[256];
    size_t needed = 0;
    muniTextResult result = muniNormalize(a->name, a->nameLength, muni_nfc, muni_convertStrict,
                                          normal, sizeof normal, &needed);
    return result.status == muni_success && needed == length && memcmp(normal, name, length) == 0;
}

// The argument of a variable's name, or null.
static const mlocMessageValue* Argument(const Formatting* f, uint32_t variable)
{
    const uint8_t* name = Pool(f, Field(f, messageVariables, variable, 0));
    size_t length = Field(f, messageVariables, variable, 1);
    for (size_t i = 0; i < f->argumentCount; i++)
    {
        if (SameName(&f->arguments[i], name, length))
        {
            return &f->arguments[i].value;
        }
    }
    return NULL;
}

static Value Fallback(ValueKind kind, const uint8_t* text, size_t length, uint32_t variable)
{
    Value v = {kind, text, length, NULL, variable};
    return v;
}

static Value ResolveExpression(Formatting* f, uint32_t expression);

// A variable's value: its declaration's, or the argument of its name
// (the argument too for the operand of the .input that declares it).
static Value ResolveVariable(Formatting* f, uint32_t variable, uint32_t at, uint32_t within)
{
    uint32_t d = BoundBy(f, variable);
    if (d != messageUnbound && Field(f, messageDeclarations, d, 2) != within)
    {
        Value v = ResolveExpression(f, Field(f, messageDeclarations, d, 2));
        if (v.kind >= valueFallbackVariable)
        {
            return Fallback(valueFallbackVariable, NULL, 0, variable);
        }
        return v;
    }
    const mlocMessageValue* argument = Argument(f, variable);
    if (argument == NULL)
    {
        Fault(f, mloc_messageFaultUnresolvedVariable, at);
        return Fallback(valueFallbackVariable, NULL, 0, variable);
    }
    Value v = {valueArgument, NULL, 0, argument, variable};
    if (argument->kind == mloc_messageValueString)
    {
        v.kind = valueText;
        v.text = (const uint8_t*)argument->as.string.text;
        v.length = argument->as.string.length;
    }
    return v;
}

static Value ResolveExpression(Formatting* f, uint32_t expression)
{
    uint32_t at = Field(f, messageExpressions, expression, 7);
    uint32_t operand = Field(f, messageExpressions, expression, 0);
    Value v = {valueText, NULL, 0, NULL, 0};
    if (operand == operandVariable)
    {
        v = ResolveVariable(f, Field(f, messageExpressions, expression, 1), at, expression);
    }
    else if (operand == operandLiteral)
    {
        v.text = Pool(f, Field(f, messageExpressions, expression, 1));
        v.length = Field(f, messageExpressions, expression, 2);
    }
    if (SlotOf(f, expression) == messageFunctionNone)
    {
        return v;
    }
    // The formatter has no functions yet: each is unknown.
    Fault(f, mloc_messageFaultUnknownFunction, at);
    if (operand == operandVariable)
    {
        return Fallback(valueFallbackVariable, NULL, 0,
                        Field(f, messageExpressions, expression, 1));
    }
    if (operand == operandLiteral)
    {
        return Fallback(valueFallbackLiteral, v.text, v.length, 0);
    }
    return Fallback(valueFallbackFunction, Pool(f, Field(f, messageExpressions, expression, 3)),
                    Field(f, messageExpressions, expression, 4), 0);
}

// A fallback's string in braces: "{$name}", "{|literal|}" with "\" and
// "|" escaped, "{:function}".
static void PutFallback(Formatting* f, const Value* v)
{
    Writer* w = &f->w;
    Put(w, "{", 1);
    if (v->kind == valueFallbackVariable)
    {
        Put(w, "$", 1);
        Put(w, Pool(f, Field(f, messageVariables, v->variable, 0)),
            Field(f, messageVariables, v->variable, 1));
    }
    else if (v->kind == valueFallbackFunction)
    {
        Put(w, ":", 1);
        Put(w, v->text, v->length);
    }
    else
    {
        Put(w, "|", 1);
        size_t from = 0;
        for (size_t i = 0; i < v->length; i++)
        {
            if (v->text[i] == '\\' || v->text[i] == '|')
            {
                Put(w, v->text + from, i - from);
                Put(w, "\\", 1);
                from = i;
            }
        }
        Put(w, v->text + from, v->length - from);
        Put(w, "|", 1);
    }
    Put(w, "}", 1);
}

// An argument written with no function: a number as its plain decimal
// (the number functions come with their slice); an object cannot be.
static void PutArgument(Formatting* f, const Value* v, uint32_t at)
{
    mlocDecimal decimal;
    mlocResult made = mloc_errorInvalid;
    const mlocMessageValue* a = v->argument;
    if (a->kind == mloc_messageValueInteger)
    {
        made = mlocMakeDecimal(a->as.integer, &decimal);
    }
    else if (a->kind == mloc_messageValueDouble)
    {
        made = mlocMakeShortestDecimal(a->as.number, &decimal);
    }
    else if (a->kind == mloc_messageValueDecimal && a->as.decimal != NULL)
    {
        decimal = *a->as.decimal;
        made = mloc_success;
    }
    char digits[64];
    mlocOutput out = {digits, sizeof digits, 0, NULL, NULL};
    if (made != mloc_success || mlocFormatDecimal(&decimal, &out) != mloc_success)
    {
        Fault(f, mloc_messageFaultUnsupportedOperation, at);
        Value fallback = Fallback(valueFallbackVariable, NULL, 0, v->variable);
        PutFallback(f, &fallback);
        return;
    }
    Put(&f->w, digits, out.length);
}

static void PutExpression(Formatting* f, uint32_t expression)
{
    Value v = ResolveExpression(f, expression);
    if (v.kind == valueText)
    {
        Put(&f->w, v.text, v.length);
    }
    else if (v.kind == valueArgument)
    {
        PutArgument(f, &v, Field(f, messageExpressions, expression, 7));
    }
    else
    {
        PutFallback(f, &v);
    }
}

static void PutPattern(Formatting* f, uint32_t first, uint32_t count)
{
    for (uint32_t i = first; i < first + count; i++)
    {
        uint32_t kind = Field(f, messagePieces, i, 0);
        if (kind == pieceText)
        {
            Put(&f->w, Pool(f, Field(f, messagePieces, i, 1)), Field(f, messagePieces, i, 2));
        }
        else if (kind == pieceExpression)
        {
            PutExpression(f, Field(f, messagePieces, i, 1));
        }
        // Markup writes nothing in text.
    }
}

// The variant selected. No selector supports selection yet (the
// formatter has no functions), so each is resolved, reported as a bad
// selector, and only a variant of catch-all keys matches: the first.
static uint32_t SelectVariant(Formatting* f)
{
    const MessageView* v = &f->h.view;
    for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
    {
        uint32_t variable = Field(f, messageSelectors, s, 0);
        uint32_t at = Field(f, messageSelectors, s, 1);
        Value value = ResolveVariable(f, variable, at, UINT32_MAX);
        (void)value;
        Fault(f, mloc_messageFaultBadSelector, at);
    }
    for (uint32_t variant = 0; variant < v->count[messageVariants]; variant++)
    {
        bool catchAll = true;
        for (uint32_t s = 0; s < v->count[messageSelectors] && catchAll; s++)
        {
            uint32_t offset;
            uint32_t length;
            catchAll = mlocMessageKey(v, variant, s, &offset, &length) == keyCatchAll;
        }
        if (catchAll)
        {
            return variant;
        }
    }
    return UINT32_MAX;
}

// Whether the arguments hold what their kinds say: a name and a string
// where their lengths ask, a decimal, a known kind.
static bool GoodArguments(const mlocMessageArgument* arguments, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        const mlocMessageArgument* a = &arguments[i];
        const mlocMessageValue* v = &a->value;
        bool good = (a->name != NULL || a->nameLength == 0) &&
                    (uint32_t)v->kind <= mloc_messageValueObject &&
                    (v->kind != mloc_messageValueString || v->as.string.text != NULL ||
                     v->as.string.length == 0) &&
                    (v->kind != mloc_messageValueDecimal || v->as.decimal != NULL);
        if (!good)
        {
            return false;
        }
    }
    return true;
}

mlocResult mlocFormatMessage(const void* bound, const mlocMessageArgument* arguments,
                             size_t argumentCount, mlocOutput* output, mlocMessageFaults* faults)
{
    Formatting f;
    memset(&f, 0, sizeof f);
    if (!mlocReadBound(bound, &f.h, &f.formatter) || output == NULL ||
        (output->buffer == NULL && output->capacity > 0) ||
        (arguments == NULL && argumentCount > 0) || !GoodArguments(arguments, argumentCount) ||
        (faults != NULL && faults->faults == NULL && faults->capacity > 0))
    {
        return mloc_errorInvalid;
    }
    f.words = mlocBoundWords(bound);
    f.arguments = arguments;
    f.argumentCount = argumentCount;
    f.w.buffer = output->buffer;
    f.w.capacity = output->capacity;
    f.faults = faults;
    const MessageView* v = &f.h.view;
    if (v->count[messageVariants] == 0)
    {
        PutPattern(&f, 0, v->count[messagePieces]);
    }
    else
    {
        uint32_t variant = SelectVariant(&f);
        // A compiled message always has a variant of catch-all keys.
        if (variant != UINT32_MAX)
        {
            PutPattern(&f, Field(&f, messageVariants, variant, 0),
                       Field(&f, messageVariants, variant, 1));
        }
    }
    if (faults != NULL)
    {
        faults->count = f.faultCount;
    }
    if (f.faultCount > 0 && f.formatter.strict)
    {
        output->length = 0;
        return mloc_errorFormat;
    }
    output->length = f.w.length;
    if (f.w.length > f.w.capacity)
    {
        return mloc_errorCapacity;
    }
    return f.faultCount > 0 ? mloc_withFallbacks : mloc_success;
}
