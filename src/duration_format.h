// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A duration formatter's state (maul-locale/duration.h): ECMA-402's
// resolved options, the locale's names of each unit-style field, its unit
// list and its digital separators.

#ifndef MLOC_DURATION_FORMAT_H
#define MLOC_DURATION_FORMAT_H

#include "list_data.h"
#include "number_format.h"
#include "unit_patterns.h"

#include "maul-locale/duration.h"

enum
{
    durationStamp = 0x44,
    // A fraction field's resolved style: numeric folded into the next
    // larger field.
    durationFieldFraction = 6,
    // The room for a separator between numeric fields.
    durationSeparatorMost = 15,
    // The bytes the unit-style fields' patterns take, each kept once.
    durationPool = 3072,
};

typedef struct DurationFormat
{
    // The locale's decimal formatter, its cardinal rule set.
    Formatter formatter;
    mlocPluralRules rules;
    ListPatterns list;
    // Each unit-style field's patterns in the pool.
    PatternSlot slots[mloc_durationFieldCount];
    // Resolved: mlocDurationFieldStyle or durationFieldFraction, and
    // mlocDurationDisplay (auto or always).
    uint8_t style[mloc_durationFieldCount];
    uint8_t display[mloc_durationFieldCount];
    // The whole's mlocDurationStyle.
    uint8_t base;
    uint8_t fixedFraction;
    uint8_t fractionalDigits;
    // Between hours and minutes, between minutes and seconds.
    uint8_t separatorLength[2];
    char separator[2][durationSeparatorMost];
    uint8_t stamp;
    // The pool the patterns are read from, in the formatter itself; null
    // until it is read.
    const uint8_t* patterns;
    // The patterns; last, so that a formatter is read without it.
    uint8_t pool[durationPool];
} DurationFormat;

// ECMA-402's GetDurationUnitOptions over the fields in order, into the
// resolved styles and displays (the locale's two-digit hours aside);
// false for options out of range or in conflict.
bool mlocResolveDurationOptions(const mlocDurationOptions* options,
                                uint8_t style[mloc_durationFieldCount],
                                uint8_t display[mloc_durationFieldCount]);

// The separators and hour padding of an hms pattern, "h" or "hh", a
// separator, "m" or "mm", a separator, "s" or "ss", into a formatter's
// state: false for another.
bool mlocReadDurationDigital(const char* text, size_t length, DurationFormat* format,
                             bool* twoDigitOut);

// Whether a formatter's state is one the library made: its styles,
// displays, digits, separators, list and patterns within their bounds.
bool mlocValidDurationFormat(const DurationFormat* format);

#endif
