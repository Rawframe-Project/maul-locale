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
#include "message_value.h"

#include "maul-locale/message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
    // Set while binding probes an expression: an argument stands for a
    // plain number, and dynamic is set when an option's value comes from
    // an argument or a program function is reached (the value then
    // changes from call to call). optionDepth counts the options being
    // resolved.
    bool probe;
    bool dynamic;
    uint32_t optionDepth;
    // What an argument stands for in a probe: a plain number for a
    // number function's expression, a date-time for a date function's.
    const mlocMessageValue* probeArgument;
} Resolution;

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

// A string in the message's pool.
static inline const uint8_t* mlocResolutionPool(const Resolution* r, uint32_t offset)
{
    return r->h.view.pool + offset;
}

// Whether a parameter is a u: option, hidden from functions.
bool mlocUnicodeOption(const Resolution* r, uint32_t p);

// An option's value, its variable resolved quietly (its faults are
// reported once, when the expression's options are checked).
Value mlocOptionValue(Resolution* r, uint32_t p, uint32_t at);

// A record's field.
static inline uint32_t mlocResolutionField(const Resolution* r, uint32_t kind, uint32_t index,
                                           uint32_t field)
{
    return mlocMessageField(&r->h.view, kind, index, field);
}

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

#endif
