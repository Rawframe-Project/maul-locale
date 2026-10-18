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

#include "maul-locale/currency.h"
#include "maul-locale/message.h"
#include "maul-locale/number.h"
#include "maul-locale/plural.h"
#include "maul-locale/relative.h"
#include "maul-locale/spellout.h"
#include "maul-locale/unit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Which number function made a value.
typedef enum NumberFunction
{
    numberFunctionNumber = 0,
    numberFunctionInteger = 1,
    numberFunctionPercent = 2,
    // Not a value's function: :offset keeps its operand's.
    numberFunctionOffset = 3,
    numberFunctionCurrency = 4,
    numberFunctionUnit = 5,
    numberFunctionSpellout = 6,
    numberFunctionRelative = 7,
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
    numberGivenCurrency = 1u << 12,
    numberGivenDisplay = 1u << 13,
    numberGivenCurrencySign = 1u << 14,
    numberGivenFractionDigits = 1u << 15,
    numberGivenUnit = 1u << 16,
    numberGivenUsage = 1u << 17,
    numberGivenWidth = 1u << 18,
    numberGivenRules = 1u << 19,
    numberGivenSystem = 1u << 20,
    numberGivenRelativeUnit = 1u << 21,
    numberGivenStyle = 1u << 22,
    numberGivenNumeric = 1u << 23,
};

// The options that choose a mloc:spellout or mloc:relative formatter:
// literals only.
static const uint32_t numberGivenWords = numberGivenRules | numberGivenSystem |
                                         numberGivenRelativeUnit | numberGivenStyle |
                                         numberGivenNumeric;

// A number value: its decimal, the expression that made it, and the
// options given to it, each read only where its bit is set in given.
// Without padding, so two values' options compare by their bytes.
typedef struct NumberValue
{
    mlocDecimal decimal;
    uint32_t origin;
    // A :unit value's unit and usage, as text in the compiled message or
    // an argument, which outlive the call; a mloc:spellout value's rule
    // set (without its "%") and numbering system.
    const char* unit;
    const char* usage;
    uint32_t unitLength;
    uint32_t usageLength;
    uint32_t given;
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
    // A currency value's: its code (upper case), mlocCurrencyDisplay,
    // mlocCurrencySign, its fractionDigits option, and the currency's
    // own digits, found when the value is made.
    char code[3];
    uint8_t display;
    uint8_t currencySign;
    uint8_t fractionDigits;
    uint8_t currencyDigits;
    // A :unit value's mlocUnitWidth, a mloc:relative value's
    // mlocRelativeWidth.
    uint8_t width;
    // A mloc:relative value's mlocRelativeUnit and mlocRelativeNumeric.
    uint8_t relativeUnit;
    uint8_t numeric;
    // Zero, so values compare by their bytes.
    uint8_t reserved[3];
} NumberValue;

// Which formatter a prepared value has.
typedef enum PreparedKind
{
    preparedNumber = 0,
    preparedCurrency = 1,
    preparedUnit = 2,
    preparedSpellout = 3,
    preparedRelative = 4,
} PreparedKind;

// What binding prepares for a number function's expression (record
// mloc-0045): the options of the value it makes (number, origin and
// selectability cleared), the plural rules its selection reads (rulesType
// 1 plus the mlocPluralType, or 0 for none), and its formatter, of the
// PreparedKind kind. A bound message holds the bytes up to the end of the
// formatter it has.
typedef struct PreparedNumber
{
    NumberValue key;
    mlocPluralRules rules;
    uint8_t rulesType;
    uint8_t kind;
    uint8_t reserved[6];
    union
    {
        mlocNumberFormatter number;
        mlocCurrencyFormatter currency;
        mlocUnitFormatter unit;
        mlocSpelloutFormatter spellout;
        mlocRelativeFormatter relative;
    } formatter;
} PreparedNumber;

// The bytes a prepared value of a kind takes in a bound message.
size_t mlocPreparedSize(PreparedKind kind);

// A value's options as a prepared value keeps them (its number, origin
// and selectability cleared), and whether a prepared value, which may be
// null, was made with a value's options.
NumberValue mlocNumberKey(const NumberValue* value);
bool mlocNumberFits(const PreparedNumber* prepared, const NumberValue* value);

