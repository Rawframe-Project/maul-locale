// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A message's resolved values (record mloc-0045) and what reads them
// without the resolver: an argument's name compared after NFC, a numeric
// argument's decimal and plain digits, a :string value against a key.

#ifndef MLOC_SRC_MESSAGE_VALUE_H
#define MLOC_SRC_MESSAGE_VALUE_H

#include "message_date.h"
#include "message_duration.h"
#include "message_grammar.h"
#include "message_list.h"
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
    // A date and time function's value.
    valueDate,
    // A mloc:duration value.
    valueDuration,
    // A mloc:list value.
    valueList,
    // A mloc:case or mloc:particle value.
    valueGrammar,
    // A mloc:gender value.
    valueGender,
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
        DateValue date;
        DurationValue duration;
        ListValue list;
        GrammarValue grammar;
        GenderValue gender;
    };
} Value;

// Whether an argument's name is a variable's (NFC, from the compiled
// message).
bool mlocSameName(const mlocMessageArgument* a, const uint8_t* name, size_t length);

// A numeric argument's decimal (a currency amount's amount); false for
// another kind or a number past a decimal's limits.
bool mlocArgumentDecimal(const mlocMessageValue* argument, mlocDecimal* out);

// An argument written with no function as its plain decimal (a currency
// amount's followed by a space and its code; a unit amount's digits
// alone, its unit written after them and a space by the caller), or a
// date, time or date-time in ISO 8601; false for a kind that has none
// (an object).
bool mlocArgumentDigits(const mlocMessageValue* argument, char* digits, size_t capacity,
                        size_t* lengthOut);

// Whether a :string value matches a key (in NFC); false with a fault
// kind in *faultOut when its text cannot be compared.
bool mlocStringMatches(const Value* value, const uint8_t* key, size_t length,
                       mlocMessageFaultKind* faultOut);

#endif
