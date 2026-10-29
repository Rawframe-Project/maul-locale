// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Formatting a bound message (maul-locale/message.h): the pattern
// selected, then each piece written, each placeholder resolved where it
// stands (message_resolve).

#include "message_format.h"

#include "decimal.h"
#include "invariant.h"
#include "message_prepare.h"
#include "message_resolve.h"

#include <string.h>

// The bytes that fit go to the buffer; length counts them all. In the
// pass that reports parts, part is the caller's.
typedef struct Writer
{
    char* buffer;
    size_t capacity;
    size_t length;
    mlocPartFunction part;
    void* context;
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

// Reports a message part of a kind and index, in the pass that reports.
static void Report(const Writer* w, uint32_t kind, uint32_t index, size_t start, size_t length)
{
    if (w->part != NULL)
    {
        w->part(w->context, index << 8 | kind, start, length);
    }
}

// What a function's parts are moved by: the place its text starts.
typedef struct Inner
{
    const Writer* w;
    size_t start;
} Inner;

// Passes on a function's part at its place in the message; a kind whose
// low byte is a message part's is not the function's to give.
static void PassPart(void* context, uint32_t part, size_t start, size_t length)
{
    const Inner* inner = context;
    if ((part & 0xFF) < mloc_messagePartText)
    {
        inner->w->part(inner->w->context, part, inner->start + start, length);
    }
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
    // A zoned date-time's text is the longest.
    char digits[128];
    size_t length = 0;
    if (!mlocArgumentDigits(v->argument, digits, sizeof digits, &length))
    {
        mlocReportFault(&f->r, mloc_messageFaultUnsupportedOperation,
                        Field(f, messageExpressions, expression, 7));
        return false;
    }
    Put(&f->w, digits, length);
    if (v->argument->kind == mloc_messageValueUnit)
    {
        // The amount and the unit, as ICU4J writes a Measure.
        Put(&f->w, " ", 1);
        Put(&f->w, v->argument->as.unit.unit, v->argument->as.unit.unitLength);
    }
    return true;
}

// The output over the writer's room left, passing parts on through
// inner when the writer reports them.
static mlocOutput Room(const Writer* w, Inner* inner)
{
    bool room = w->buffer != NULL && w->length < w->capacity;
    inner->w = w;
    inner->start = w->length;
    mlocOutput out = {room ? w->buffer + w->length : NULL, room ? w->capacity - w->length : 0, 0,
                      w->part != NULL ? PassPart : NULL, inner};
    return out;
}

// A function's value, written into the room left by its program
// function's format entry or by the number formatter; a fault, or no
// format entry, is reported, false.
static bool PutResolved(Formatting* f, const Value* v, uint32_t expression)
{
    Writer* w = &f->w;
    Inner inner;
    mlocOutput out = Room(w, &inner);
    mlocMessageFaultKind fault;
    if (v->kind == valueNumber)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedNumber* prepared = mlocPreparedFor(&f->r, v->number.origin);
        fault = mlocWordsValue(&v->number)
                    ? mlocWriteWordsValue(&v->number, prepared, formatter->data, &formatter->locale,
                                          &out)
                    : mlocWriteNumberValue(&v->number, prepared, formatter->data,
                                           &formatter->locale, &out);
    }
    else if (v->kind == valueDate)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedDate* prepared = mlocPreparedFor(&f->r, v->date.origin);
        fault = mlocWriteDateValue(&v->date, prepared, formatter->data, &formatter->locale,
                                   formatter->hasZone ? &formatter->zone : NULL, &out);
    }
    else if (v->kind == valueDuration)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedDuration* prepared = mlocPreparedFor(&f->r, v->duration.origin);
        fault = mlocWriteDurationValue(&v->duration, prepared, formatter->data, &formatter->locale,
                                       &out);
    }
    else if (v->kind == valueList)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedList* prepared = mlocPreparedFor(&f->r, v->list.origin);
        fault = mlocWriteListValue(&v->list, prepared, formatter->data, &formatter->locale, &out);
    }
    else if (v->kind == valueGrammar)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedGrammar* prepared = mlocPreparedFor(&f->r, v->grammar.origin);
        fault =
            mlocWriteGrammarValue(&v->grammar, prepared, formatter->data, &formatter->locale, &out);
    }
    else if (v->kind == valueGender)
    {
        // A gender has no text: it only selects.
        fault = mloc_messageFaultUnsupportedOperation;
    }
    else if (v->kind == valueResolved)
    {
        const mlocMessageFunction* function = &f->r.formatter.functions[v->function];
        fault = function->format == NULL ? mloc_messageFaultUnsupportedOperation
                                         : function->format(function->context, &v->resolved, &out);
    }
    else
    {
        // No function's value stands behind :string.
        fault = mloc_messageFaultUnsupportedOperation;
    }
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
    if (kind == valueResolved || kind == valueNumber || kind == valueDate ||
        kind == valueDuration || kind == valueList || kind == valueGrammar || kind == valueGender)
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

