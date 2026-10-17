// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The number functions of messages (record mloc-0045; UTS #35 Part 9,
// "Numeric Value Selection and Formatting"): a number value, its options
// read and passed on as each function says, its precision by ECMA-402's
// digit options, its text by the library's number formatter, and its
// matching of keys by exact value and plural category.

#ifndef MLOC_SRC_MESSAGE_NUMBER_H
#define MLOC_SRC_MESSAGE_NUMBER_H

#include "maul-locale/message.h"
#include "maul-locale/number.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Which number function made a value.
typedef enum NumberFunction
{
    numberFunctionNumber = 0,
    numberFunctionInteger = 1,
} NumberFunction;

// How a number value selects.
typedef enum NumberSelect
{
    numberSelectPlural = 0,
    numberSelectOrdinal = 1,
    numberSelectExact = 2,
} NumberSelect;

// An option a number value was given, as a bit of its given mask.
enum
{
    numberGivenSelect = 1u << 0,
    numberGivenSign = 1u << 1,
    numberGivenGrouping = 1u << 2,
    numberGivenMinInteger = 1u << 3,
    numberGivenMinFraction = 1u << 4,
    numberGivenMaxFraction = 1u << 5,
    numberGivenMinSignificant = 1u << 6,
    numberGivenMaxSignificant = 1u << 7,
    numberGivenTrailingZeros = 1u << 8,
    numberGivenPriority = 1u << 9,
    numberGivenIncrement = 1u << 10,
    numberGivenMode = 1u << 11,
};

// A number value: its decimal and the options given to it, each read
// only where its bit is set in given.
typedef struct NumberValue
{
    mlocDecimal decimal;
    uint16_t given;
    uint16_t increment;
    uint8_t function;
    uint8_t select;
    // False when select came from a variable or an operand: the value
    // then formats but cannot select.
    bool selectable;
    uint8_t sign;
    uint8_t grouping;
    uint8_t minInteger;
    uint8_t minFraction;
    uint8_t maxFraction;
    uint8_t minSignificant;
    uint8_t maxSignificant;
    uint8_t trailingZeros;
    uint8_t priority;
    uint8_t mode;
} NumberValue;

// An option's value as a number function reads it: text (a literal's,
// a string argument's), or an integer (a numeric argument's or a number
// value's, when whole); literal for a literal in the message.
typedef struct NumberOption
{
    const uint8_t* text;
    size_t length;
    bool hasText;
    bool hasInteger;
    int64_t integer;
    bool literal;
} NumberOption;

// Whether text matches MF2's number-literal production.
bool mlocNumberLiteral(const uint8_t* text, size_t length);

// A number value of a function for a decimal, no option given.
NumberValue mlocMakeNumberValue(NumberFunction function, const mlocDecimal* decimal);

// Starts a value from an operand that is a number value: its decimal
// (for :integer, rounded to an integer), and the options the function
// passes on. Returns mloc_messageFaultBadOption when the operand's
// select may not pass on (the value then cannot select), else none.
mlocMessageFaultKind mlocNumberFromValue(NumberFunction function, const NumberValue* operand,
                                         NumberValue* out);

// Rounds a fresh :integer value's decimal to an integer (half-expand).
// Returns false when rounding passes the decimal's limits.
bool mlocMakeInteger(NumberValue* value);

// Sets an option the expression gives by name: mloc_messageFaultNone,
// also for a name the function does not know (ignored);
// mloc_messageFaultBadOption for a value that is not one of the option's
// (ignored), or a select that is not a literal (the value then cannot
// select).
mlocMessageFaultKind mlocSetNumberOption(NumberValue* value, const uint8_t* name, size_t length,
                                         const NumberOption* option);

// Checks the options together as ECMA-402's digit options do: a maximum
// under its minimum, or an increment without equal fraction limits, is
// a bad option and dropped. Returns the count of faults.
uint32_t mlocCheckNumberOptions(NumberValue* value);

// The number formatter's options for a checked value.
mlocNumberOptions mlocNumberFormatOptions(const NumberValue* value);

// Writes a value in a locale; mloc_messageFaultUnsupportedOperation when
// the locale's numbers cannot be made, else none (the output saying
// what it needs, as mlocOutput does).
mlocMessageFaultKind mlocWriteNumberValue(const NumberValue* value, const mlocDataSet* data,
                                          const mlocLocale* locale, mlocOutput* output);

// Whether a key matches a value: a number literal its exact
// serialization, a plural keyword its category (not in exact mode).
// mloc_messageFaultBadVariantKey for another key (no match).
mlocMessageFaultKind mlocMatchNumber(const NumberValue* value, const mlocDataSet* data,
                                     const mlocLocale* locale, const uint8_t* key, size_t length,
                                     bool* matchOut);

// Whether key1 is a better match than key2, both matching: a number
// literal over a keyword.
bool mlocNumberKeyBetter(const uint8_t* key1, size_t length1, const uint8_t* key2, size_t length2);

#endif
