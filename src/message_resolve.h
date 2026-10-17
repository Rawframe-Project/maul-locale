// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Resolving a bound message's expressions and variables (UTS #35 Part 9,
// "Expression and Markup Resolution"), each where it stands: a variable
// through its declaration on the stack, a function through its slot
// (:string here, a program function through its entries), with the
// fallbacks and faults of the specification, and each value's direction
// with the u: options. Also the accessors a program function reads its
// call's options with, which hide the u: options.

#ifndef MLOC_SRC_MESSAGE_RESOLVE_H
#define MLOC_SRC_MESSAGE_RESOLVE_H

#include "message_bind.h"
#include "message_number.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A resolved value's kind.
typedef enum ValueKind
{
    // Text with no function: a literal's or a string argument's.
    valueText,
    // An argument of another kind, with no function.
    valueArgument,
    // A :string value; what stands behind it is in behind.
    valueString,
    // A program function's value.
    valueResolved,
    // A number function's value.
    valueNumber,
    // Fallbacks, showing "$name", "|literal|" or ":function".
    valueFallbackVariable,
    valueFallbackLiteral,
    valueFallbackFunction,
} ValueKind;

typedef struct Value
{
    ValueKind kind;
    // For a :string value: valueText, valueArgument or a fallback kind,
    // the fields below then describing that.
    ValueKind behind;
    // Text, a fallback's literal, or a fallback function's identifier.
    const uint8_t* text;
    size_t length;
    const mlocMessageValue* argument;
    // A fallback variable; a program function's index.
    uint32_t variable;
    uint32_t function;
    // Its direction (unknown for a literal, an argument, :string and a
    // fallback), and whether a u:dir other than inherit isolates it.
    mlocDirection direction;
    bool isolate;
    union
    {
        mlocMessageResolved resolved;
        NumberValue number;
    };
} Value;

// The state of one format call that resolution reads and reports to.
typedef struct Resolution
{
    BoundHeader h;
    MessageFormatter formatter;
    const uint8_t* words;
    const mlocMessageArgument* arguments;
    size_t argumentCount;
    mlocMessageFaults* faults;
    size_t faultCount;
    // While above 0, faults are not reported (an option read again).
    uint32_t quiet;
    // The resolutions left (messageResolutionLimit at the start), and
    // whether running out has been reported.
    uint32_t budget;
    bool exhausted;
    // A bit per declaration whose faults were reported.
    uint8_t reported[messageDeclarationLimit / 8];
} Resolution;

// A record's field.
uint32_t mlocResolutionField(const Resolution* r, uint32_t kind, uint32_t index, uint32_t field);

// Reports a fault at a source offset.
void mlocReportFault(Resolution* r, mlocMessageFaultKind kind, uint32_t at);

// A variable's value; within is the expression being resolved, whose
// operand, when it is the variable of the .input declaring it, is the
// argument (UINT32_MAX for none).
Value mlocResolveVariable(Resolution* r, uint32_t variable, uint32_t at, uint32_t within);

// An expression's value.
Value mlocResolveExpression(Resolution* r, uint32_t expression);

// Resolves a markup's options for their faults (record mloc-0045:
// markup has no value, but option resolution reports a failed option,
// and u:dir on markup is a bad option).
void mlocResolveMarkup(Resolution* r, uint32_t markup);

// The fallback an expression shows when its value cannot be written.
Value mlocExpressionFallback(const Resolution* r, uint32_t expression);

// Whether a :string value matches a key (in NFC); false with a fault
// kind in *faultOut when its text cannot be compared.
bool mlocStringMatches(const Value* value, const uint8_t* key, size_t length,
                       mlocMessageFaultKind* faultOut);

// An argument written with no function as its plain decimal; false for
// a kind that has none (an object).
bool mlocArgumentDigits(const mlocMessageValue* argument, char* digits, size_t capacity,
                        size_t* lengthOut);

#endif
