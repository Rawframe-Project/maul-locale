// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Values as functions take them (record mloc-0045): a value as a program
// function's operand, the default functions' operands read from values,
// and the setters through which resolution passes their options.

#ifndef MLOC_SRC_MESSAGE_OPERAND_H
#define MLOC_SRC_MESSAGE_OPERAND_H

#include "message_date.h"
#include "message_duration.h"
#include "message_number.h"
#include "message_value.h"

#include "maul-locale/message.h"

// Sets an option by its name and value, or notes one that failed to
// resolve (null), returning the fault to report.
typedef mlocMessageFaultKind (*OptionSetter)(void* context, const uint8_t* name, size_t length,
                                             const NumberOption* option);

// A number value and its function, for SetNumber.
typedef struct NumberSetting
{
    NumberValue* value;
    NumberFunction function;
} NumberSetting;

// A date value, its function and the data, for SetDate.
typedef struct DateSetting
{
    DateValue* value;
    DateFunction function;
    const mlocDataSet* data;
} DateSetting;

// A value as a function receives it; storage holds a resolved value.
void mlocToOperand(const Value* v, mlocMessageOperand* out, mlocMessageResolved* storage);

// The setters of a number value (a NumberSetting), a date value (a
// DateSetting), a duration value and a list value (the values), for
// resolution's option loop.
mlocMessageFaultKind mlocSetNumberFrom(void* context, const uint8_t* name, size_t length,
                                       const NumberOption* option);
mlocMessageFaultKind mlocSetDateFrom(void* context, const uint8_t* name, size_t length,
                                     const NumberOption* option);
mlocMessageFaultKind mlocSetDurationFrom(void* context, const uint8_t* name, size_t length,
                                         const NumberOption* option);
mlocMessageFaultKind mlocSetListFrom(void* context, const uint8_t* name, size_t length,
                                     const NumberOption* option);

// The list a mloc:list operand gives: a list argument, or another
// mloc:list value, whose options pass on; false for anything else.
bool mlocListOperand(const Value* operand, ListValue* out);

// The date value a date function's operand gives: a date/time literal
// (or a string argument that is one), a date, time, date-time, instant
// or zoned argument, or another date function's value, whose override
// options pass on; false for anything else or parts the function cannot
// take.
bool mlocDateOperand(const mlocDataSet* data, DateFunction function, const Value* operand,
                     DateValue* out);

// The duration a mloc:duration operand gives: a duration literal (or a
// string argument that is one), a duration argument, a number of
// seconds, or another mloc:duration value, whose options pass on; false
// for anything else.
bool mlocDurationOperand(const Value* operand, DurationValue* out);

#endif
