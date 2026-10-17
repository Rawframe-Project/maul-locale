// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formatting a bound message (maul-locale/message.h, record mloc-0045):
// the pattern selected, then each piece written, each placeholder
// resolved where it stands (message_resolve).

#include "message_format.h"

#include "message_resolve.h"

#include <string.h>

// The bytes that fit go to the buffer; length counts them all.
typedef struct Writer
{
    char* buffer;
    size_t capacity;
    size_t length;
} Writer;

typedef struct Formatting
{
    Resolution r;
    Writer w;
} Formatting;

static uint32_t Field(const Formatting* f, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocResolutionField(&f->r, kind, index, field);
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

static const uint8_t* Pool(const Formatting* f, uint32_t offset)
{
    return f->r.h.view.pool + offset;
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
// (the number functions come with their slice); an object cannot be,
// which is reported, false.
static bool PutArgument(Formatting* f, const Value* v, uint32_t expression)
{
    char digits[64];
    size_t length = 0;
    if (!mlocArgumentDigits(v->argument, digits, sizeof digits, &length))
    {
        mlocReportFault(&f->r, mloc_messageFaultUnsupportedOperation,
                        Field(f, messageExpressions, expression, 7));
        return false;
    }
    Put(&f->w, digits, length);
    return true;
}

// A program function's value, written by its format entry into the room
// left; a fault, or no format entry, is reported, false.
static bool PutResolved(Formatting* f, const Value* v, uint32_t expression)
{
    const mlocMessageFunction* function = &f->r.formatter.functions[v->function];
    Writer* w = &f->w;
    bool room = w->buffer != NULL && w->length < w->capacity;
    mlocOutput out = {room ? w->buffer + w->length : NULL, room ? w->capacity - w->length : 0, 0,
                      NULL, NULL};
    mlocMessageFaultKind fault = function->format == NULL
                                     ? mloc_messageFaultUnsupportedOperation
                                     : function->format(function->context, &v->resolved, &out);
    if (fault != mloc_messageFaultNone)
    {
        mlocReportFault(&f->r, fault, Field(f, messageExpressions, expression, 7));
        return false;
    }
    w->length = out.length > SIZE_MAX - w->length ? SIZE_MAX : w->length + out.length;
    return true;
}

// Writes a value; false when the expression's own fallback must stand
// in its place (a fault reported, a fallback value, which is the
// expression's, or :string over a failed operand).
static bool PutValue(Formatting* f, const Value* v, uint32_t expression)
{
    ValueKind kind = v->kind == valueString ? v->behind : v->kind;
    if (kind == valueText)
    {
        Put(&f->w, v->text, v->length);
        return true;
    }
    if (kind == valueArgument)
    {
        return PutArgument(f, v, expression);
    }
    if (kind == valueResolved)
    {
        return PutResolved(f, v, expression);
    }
    return false;
}

// The isolate a placeholder opens with by the Default Bidi Strategy
// (LRI, RLI or FSI in UTF-8, closed by PDI), or null: left to right in
// a left-to-right message without u:dir, or isolation off.
static const char* Opening(const Formatting* f, const Value* v)
{
    const MessageFormatter* formatter = &f->r.formatter;
    if (!formatter->isolate)
    {
        return NULL;
    }
    if (v->direction == mloc_directionLeftToRight)
    {
        bool bare = formatter->direction == mloc_directionLeftToRight && !v->isolate;
        return bare ? NULL : "\xE2\x81\xA6";
    }
    return v->direction == mloc_directionRightToLeft ? "\xE2\x81\xA7" : "\xE2\x81\xA8";
}

static void PutExpression(Formatting* f, uint32_t expression)
{
    Value v = mlocResolveExpression(&f->r, expression);
    size_t start = f->w.length;
    const char* open = Opening(f, &v);
    if (open != NULL)
    {
        Put(&f->w, open, 3);
    }
    if (!PutValue(f, &v, expression))
    {
        // What was written goes; the fallback, of unknown direction,
        // takes its place.
        f->w.length = start;
        Value fallback = mlocExpressionFallback(&f->r, expression);
        open = Opening(f, &fallback);
        if (open != NULL)
        {
            Put(&f->w, open, 3);
        }
        PutFallback(f, &fallback);
    }
    if (open != NULL)
    {
        Put(&f->w, "\xE2\x81\xA9", 3);
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
        else
        {
            // Markup writes nothing in text; its options still resolve.
            mlocResolveMarkup(&f->r, Field(f, messagePieces, i, 1));
        }
    }
}

// A selector's value, and whether a fault has stopped its matching (it
// then matches only "*").
typedef struct Selector
{
    Value value;
    bool broken;
} Selector;

// Whether a selector's value can select: a :string value, or a program
// function's with a match entry.
static bool Selectable(const Formatting* f, const Value* v)
{
    return v->kind == valueString ||
           (v->kind == valueResolved && f->r.formatter.functions[v->function].match != NULL);
}

// Whether a key matches a selector; a fault breaks the selector, reported
// once as a bad selector.
static bool KeyMatches(Formatting* f, Selector* s, uint32_t at, const uint8_t* key, size_t length)
{
    if (s->broken)
    {
        return false;
    }
    bool matched = false;
    mlocMessageFaultKind fault = mloc_messageFaultNone;
    if (s->value.kind == valueString)
    {
        matched = mlocStringMatches(&s->value, key, length, &fault);
    }
    else
    {
        const mlocMessageFunction* function = &f->r.formatter.functions[s->value.function];
        fault = function->match(function->context, &s->value.resolved, (const char*)key, length,
                                &matched);
    }
    if (fault != mloc_messageFaultNone)
    {
        s->broken = true;
        mlocReportFault(&f->r, mloc_messageFaultBadSelector, at);
        return false;
    }
    return matched;
}

// Whether each of a variant's keys matches its selector.
static bool VariantMatches(Formatting* f, Selector* selectors, uint32_t variant)
{
    const MessageView* v = &f->r.h.view;
    for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
    {
        uint32_t offset;
        uint32_t length;
        if (mlocMessageKey(v, variant, s, &offset, &length) == keyCatchAll)
        {
            continue;
        }
        if (!KeyMatches(f, &selectors[s], Field(f, messageSelectors, s, 1), Pool(f, offset),
                        length))
        {
            return false;
        }
    }
    return true;
}

// Whether a matching variant is a better match than another: at the first
// selector whose keys differ, a key beats "*", and of two keys the one
// the function calls better; keys neither of which is better tie there.
static bool BetterVariant(const Formatting* f, const Selector* selectors, uint32_t variant,
                          uint32_t than)
{
    const MessageView* v = &f->r.h.view;
    for (uint32_t s = 0; s < v->count[messageSelectors]; s++)
    {
        uint32_t offset1;
        uint32_t length1;
        uint32_t offset2;
        uint32_t length2;
        bool all1 = mlocMessageKey(v, variant, s, &offset1, &length1) == keyCatchAll;
        bool all2 = mlocMessageKey(v, than, s, &offset2, &length2) == keyCatchAll;
        if (all1 != all2)
        {
            return all2;
        }
        const Value* value = &selectors[s].value;
        if (all1 || value->kind != valueResolved ||
            (length1 == length2 && memcmp(Pool(f, offset1), Pool(f, offset2), length1) == 0))
        {
            continue;
        }
        const mlocMessageFunction* function = &f->r.formatter.functions[value->function];
        if (function->better == NULL)
        {
            continue;
        }
        const char* key1 = (const char*)Pool(f, offset1);
        const char* key2 = (const char*)Pool(f, offset2);
        if (function->better(function->context, &value->resolved, key1, length1, key2, length2))
        {
            return true;
        }
        if (function->better(function->context, &value->resolved, key2, length2, key1, length1))
        {
            return false;
        }
    }
    return false;
}

// The variant selected (UTS #35 Part 9, "Pattern Selection"): each
// selector resolved, a value that cannot select reported as a bad
// selector and matching only "*"; of the variants whose keys all match,
// the best, the first of those that tie.
static uint32_t SelectVariant(Formatting* f)
{
    const MessageView* v = &f->r.h.view;
    Selector selectors[messageSelectorLimit];
    uint32_t count = v->count[messageSelectors];
    for (uint32_t s = 0; s < count; s++)
    {
        uint32_t at = Field(f, messageSelectors, s, 1);
        selectors[s].value =
            mlocResolveVariable(&f->r, Field(f, messageSelectors, s, 0), at, UINT32_MAX);
        selectors[s].broken = !Selectable(f, &selectors[s].value);
        if (selectors[s].broken)
        {
            mlocReportFault(&f->r, mloc_messageFaultBadSelector, at);
        }
    }
    uint32_t best = UINT32_MAX;
    for (uint32_t variant = 0; variant < v->count[messageVariants]; variant++)
    {
        if (VariantMatches(f, selectors, variant) &&
            (best == UINT32_MAX || BetterVariant(f, selectors, variant, best)))
        {
            best = variant;
        }
    }
    return best;
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
    if (!mlocReadBound(bound, &f.r.h, &f.r.formatter) || output == NULL ||
        (output->buffer == NULL && output->capacity > 0) ||
        (arguments == NULL && argumentCount > 0) || !GoodArguments(arguments, argumentCount) ||
        (faults != NULL && faults->faults == NULL && faults->capacity > 0))
    {
        return mloc_errorInvalid;
    }
    f.r.words = mlocBoundWords(bound);
    f.r.arguments = arguments;
    f.r.argumentCount = argumentCount;
    f.w.buffer = output->buffer;
    f.w.capacity = output->capacity;
    f.r.faults = faults;
    f.r.budget = messageResolutionLimit;
    const MessageView* v = &f.r.h.view;
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
        faults->count = f.r.faultCount;
    }
    if (f.r.faultCount > 0 && f.r.formatter.strict)
    {
        output->length = 0;
        return mloc_errorFormat;
    }
    output->length = f.w.length;
    if (f.w.length > f.w.capacity)
    {
        return mloc_errorCapacity;
    }
    return f.r.faultCount > 0 ? mloc_withFallbacks : mloc_success;
}
