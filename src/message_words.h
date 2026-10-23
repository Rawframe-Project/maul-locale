// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The mloc: functions of messages over numbers: mloc:spellout, a number in
// words, ordinal suffixes or an algorithmic numbering system by the
// library's spell-out rules, and mloc:relative, an amount of a unit
// relative to now by its relative time formatter. Their values are number
// values (message_number), of their functions.

#ifndef MLOC_SRC_MESSAGE_WORDS_H
#define MLOC_SRC_MESSAGE_WORDS_H

#include "message_number.h"

#include <stdbool.h>

// Whether a number value is a mloc:spellout or mloc:relative value,
// written and prepared here.
bool mlocWordsValue(const NumberValue* value);

// Prepares a value as its expression makes it; false when its formatter
// cannot be made.
bool mlocPrepareWords(const NumberValue* value, const mlocDataSet* data, const mlocLocale* locale,
                      PreparedNumber* out);

// Writes a value in a locale, with the formatter prepared for it when
// prepared (which may be null) has its options, else one made here;
// mloc_messageFaultUnsupportedOperation when the formatter cannot be made
// (the data without the spell-out rules or relative times, a rule set,
// numbering system or unit the locale lacks).
mlocMessageFaultKind mlocWriteWordsValue(const NumberValue* value, const PreparedNumber* prepared,
                                         const mlocDataSet* data, const mlocLocale* locale,
                                         mlocOutput* output);

#endif