// Writes an isolation mark, reported as a part.
static void PutMark(Writer* w, const char* mark)
{
    Report(w, mloc_messagePartIsolation, 0, w->length, 3);
    Put(w, mark, 3);
}

// Measures a value's text, for the pass that reports parts: false when
// it falls back.
static bool Measure(Formatting* f, const Value* v, uint32_t expression, size_t* lengthOut)
{
    Writer saved = f->w;
    f->w = (Writer){NULL, 0, 0, NULL, NULL};
    bool good = PutValue(f, v, expression);
    *lengthOut = f->w.length;
    f->w = saved;
    return good;
}

static void PutExpression(Formatting* f, uint32_t expression)
{
    Value v = mlocResolveExpression(&f->r, expression);
    size_t start = f->w.length;
    size_t length = 0;
    // The pass that reports parts reports the value's range before the
    // parts inside it, so it measures first.
    bool good = f->w.part == NULL || Measure(f, &v, expression, &length);
    const char* open = Opening(f, &v);
    if (good)
    {
        if (open != NULL)
        {
            PutMark(&f->w, open);
        }
        Report(&f->w, mloc_messagePartPlaceholder, expression, f->w.length, length);
        good = PutValue(f, &v, expression);
    }
    if (!good)
    {
        // What was written goes; the fallback, of unknown direction,
        // takes its place.
        f->w.length = start;
        Value fallback = mlocExpressionFallback(&f->r, expression);
        open = Opening(f, &fallback);
        if (open != NULL)
        {
            PutMark(&f->w, open);
        }
        size_t at = f->w.length;
        PutFallback(f, &fallback);
        Report(&f->w, mloc_messagePartFallback, expression, at, f->w.length - at);
    }
    if (open != NULL)
    {
        PutMark(&f->w, "\xE2\x81\xA9");
    }
}

