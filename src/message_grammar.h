// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloc:case and mloc:particle (record mloc-0052): a value over a string
// or a number value, inflected by the locale's inflector (records
// mloc-0047 to mloc-0050) with the request the expression's options
// make; mloc:gender, a selector on the gender of a list of persons
// (record mloc-0051).

#ifndef MLOC_SRC_MESSAGE_GRAMMAR_H
#define MLOC_SRC_MESSAGE_GRAMMAR_H

#include "message_number.h"

#include "maul-locale/grammar.h"
#include "maul-locale/list.h"
#include "maul-locale/message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Which function made a grammar value.
enum
{
    grammarCase = 0,
    grammarParticle = 1,
};

// A grammar value: its operand (a text, or a number value, which keeps
// its own formatter's options), the request its options make, and the
// expression that made it. The request's hint points into the message
// or an argument, which outlive the call.
typedef struct GrammarValue
{
    uint32_t origin;
    uint8_t function;
    bool isNumber;
    uint8_t reserved[2];
    mlocInflection request;
    const uint8_t* text;
    size_t length;
    NumberValue number;
} GrammarValue;

// What binding prepares for a grammar expression: the locale's
// inflector (made set when it could be made).
typedef struct PreparedGrammar
{
    uint32_t made;
    uint32_t reserved;
    mlocInflector inflector;
} PreparedGrammar;

// A mloc:gender value: the gender of its list of persons in the locale.
typedef struct GenderValue
{
    uint32_t origin;
    uint8_t gender;
    uint8_t reserved[3];
} GenderValue;

// Reads a list of persons' genders ("male", "female", "other" items, or
// one such text when items is null) and the gender of the list in a
// locale; false for another item or a list the call refuses.
bool mlocMakeGenderValue(const mlocListItem* items, size_t count, const uint8_t* text,
                         size_t length, const mlocDataSet* data, const mlocLocale* locale,
                         GenderValue* out);

// Whether a key matches a gender value: male, female or other as the
// list's gender; mloc_messageFaultBadVariantKey for another key.
mlocMessageFaultKind mlocMatchGender(const GenderValue* value, const uint8_t* key, size_t length,
                                     bool* matchOut);

// A grammar value of a function over a text.
GrammarValue mlocMakeGrammarText(uint8_t function, const uint8_t* text, size_t length);

// A grammar value of a function over a number value.
GrammarValue mlocMakeGrammarNumber(uint8_t function, const NumberValue* number);

// Sets an option the expression gives by name: none, also for a name the
// function does not take (ignored); mloc_messageFaultBadOption for a
// value the option does not take, or a variable's where the option
// chooses the rules (all but hint, which may be a variable's text).
mlocMessageFaultKind mlocSetGrammarOption(GrammarValue* value, const uint8_t* name, size_t length,
                                          const NumberOption* option);

// Sets an option as mlocSetGrammarOption does, for the resolver's option
// walk (context a GrammarValue; a null option is none).
mlocMessageFaultKind mlocSetGrammarFrom(void* context, const uint8_t* name, size_t length,
                                        const NumberOption* option);

// Prepares the locale's inflector; false when the locale has no rules.
bool mlocPrepareGrammar(const mlocDataSet* data, const mlocLocale* locale, PreparedGrammar* out);

// Writes a value inflected, with the inflector prepared when prepared
// (which may be null) has one, else one made here, and the number's own
// formatter for a number; mloc_messageFaultUnsupportedOperation for a
// locale without rules or a number its formatter cannot be made for,
// mloc_messageFaultBadOption for a request the language refuses.
mlocMessageFaultKind mlocWriteGrammarValue(const GrammarValue* value,
                                           const PreparedGrammar* prepared, const mlocDataSet* data,
                                           const mlocLocale* locale, mlocOutput* output);

#endif
