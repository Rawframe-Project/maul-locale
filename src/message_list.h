// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloc:list: a list value over a list argument (its items' texts),
// ECMA-402's ListFormat options, its text by the library's list formatter,
// and its selection by its count: an exact count, else the count's plural
// category.

#ifndef MLOC_SRC_MESSAGE_LIST_H
#define MLOC_SRC_MESSAGE_LIST_H

#include "message_number.h"

#include "maul-locale/list.h"
#include "maul-locale/message.h"
#include "maul-locale/plural.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// An option a list value was given, as a bit of its given mask.
enum
{
    listGivenType = 1u << 0,
    listGivenStyle = 1u << 1,
};

// The options a list's formatter is made with, without padding: an
// mlocListType and an mlocListWidth, each where given.
typedef struct ListKey
{
    uint32_t given;
    uint8_t type;
    uint8_t width;
    uint8_t reserved[2];
} ListKey;

// A list value: its items (an argument's, which outlive the call), the
// expression that made it, and its options.
typedef struct ListValue
{
    const mlocListItem* items;
    size_t count;
    uint32_t origin;
    ListKey key;
} ListValue;

// What binding prepares for a mloc:list expression: its options, the
// cardinal rules its selection reads (rulesMade set when made), and its
// formatter.
typedef struct PreparedList
{
    ListKey key;
    uint32_t rulesMade;
    mlocPluralRules rules;
    mlocListFormatter formatter;
} PreparedList;

// Whether a list argument holds what the library takes: items with text
// where their lengths ask, a count of at most UINT32_MAX.
bool mlocGoodList(const mlocMessageValue* argument);

// A list value of a list argument's items, no option given.
ListValue mlocMakeListValue(const mlocMessageValue* argument);

// Sets an option the expression gives by name: none, also for a name
// mloc:list does not take (ignored); mloc_messageFaultBadOption for a
// value the option does not take, or a variable's (they choose the
// formatter).
mlocMessageFaultKind mlocSetListOption(ListValue* value, const uint8_t* name, size_t length,
                                       const NumberOption* option);

// Prepares a value as its expression makes it; false when its formatter
// cannot be made.
bool mlocPrepareList(const ListValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedList* out);

// Writes a value in a locale, with the formatter prepared for it when
// prepared (which may be null) has its options, else one made here;
// mloc_messageFaultUnsupportedOperation when the formatter cannot be made.
mlocMessageFaultKind mlocWriteListValue(const ListValue* value, const PreparedList* prepared,
                                        const mlocDataSet* data, const mlocLocale* locale,
                                        mlocOutput* output);

// Whether a key matches a list by its count: a number literal its count
// exactly as an integer is written, a plural keyword the count's
// cardinal category (with the prepared rules when prepared, which may be
// null, has them); mloc_messageFaultBadVariantKey for another key,
// mloc_messageFaultUnsupportedOperation when the rules cannot be made.
mlocMessageFaultKind mlocMatchList(const ListValue* value, const PreparedList* prepared,
                                   const mlocDataSet* data, const mlocLocale* locale,
                                   const uint8_t* key, size_t length, bool* matchOut);

#endif
