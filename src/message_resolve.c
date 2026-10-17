// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen

#include "message_resolve.h"

#include <assert.h>
#include <string.h>

// What an mlocMessageCall holds while a program function is called.
typedef struct CallState
{
    Resolution* r;
    uint32_t expression;
    uint32_t stamp;
} CallState;

enum
{
    callStamp = 0x4D43414C,
};

// Keeps a function off the resolver's recursion: a chain of declarations
// nests 64 levels deep, and what a leaf needs should not be on the stack
// at each level.
#if defined(_MSC_VER) && !defined(__clang__)
#define MLOC_NOINLINE __declspec(noinline)
#else
#define MLOC_NOINLINE __attribute__((noinline))
#endif

static_assert(sizeof(CallState) <= sizeof(mlocMessageCall), "a call's state fits its value");
static_assert(sizeof(mlocMessageResolved) == 64, "a resolved value is 64 bytes");

uint32_t mlocResolutionField(const Resolution* r, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(&r->h.view, kind, index, field);
}

static uint32_t Field(const Resolution* r, uint32_t kind, uint32_t index, uint32_t field)
{
    return mlocMessageField(&r->h.view, kind, index, field);
}

static uint32_t Word(const uint8_t* words, uint32_t index)
{
    const uint8_t* p = words + (size_t)index * 4;
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint32_t BoundBy(const Resolution* r, uint32_t variable)
{
    uint32_t d = Word(r->words, variable);
    return d < r->h.view.count[messageDeclarations] ? d : messageUnbound;
}

static uint32_t SlotOf(const Resolution* r, uint32_t expression)
{
    return Word(r->words, r->h.view.count[messageVariables] + expression) & 0xFFFFFF;
}

static const uint8_t* Pool(const Resolution* r, uint32_t offset)
{
    return r->h.view.pool + offset;
}

void mlocReportFault(Resolution* r, mlocMessageFaultKind kind, uint32_t at)
{
    if (r->quiet > 0)
    {
        return;
    }
    if (r->faults != NULL && r->faultCount < r->faults->capacity)
    {
        r->faults->faults[r->faultCount].kind = kind;
        r->faults->faults[r->faultCount].at = at;
    }
    r->faultCount++;
}

// In a probe, what an argument stands for: a plain number as an operand.
static const mlocMessageValue s_probe = {mloc_messageValueInteger, 0, {.integer = 0}};

// The argument of a variable's name, or null.
MLOC_NOINLINE static const mlocMessageValue* Argument(Resolution* r, uint32_t variable)
{
    if (r->probe)
    {
        // An option's value from an argument changes from call to call.
        r->dynamic = r->dynamic || r->optionDepth > 0;
        return r->optionDepth > 0 ? NULL : &s_probe;
    }
    const uint8_t* name = Pool(r, Field(r, messageVariables, variable, 0));
    size_t length = Field(r, messageVariables, variable, 1);
    for (size_t i = 0; i < r->argumentCount; i++)
    {
        if (mlocSameName(&r->arguments[i], name, length))
        {
            return &r->arguments[i].value;
        }
    }
    return NULL;
}

static Value Make(ValueKind kind)
{
    Value v;
    memset(&v, 0, sizeof v);
    v.kind = kind;
    return v;
}

// Makes a value of a kind in place, its other fields zero (no copy on
// the recursion's frames).
static void SetKind(Value* v, ValueKind kind)
{
    memset(v, 0, sizeof *v);
    v->kind = kind;
}

static void SetFallbackVariable(Value* v, uint32_t variable)
{
    SetKind(v, valueFallbackVariable);
    v->variable = variable;
}

static Value FallbackVariable(uint32_t variable)
{
    Value v = Make(valueFallbackVariable);
    v.variable = variable;
    return v;
}

static bool IsFallback(ValueKind kind)
{
    return kind >= valueFallbackVariable;
}

Value mlocExpressionFallback(const Resolution* r, uint32_t expression)
{
    uint32_t operand = Field(r, messageExpressions, expression, 0);
    if (operand == operandVariable)
    {
        return FallbackVariable(Field(r, messageExpressions, expression, 1));
    }
    Value v = Make(operand == operandLiteral ? valueFallbackLiteral : valueFallbackFunction);
    uint32_t field = operand == operandLiteral ? 1 : 3;
    v.text = Pool(r, Field(r, messageExpressions, expression, field));
    v.length = Field(r, messageExpressions, expression, field + 1);
    return v;
}

static void ResolveInto(Resolution* r, uint32_t expression, Value* out);

// An argument's value, or a fallback with the fault reported.
MLOC_NOINLINE static void ArgumentInto(Resolution* r, uint32_t variable, uint32_t at, Value* out)
{
    const mlocMessageValue* argument = Argument(r, variable);
    if (argument == NULL)
    {
        mlocReportFault(r, mloc_messageFaultUnresolvedVariable, at);
        *out = FallbackVariable(variable);
        return;
    }
    *out = Make(valueArgument);
    out->argument = argument;
    out->variable = variable;
    if (argument->kind == mloc_messageValueString)
    {
        out->kind = valueText;
        out->text = (const uint8_t*)argument->as.string.text;
        out->length = argument->as.string.length;
    }
}

// A variable's value into out, the recursion's half that stays small:
// a declaration's expression is resolved straight into the caller's
// value.
static void VariableInto(Resolution* r, uint32_t variable, uint32_t at, uint32_t within, Value* out)
{
    uint32_t d = BoundBy(r, variable);
    if (d == messageUnbound || Field(r, messageDeclarations, d, 2) == within)
    {
        ArgumentInto(r, variable, at, out);
        return;
    }
    // Call-by-name resolves a declaration at each reference; its faults
    // are reported the first time it is resolved aloud, and quietly
    // after.
    uint8_t bit = (uint8_t)(1u << (d % 8));
    bool again = (r->reported[d / 8] & bit) != 0;
    if (r->quiet == 0)
    {
        r->reported[d / 8] |= bit;
    }
    r->quiet += again;
    ResolveInto(r, Field(r, messageDeclarations, d, 2), out);
    r->quiet -= again;
    if (IsFallback(out->kind))
    {
        SetFallbackVariable(out, variable);
    }
}

Value mlocResolveVariable(Resolution* r, uint32_t variable, uint32_t at, uint32_t within)
{
    Value v;
    VariableInto(r, variable, at, within, &v);
    return v;
}

// A value as a function receives it; storage holds a resolved value.
static void ToOperand(const Value* v, mlocMessageOperand* out, mlocMessageResolved* storage)
{
    memset(out, 0, sizeof *out);
    ValueKind kind = v->kind == valueString ? v->behind : v->kind;
    if (kind == valueText)
    {
        out->kind = mloc_messageOperandText;
        out->text = (const char*)v->text;
        out->length = v->length;
    }
    else if (kind == valueArgument)
    {
        out->kind = mloc_messageOperandArgument;
        out->argument = v->argument;
    }
    else if (kind == valueResolved)
    {
        *storage = v->resolved;
        out->kind = mloc_messageOperandResolved;
        out->function = v->function;
        out->resolved = storage;
    }
    else if (kind == valueNumber && v->number.function == numberFunctionCurrency)
    {
        storage->currency.amount = v->number.decimal;
        memcpy(storage->currency.code, v->number.code, 3);
        storage->currency.code[3] = 0;
        out->kind = mloc_messageOperandCurrency;
        out->number = &storage->currency.amount;
        out->text = storage->currency.code;
        out->length = 3;
    }
    else if (kind == valueNumber)
    {
        storage->decimal = v->number.decimal;
        out->kind = mloc_messageOperandNumber;
        out->number = &storage->decimal;
    }
    else
    {
        out->kind = mloc_messageOperandFallback;
    }
}

// Option resolution's report for a run of parameters: each option
// whose variable fails to resolve is a bad option (and is read as a
// fallback later).
MLOC_NOINLINE static void CheckParameters(Resolution* r, uint32_t first, uint32_t count,
                                          uint32_t at)
{
    for (uint32_t p = first; p < first + count; p++)
    {
        if (Field(r, messageParameters, p, 0) != parameterOptionVariable)
        {
            continue;
        }
        Value v = mlocResolveVariable(r, Field(r, messageParameters, p, 3), at, UINT32_MAX);
        if (IsFallback(v.kind))
        {
            mlocReportFault(r, mloc_messageFaultBadOption, at);
        }
    }
}

// Whether a parameter is an option of a name.
static bool OptionNamed(const Resolution* r, uint32_t p, const char* name, size_t length)
{
    uint32_t kind = Field(r, messageParameters, p, 0);
    return (kind == parameterOptionLiteral || kind == parameterOptionVariable) &&
           Field(r, messageParameters, p, 2) == length &&
           memcmp(Pool(r, Field(r, messageParameters, p, 1)), name, length) == 0;
}

// Whether a parameter's name is in the u: namespace, the library's.
static bool UnicodeOption(const Resolution* r, uint32_t p)
{
    return Field(r, messageParameters, p, 2) > 2 &&
           memcmp(Pool(r, Field(r, messageParameters, p, 1)), "u:", 2) == 0;
}

// An option's value: a literal's text, or its variable's value resolved
// again without reporting faults (option resolution reported them).
static Value OptionValue(Resolution* r, uint32_t p, uint32_t at)
{
    if (Field(r, messageParameters, p, 0) == parameterOptionLiteral)
    {
        Value v = Make(valueText);
        v.text = Pool(r, Field(r, messageParameters, p, 3));
        v.length = Field(r, messageParameters, p, 4);
        return v;
    }
    r->quiet++;
    r->optionDepth++;
    Value v = mlocResolveVariable(r, Field(r, messageParameters, p, 3), at, UINT32_MAX);
    r->optionDepth--;
    r->quiet--;
    return v;
}

// What an expression's u:dir asks: nothing, or a direction and whether
// to isolate.
typedef struct Directed
{
    bool set;
    mlocDirection direction;
    bool isolate;
} Directed;

static bool TextIs(const Value* v, const char* text)
{
    size_t length = strlen(text);
    bool textual = v->kind == valueText || (v->kind == valueString && v->behind == valueText);
    return textual && v->length == length && memcmp(v->text, text, length) == 0;
}

// The u: options of an expression's call (record mloc-0045): u:dir one
// of ltr, rtl, auto and inherit, u:id anything with a string; another
// value is a bad option and ignored (a failed variable already was).
// Other u: options are ignored.
MLOC_NOINLINE static Directed UnicodeOptions(Resolution* r, uint32_t expression)
{
    uint32_t first = Field(r, messageExpressions, expression, 5);
    uint32_t count = Field(r, messageExpressions, expression, 6);
    uint32_t at = Field(r, messageExpressions, expression, 7);
    Directed d = {false, mloc_directionUnknown, false};
    for (uint32_t p = first; p < first + count; p++)
    {
        bool dir = OptionNamed(r, p, "u:dir", 5);
        if (!dir && !OptionNamed(r, p, "u:id", 4))
        {
            continue;
        }
        Value v = OptionValue(r, p, at);
        bool good = true;
        if (IsFallback(v.kind))
        {
            continue;
        }
        if (dir)
        {
            static const char* const values[] = {"ltr", "rtl", "auto", "inherit"};
            static const mlocDirection directions[] = {
                mloc_directionLeftToRight, mloc_directionRightToLeft, mloc_directionUnknown,
                mloc_directionUnknown};
            good = false;
            for (int i = 0; i < 4 && !good; i++)
            {
                if (TextIs(&v, values[i]))
                {
                    good = true;
                    d.set = true;
                    d.direction = directions[i];
                    d.isolate = i < 3;
                }
            }
        }
        else
        {
            good = !(v.kind == valueArgument && v.argument->kind == mloc_messageValueObject);
        }
        if (!good)
        {
            mlocReportFault(r, mloc_messageFaultBadOption, at);
        }
    }
    return d;
}

void mlocResolveMarkup(Resolution* r, uint32_t markup)
{
    uint32_t first = Field(r, messageMarkups, markup, 3);
    uint32_t count = Field(r, messageMarkups, markup, 4);
    uint32_t at = Field(r, messageMarkups, markup, 5);
    CheckParameters(r, first, count, at);
    for (uint32_t p = first; p < first + count; p++)
    {
        if (OptionNamed(r, p, "u:dir", 5))
        {
            mlocReportFault(r, mloc_messageFaultBadOption, at);
        }
    }
}

// :string: the string value of its operand (text, an argument other than
// an object, another :string's; not a function's value); over a failed
// operand a value that matches no key and is written as that fallback.
MLOC_NOINLINE static Value StringOf(Resolution* r, uint32_t expression, const Value* operand)
{
    uint32_t at = Field(r, messageExpressions, expression, 7);
    bool good =
        operand != NULL && operand->kind != valueResolved && operand->kind != valueNumber &&
        !(operand->kind == valueArgument && operand->argument->kind == mloc_messageValueObject);
    if (!good)
    {
        mlocReportFault(r, mloc_messageFaultBadOperand, at);
        return mlocExpressionFallback(r, expression);
    }
    Value v = *operand;
    if (v.kind != valueString)
    {
        v.behind = v.kind;
        v.kind = valueString;
    }
    // A new value: its operand's u:dir is not its own.
    v.direction = mloc_directionUnknown;
    v.isolate = false;
    return v;
}

MLOC_NOINLINE static Value CallProgram(Resolution* r, uint32_t expression, uint32_t slot,
                                       const Value* operand)
{
    if (r->probe)
    {
        // A probe never calls the program.
        r->dynamic = true;
        return mlocExpressionFallback(r, expression);
    }
    const mlocMessageFunction* function = &r->formatter.functions[slot - messageFunctionProgram];
    mlocMessageOperand given;
    mlocMessageResolved storage;
    if (operand != NULL)
    {
        ToOperand(operand, &given, &storage);
    }
    else
    {
        memset(&given, 0, sizeof given);
    }
    CallState state = {r, expression, callStamp};
    mlocMessageCall call;
    memset(&call, 0, sizeof call);
    memcpy(&call, &state, sizeof state);
    Value v = Make(valueResolved);
    v.function = slot - messageFunctionProgram;
    mlocMessageFaultKind fault = function->resolve(function->context, &call, &given, &v.resolved);
    if (fault != mloc_messageFaultNone)
    {
        mlocReportFault(r, fault, Field(r, messageExpressions, expression, 7));
        return mlocExpressionFallback(r, expression);
    }
    if (function->direction != NULL)
    {
        mlocDirection direction = function->direction(function->context, &v.resolved);
        v.direction =
            (uint32_t)direction <= mloc_directionRightToLeft ? direction : mloc_directionUnknown;
    }
    return v;
}

// A whole number an option's value holds, for a digit size: an integer
// argument, or a double, decimal or number value that is whole.
static bool WholeValue(const Value* v, int64_t* out)
{
    mlocDecimal decimal;
    if (v->kind == valueNumber)
    {
        decimal = v->number.decimal;
    }
    else if (v->kind == valueArgument && v->argument->kind == mloc_messageValueInteger)
    {
        *out = v->argument->as.integer;
        return true;
    }
    else if (v->kind == valueArgument && v->argument->kind == mloc_messageValueDouble)
    {
        double d = v->argument->as.number;
        bool whole = d >= -1e15 && d <= 1e15 && d == (double)(int64_t)d;
        *out = whole ? (int64_t)d : 0;
        return whole;
    }
    else if (v->kind == valueArgument && v->argument->kind == mloc_messageValueDecimal)
    {
        decimal = *v->argument->as.decimal;
    }
    else
    {
        return false;
    }
    // A small whole decimal: no fraction shown, few digits.
    if (decimal.kind != mloc_decimalFinite || decimal.fractionDigits != 0 ||
        decimal.exponent + decimal.count > 15)
    {
        return false;
    }
    int64_t value = 0;
    for (int64_t power = decimal.exponent + decimal.count - 1; power >= 0; power--)
    {
        int64_t index = decimal.exponent + decimal.count - 1 - power;
        value = value * 10 + (index < decimal.count ? decimal.digits[index] : 0);
    }
    *out = decimal.negative ? -value : value;
    return true;
}

// An option's value as a number function reads it.
static NumberOption NumberOptionAt(Resolution* r, uint32_t p, uint32_t at, bool* failedOut)
{
    NumberOption o;
    memset(&o, 0, sizeof o);
    o.literal = Field(r, messageParameters, p, 0) == parameterOptionLiteral;
    Value v = OptionValue(r, p, at);
    *failedOut = IsFallback(v.kind);
    bool textual = v.kind == valueText || (v.kind == valueString && v.behind == valueText);
    if (textual)
    {
        o.hasText = true;
        o.text = v.text;
        o.length = v.length;
    }
    else
    {
        o.hasInteger = WholeValue(&v, &o.integer);
    }
    return o;
}

// The number a number function's operand gives: a number literal (or a
// string argument that is one), a numeric argument, or a number value,
// whose options pass on. mloc_messageFaultBadOperand for anything else.
static mlocMessageFaultKind NumberOperand(Resolution* r, NumberFunction function,
                                          const Value* operand, uint32_t at, NumberValue* out)
{
    if (operand == NULL)
    {
        return mloc_messageFaultBadOperand;
    }
    ValueKind kind = operand->kind == valueString ? operand->behind : operand->kind;
    mlocDecimal decimal;
    if (kind == valueNumber)
    {
        mlocMessageFaultKind fault = mlocNumberFromValue(function, &operand->number, out);
        if (fault != mloc_messageFaultNone)
        {
            mlocReportFault(r, fault, at);
        }
        return mloc_messageFaultNone;
    }
    if (kind == valueText)
    {
        if (!mlocNumberLiteral(operand->text, operand->length))
        {
            return mloc_messageFaultBadOperand;
        }
        if (mlocParseDecimal((const char*)operand->text, operand->length, &decimal).status !=
            mloc_success)
        {
            // A literal past the decimal's 40 digits or its exponent.
            return mloc_messageFaultUnsupportedOperation;
        }
    }
    else if (kind != valueArgument || !mlocArgumentDecimal(operand->argument, &decimal))
    {
        return mloc_messageFaultBadOperand;
    }
    // :offset over a plain number formats as :number does, over a
    // currency amount as :currency does; the number functions read an
    // amount's number alone.
    bool amount = kind == valueArgument && operand->argument->kind == mloc_messageValueCurrency;
    NumberFunction made = function;
    if (function == numberFunctionOffset)
    {
        made = amount ? numberFunctionCurrency : numberFunctionNumber;
    }
    *out = mlocMakeNumberValue(made, &decimal);
    if (amount && made == numberFunctionCurrency)
    {
        mlocSetCurrencyCode(out, operand->argument->as.currency.code);
    }
    return mloc_messageFaultNone;
}

// An :offset value: its operand's number, function and options, plus or
// minus its one add or subtract option (a digit size). None of them,
// both, or a bad value is a bad option and the expression falls back.
MLOC_NOINLINE static Value ResolveOffset(Resolution* r, uint32_t expression, const Value* operand)
{
    uint32_t at = Field(r, messageExpressions, expression, 7);
    NumberValue n;
    mlocMessageFaultKind fault = NumberOperand(r, numberFunctionOffset, operand, at, &n);
    if (fault != mloc_messageFaultNone)
    {
        mlocReportFault(r, fault, at);
        return mlocExpressionFallback(r, expression);
    }
    uint32_t first = Field(r, messageExpressions, expression, 5);
    uint32_t count = Field(r, messageExpressions, expression, 6);
    uint32_t found = 0;
    int32_t delta = 0;
    bool reported = false;
    bool bad = false;
    for (uint32_t p = first; p < first + count; p++)
    {
        uint32_t kind = Field(r, messageParameters, p, 0);
        const uint8_t* name = Pool(r, Field(r, messageParameters, p, 1));
        size_t length = Field(r, messageParameters, p, 2);
        bool add = length == 3 && memcmp(name, "add", 3) == 0;
        if ((kind != parameterOptionLiteral && kind != parameterOptionVariable) ||
            (!add && !(length == 8 && memcmp(name, "subtract", 8) == 0)))
        {
            continue;
        }
        found++;
        bool failed = false;
        NumberOption option = NumberOptionAt(r, p, at, &failed);
        int digits = failed ? -1 : mlocDigitSize(&option);
        // A failed variable was reported as a bad option already.
        reported = reported || failed;
        bad = bad || digits < 0;
        delta = add ? digits : -digits;
    }
    if (found != 1 || bad)
    {
        if (!reported)
        {
            mlocReportFault(r, mloc_messageFaultBadOption, at);
        }
        return mlocExpressionFallback(r, expression);
    }
    if (!mlocOffsetNumber(&n, delta))
    {
        // A sum past a decimal's digits, which UTS #35 lets refuse.
        mlocReportFault(r, mloc_messageFaultUnsupportedOperation, at);
        return mlocExpressionFallback(r, expression);
    }
    Value v = Make(valueNumber);
    v.number = n;
    v.number.origin = expression;
    v.direction = r->formatter.direction;
    return v;
}

// A :number, :integer, :percent or :currency value: its operand's number, the options passed
// on, then the expression's own (u: options aside), checked together.
MLOC_NOINLINE static Value ResolveNumber(Resolution* r, uint32_t expression,
                                         NumberFunction function, const Value* operand)
{
    uint32_t at = Field(r, messageExpressions, expression, 7);
    NumberValue n;
    mlocMessageFaultKind fault = NumberOperand(r, function, operand, at, &n);
    if (fault == mloc_messageFaultNone && function == numberFunctionInteger && !mlocMakeInteger(&n))
    {
        fault = mloc_messageFaultUnsupportedOperation;
    }
    if (fault != mloc_messageFaultNone)
    {
        mlocReportFault(r, fault, at);
        return mlocExpressionFallback(r, expression);
    }
    uint32_t first = Field(r, messageExpressions, expression, 5);
    uint32_t count = Field(r, messageExpressions, expression, 6);
    for (uint32_t p = first; p < first + count; p++)
    {
        uint32_t kind = Field(r, messageParameters, p, 0);
        if ((kind != parameterOptionLiteral && kind != parameterOptionVariable) ||
            UnicodeOption(r, p))
        {
            continue;
        }
        bool failed = false;
        NumberOption option = NumberOptionAt(r, p, at, &failed);
        const uint8_t* name = Pool(r, Field(r, messageParameters, p, 1));
        size_t length = Field(r, messageParameters, p, 2);
        if (failed)
        {
            // Reported in option resolution; a select still counts as
            // set from a variable.
            n.selectable = n.selectable && !(length == 6 && memcmp(name, "select", 6) == 0);
            continue;
        }
        fault = mlocSetNumberOption(&n, function, name, length, &option);
        if (fault != mloc_messageFaultNone)
        {
            mlocReportFault(r, fault, at);
        }
    }
    if (n.function == numberFunctionCurrency)
    {
        // A number without a currency is no currency's amount.
        fault = (n.given & numberGivenCurrency) == 0 ? mloc_messageFaultBadOperand
                : !mlocFindCurrencyDigits(&n, r->formatter.data)
                    ? mloc_messageFaultUnsupportedOperation
                    : mloc_messageFaultNone;
        if (fault != mloc_messageFaultNone)
        {
            mlocReportFault(r, fault, at);
            return mlocExpressionFallback(r, expression);
        }
    }
    for (uint32_t i = mlocCheckNumberOptions(&n); i > 0; i--)
    {
        mlocReportFault(r, mloc_messageFaultBadOption, at);
    }
    Value v = Make(valueNumber);
    v.number = n;
    v.number.origin = expression;
    // A number reads in its locale's direction.
    v.direction = r->formatter.direction;
    return v;
}

// An expression's function applied to its resolved operand (null for
// none), into out: off the recursion's frames, as only options recurse
// from here.
MLOC_NOINLINE static void Apply(Resolution* r, uint32_t expression, uint32_t slot,
                                const Value* given, Value* out)
{
    uint32_t at = Field(r, messageExpressions, expression, 7);
    if (slot == messageFunctionUnknown)
    {
        mlocReportFault(r, mloc_messageFaultUnknownFunction, at);
        *out = mlocExpressionFallback(r, expression);
        return;
    }
    CheckParameters(r, Field(r, messageExpressions, expression, 5),
                    Field(r, messageExpressions, expression, 6), at);
    Directed d = UnicodeOptions(r, expression);
    Value v;
    if (slot == messageFunctionString)
    {
        v = StringOf(r, expression, given);
    }
    else if (slot == messageFunctionOffset)
    {
        v = ResolveOffset(r, expression, given);
    }
    else if (slot == messageFunctionNumber || slot == messageFunctionInteger ||
             slot == messageFunctionPercent || slot == messageFunctionCurrency)
    {
        static const NumberFunction functions[] = {
            [messageFunctionNumber] = numberFunctionNumber,
            [messageFunctionInteger] = numberFunctionInteger,
            [messageFunctionPercent] = numberFunctionPercent,
            [messageFunctionCurrency] = numberFunctionCurrency,
        };
        v = ResolveNumber(r, expression, functions[slot], given);
    }
    else
    {
        v = CallProgram(r, expression, slot, given);
    }
    if (d.set && !IsFallback(v.kind))
    {
        // u:dir replaces the direction (inherit keeps it) and isolates.
        v.direction = d.isolate ? d.direction : v.direction;
        v.isolate = d.isolate;
    }
    *out = v;
}

// Past the resolution limit (functions reading options again and
// again): reported once, even while quiet.
MLOC_NOINLINE static void ExhaustedInto(Resolution* r, uint32_t expression, Value* out)
{
    if (!r->exhausted)
    {
        r->exhausted = true;
        uint32_t quiet = r->quiet;
        r->quiet = 0;
        mlocReportFault(r, mloc_messageFaultUnsupportedOperation,
                        Field(r, messageExpressions, expression, 7));
        r->quiet = quiet;
    }
    *out = mlocExpressionFallback(r, expression);
}

// An expression's value into out. A chain of declarations recurses
// through here and VariableInto, so each level holds its operand alone.
static void ResolveInto(Resolution* r, uint32_t expression, Value* out)
{
    if (r->budget == 0)
    {
        ExhaustedInto(r, expression, out);
        return;
    }
    r->budget--;
    uint32_t operandKind = Field(r, messageExpressions, expression, 0);
    uint32_t slot = SlotOf(r, expression);
    // With no function the operand is the value, resolved into out.
    Value operand;
    Value* into = slot == messageFunctionNone ? out : &operand;
    if (operandKind == operandVariable)
    {
        VariableInto(r, Field(r, messageExpressions, expression, 1),
                     Field(r, messageExpressions, expression, 7), expression, into);
    }
    else
    {
        SetKind(into, valueText);
        if (operandKind == operandLiteral)
        {
            into->text = Pool(r, Field(r, messageExpressions, expression, 1));
            into->length = Field(r, messageExpressions, expression, 2);
        }
    }
    if (slot != messageFunctionNone)
    {
        Apply(r, expression, slot, operandKind == operandNone ? NULL : &operand, out);
    }
}

Value mlocResolveExpression(Resolution* r, uint32_t expression)
{
    Value v;
    ResolveInto(r, expression, &v);
    return v;
}

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
    uint32_t first = Field(r, messageExpressions, s->expression, 5);
    uint32_t count = Field(r, messageExpressions, s->expression, 6);
    size_t seen = 0;
    for (uint32_t p = first; p < first + count; p++)
    {
        uint32_t kind = Field(r, messageParameters, p, 0);
        if ((kind != parameterOptionLiteral && kind != parameterOptionVariable) ||
            UnicodeOption(r, p))
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
    Value v = OptionValue(s->r, p, Field(s->r, messageExpressions, s->expression, 7));
    ToOperand(&v, valueOut, storage);
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
    *nameOut = (const char*)Pool(s.r, Field(s.r, messageParameters, p, 1));
    *lengthOut = Field(s.r, messageParameters, p, 2);
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
        if (Field(s.r, messageParameters, p, 2) == length &&
            memcmp(Pool(s.r, Field(s.r, messageParameters, p, 1)), name, length) == 0)
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