// Prepares a value as its expression makes it; false when its formatter
// cannot be made (the expression is then not prepared).
bool mlocPrepareNumber(const NumberValue* value, const mlocDataSet* data, const mlocLocale* locale,
                       PreparedNumber* out);

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

// Whether a decimal (as it formats) is written plainly as a number
// literal key: no exponent, zero unsigned, the fraction zeros it shows.
bool mlocExactlyKey(const mlocDecimal* rounded, const uint8_t* key, size_t length);

// A number value of a function for a decimal, no option given.
NumberValue mlocMakeNumberValue(NumberFunction function, const mlocDecimal* decimal);

// Starts a value from an operand that is a number value: its decimal
// (for :integer, rounded to an integer), and the options the function
// passes on (:offset all, its function kept; :percent drops minimum
// integer digits, increment and select; :currency select, as it does
// not select). Returns mloc_messageFaultBadOption when the operand's
// select may not pass on (the value then cannot select), else none.
mlocMessageFaultKind mlocNumberFromValue(NumberFunction function, const NumberValue* operand,
                                         NumberValue* out);

// Writes a currency code of three ASCII letters in upper case.
void mlocUpperCode(const char* code, char* out);

// Gives a value its operand's currency (a currency amount's code).
void mlocSetCurrencyCode(NumberValue* value, const char* code);

// Whether text is lexically a unit identifier or a usage: lower-case
// ASCII letters and digits in parts joined by single hyphens.
bool mlocUnitText(const char* text, size_t length);

// Gives a value its operand's unit (a unit amount's identifier).
void mlocSetUnit(NumberValue* value, const char* unit, size_t length);

// Finds a currency value's own digits once its code is known: false for
// data without the currencies' fractions.
bool mlocFindCurrencyDigits(NumberValue* value, const mlocDataSet* data);

// Rounds a fresh :integer value's decimal to an integer (half-expand).
// Returns false when rounding passes the decimal's limits.
bool mlocMakeInteger(NumberValue* value);

// Sets an option the expression gives by name, for the function whose
// expression it is: mloc_messageFaultNone, also for a name the function
// does not take (ignored);
// mloc_messageFaultBadOption for a value that is not one of the option's
// (ignored), a select that is not a literal (the value then cannot
// select), or a currency over the operand's.
mlocMessageFaultKind mlocSetNumberOption(NumberValue* value, NumberFunction function,
                                         const uint8_t* name, size_t length,
                                         const NumberOption* option);

// A digit size option's value, 0 to 99, or -1.
int mlocDigitSize(const NumberOption* option);

// Checks the options together as ECMA-402's digit options do: a maximum
// under its minimum, or an increment without equal fraction limits, is
// a bad option and dropped. Returns the count of faults.
uint32_t mlocCheckNumberOptions(NumberValue* value);

// The number formatter's options for a checked value.
mlocNumberOptions mlocNumberFormatOptions(const NumberValue* value);

// Writes a value in a locale, with the formatter prepared for it when
// prepared (which may be null) has its options, else one made here;
// mloc_messageFaultUnsupportedOperation when the locale's numbers cannot
// be made, else none (the output saying what it needs, as mlocOutput
// does).
mlocMessageFaultKind mlocWriteNumberValue(const NumberValue* value, const PreparedNumber* prepared,
                                          const mlocDataSet* data, const mlocLocale* locale,
                                          mlocOutput* output);

// Whether a key matches a value: a number literal its exact
// serialization, a plural keyword its category (not in exact mode, with
// the prepared rules when prepared, which may be null, has the value's
// options). mloc_messageFaultBadVariantKey for another key (no match).
mlocMessageFaultKind mlocMatchNumber(const NumberValue* value, const PreparedNumber* prepared,
                                     const mlocDataSet* data, const mlocLocale* locale,
                                     const uint8_t* key, size_t length, bool* matchOut);

// Whether key1 is a better match than key2, both matching: a number
// literal over a keyword.
bool mlocNumberKeyBetter(const uint8_t* key1, size_t length1, const uint8_t* key2, size_t length2);

#endif