static void PutPattern(Formatting* f, uint32_t first, uint32_t count)
{
    for (uint32_t i = first; i < first + count; i++)
    {
        uint32_t kind = Field(f, messagePieces, i, 0);
        if (kind == pieceText)
        {
            uint32_t length = Field(f, messagePieces, i, 2);
            if (length > 0)
            {
                Report(&f->w, mloc_messagePartText, 0, f->w.length, length);
            }
            Put(&f->w, Pool(f, Field(f, messagePieces, i, 1)), length);
        }
        else if (kind == pieceExpression)
        {
            PutExpression(f, Field(f, messagePieces, i, 1));
        }
        else
        {
            // Markup writes nothing in text; its options still resolve.
            uint32_t markup = Field(f, messagePieces, i, 1);
            mlocResolveMarkup(&f->r, markup);
            Report(&f->w, mloc_messagePartMarkupOpen + Field(f, messageMarkups, markup, 0), markup,
                   f->w.length, 0);
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

// Whether a selector's value can select: a :string value, a mloc:list
// value, a number value whose select was not a variable's or an operand's, or a program
// function's with a match entry.
static bool Selectable(const Formatting* f, const Value* v)
{
    return v->kind == valueString || v->kind == valueList || v->kind == valueGender ||
           (v->kind == valueNumber && v->number.selectable) ||
           (v->kind == valueResolved && f->r.formatter.functions[v->function].match != NULL);
}

// Whether a key matches a selector. A bad variant key is reported and
// matches nothing; another fault breaks the selector, reported once as a
// bad selector.
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
    else if (s->value.kind == valueList)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedList* prepared = mlocPreparedFor(&f->r, s->value.list.origin);
        fault = mlocMatchList(&s->value.list, prepared, formatter->data, &formatter->locale, key,
                              length, &matched);
    }
    else if (s->value.kind == valueGender)
    {
        fault = mlocMatchGender(&s->value.gender, key, length, &matched);
    }
    else if (s->value.kind == valueNumber)
    {
        const MessageFormatter* formatter = &f->r.formatter;
        const PreparedNumber* prepared = mlocPreparedFor(&f->r, s->value.number.origin);
        fault = mlocMatchNumber(&s->value.number, prepared, formatter->data, &formatter->locale,
                                key, length, &matched);
    }
    else
    {
        const mlocMessageFunction* function = &f->r.formatter.functions[s->value.function];
        fault = function->match(function->context, &s->value.resolved, (const char*)key, length,
                                &matched);
    }
    if (fault == mloc_messageFaultBadVariantKey)
    {
        mlocReportFault(&f->r, fault, at);
        return false;
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
        bool numeric = value->kind == valueNumber || value->kind == valueList;
        if (all1 || (value->kind != valueResolved && !numeric) ||
            (length1 == length2 && memcmp(Pool(f, offset1), Pool(f, offset2), length1) == 0))
        {
            continue;
        }
        if (numeric)
        {
            // A number literal beats a plural keyword.
            if (mlocNumberKeyBetter(Pool(f, offset1), length1, Pool(f, offset2), length2))
            {
                return true;
            }
            if (mlocNumberKeyBetter(Pool(f, offset2), length2, Pool(f, offset1), length1))
            {
                return false;
            }
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
    Selector selectors[MLOC_MAX_MESSAGE_SELECTORS];
    uint32_t count = v->count[messageSelectors];
    // Binding refused a message of more selectors.
    MLOC_ASSERT(count <= MLOC_MAX_MESSAGE_SELECTORS);
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

static bool GoodDecimal(const mlocDecimal* decimal)
{
    return decimal != NULL && mlocValidDecimal(decimal);
}

static bool GoodCurrency(const mlocMessageValue* v)
{
    const char* code = v->as.currency.code;
    for (int i = 0; i < 3; i++)
    {
        if (!((code[i] >= 'A' && code[i] <= 'Z') || (code[i] >= 'a' && code[i] <= 'z')))
        {
            return false;
        }
    }
    return GoodDecimal(v->as.currency.amount);
}

// Whether a value holds what its kind says: a string where its length
// asks, a decimal, an amount and its code, a date, time or instant the
// library makes, a zone it made, a known kind.
static bool GoodValue(const mlocMessageValue* v)
{
    switch (v->kind)
    {
    case mloc_messageValueString:
        return v->as.string.text != NULL || v->as.string.length == 0;
    case mloc_messageValueInteger:
    case mloc_messageValueDouble:
    case mloc_messageValueObject:
        return true;
    case mloc_messageValueDecimal:
        return GoodDecimal(v->as.decimal);
    case mloc_messageValueCurrency:
        return GoodCurrency(v);
    case mloc_messageValueUnit:
        return GoodDecimal(v->as.unit.amount) &&
               (v->as.unit.unit != NULL || v->as.unit.unitLength == 0);
    case mloc_messageValueDate:
        return mlocGoodDate(&v->as.date);
    case mloc_messageValueTime:
        return mlocGoodTime(&v->as.time);
    case mloc_messageValueDateTime:
        return mlocGoodDate(&v->as.dateTime.date) && mlocGoodTime(&v->as.dateTime.time);
    case mloc_messageValueInstant:
        return mlocGoodInstant(&v->as.instant);
    case mloc_messageValueZonedDateTime:
        return mlocGoodZoned(v);
    case mloc_messageValueDuration:
        return mlocGoodDuration(v->as.duration);
    case mloc_messageValueList:
        return mlocGoodList(v);
    default:
        return false;
    }
}

// Whether the arguments have names where their lengths ask and hold
// what their kinds say.
static bool GoodArguments(const mlocMessageArgument* arguments, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if ((arguments[i].name == NULL && arguments[i].nameLength != 0) ||
            !GoodValue(&arguments[i].value))
        {
            return false;
        }
    }
    return true;
}

// Writes the selected pattern, faults reported unless quiet.
static void Run(Formatting* f, uint32_t quiet)
{
    f->r.quiet = quiet;
    f->r.budget = MLOC_MAX_MESSAGE_RESOLUTIONS;
    f->r.exhausted = false;
    f->r.optionDepth = 0;
    const MessageView* v = &f->r.h.view;
    // The reported bits of the declarations there are, which binding
    // kept within the bits.
    MLOC_ASSERT(v->count[messageDeclarations] <= MLOC_MAX_MESSAGE_DECLARATIONS);
    memset(f->r.reported, 0, (v->count[messageDeclarations] + 7) / 8);
    if (v->count[messageVariants] == 0)
    {
        PutPattern(f, 0, v->count[messagePieces]);
        return;
    }
    uint32_t variant = SelectVariant(f);
    // A compiled message always has a variant of catch-all keys.
    if (variant != UINT32_MAX)
    {
        PutPattern(f, Field(f, messageVariants, variant, 0), Field(f, messageVariants, variant, 1));
    }
}

mlocResult mlocFormatMessage(const void* bound, const mlocMessageArgument* arguments,
                             size_t argumentCount, mlocOutput* textOut,
                             mlocMessageFaults* faultsOut)
{
    // Each field set below rather than the whole state cleared: the
    // clearing cost more than a short message's formatting.
    Formatting f;
    if (!mlocReadBound(bound, &f.r.h, &f.r.formatter) || textOut == NULL ||
        (textOut->buffer == NULL && textOut->capacity > 0) ||
        (arguments == NULL && argumentCount > 0) || !GoodArguments(arguments, argumentCount) ||
        (faultsOut != NULL && faultsOut->faults == NULL && faultsOut->capacity > 0))
    {
        return mloc_errorInvalid;
    }
    f.r.words = mlocBoundWords(bound);
    f.r.arguments = arguments;
    f.r.argumentCount = argumentCount;
    f.r.faults = faultsOut;
    f.r.faultCount = 0;
    f.r.probe = false;
    f.r.probeArgument = NULL;
    f.r.dynamic = false;
    f.w = (Writer){textOut->buffer, textOut->capacity, 0, NULL, NULL};
    Run(&f, 0);
    if (textOut->part != NULL && f.w.length <= f.w.capacity &&
        !(f.r.faultCount > 0 && f.r.formatter.strict))
    {
        // The same text again, its parts reported, its faults already
        // counted.
        size_t faultCount = f.r.faultCount;
        f.w = (Writer){textOut->buffer, textOut->capacity, 0, textOut->part, textOut->context};
        Run(&f, 1);
        f.r.faultCount = faultCount;
    }
    if (faultsOut != NULL)
    {
        faultsOut->count = f.r.faultCount;
    }
    if (f.r.faultCount > 0 && f.r.formatter.strict)
    {
        textOut->length = 0;
        return mloc_errorFallback;
    }
    textOut->length = f.w.length;
    if (f.w.length > f.w.capacity)
    {
        return mloc_errorCapacity;
    }
    return f.r.faultCount > 0 ? mloc_withFallbacks : mloc_success;
}
