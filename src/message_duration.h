// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// mloc:duration (record mloc-0045): a duration value, its operands (a
// duration argument, an ISO 8601 duration literal, a number of seconds
// balanced up to hours as MF1's duration takes it), ECMA-402's
// DurationFormat options, and its text by the library's duration
// formatter.

#ifndef MLOC_SRC_MESSAGE_DURATION_H
#define MLOC_SRC_MESSAGE_DURATION_H

#include "message_number.h"

#include "maul-locale/duration.h"
#include "maul-locale/message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// An option a duration value was given, as a bit of its given mask: the
// style, each field's style and display (by mlocDurationField), and the
// fractional digits.
enum
{
    durationGivenStyle = 1u << 0,
    durationGivenField = 1u << 1,
    durationGivenDisplay = 1u << 11,
    durationGivenFraction = 1u << 21,
};

// The options a duration's formatter is made with, without padding, so
// two compare by their bytes: an mlocDurationStyle, each field's
// mlocDurationFieldStyle and mlocDurationDisplay, and the fractional
// digits where given.
typedef struct DurationKey
{
    uint32_t given;
    uint8_t style;
    uint8_t fields[mloc_durationFieldCount];
    uint8_t displays[mloc_durationFieldCount];
    uint8_t fractionalDigits;
    uint8_t reserved[2];
} DurationKey;

// A duration value: its fields, the expression that made it, and its
// options.
typedef struct DurationValue
{
    mlocDurationValues values;
    uint32_t origin;
    DurationKey key;
} DurationValue;

// What binding prepares for a mloc:duration expression: its options and
// its formatter.
typedef struct PreparedDuration
{
    DurationKey key;
    uint32_t reserved;
    mlocDurationFormatter formatter;
} PreparedDuration;

// Whether a duration argument's fields are a duration the library makes.
bool mlocGoodDuration(const mlocDurationValues* values);

// A duration value of fields, no option given.
DurationValue mlocMakeDurationValue(const mlocDurationValues* values);

// Reads an ISO 8601 duration literal (or string argument) as
// mlocParseDuration reads it; false for anything else.
bool mlocDurationLiteral(const uint8_t* text, size_t length, mlocDurationValues* out);

// A number of seconds as a duration balanced up to hours, its fraction
// truncated to nanoseconds; false for a number not finite or of 2^53
// seconds or more.
bool mlocDurationFromSeconds(const mlocDecimal* seconds, mlocDurationValues* out);

// Sets an option the expression gives by name: none, also for a name
// mloc:duration does not take (ignored); mloc_messageFaultBadOption for a
// value the option does not take, or a variable's (they choose the
// formatter).
mlocMessageFaultKind mlocSetDurationOption(DurationValue* value, const uint8_t* name, size_t length,
                                           const NumberOption* option);

// Prepares a value as its expression makes it; false when its formatter
// cannot be made.
bool mlocPrepareDuration(const DurationValue* value, const mlocDataSet* data,
                         const mlocLocale* locale, PreparedDuration* out);

// Writes a value in a locale, with the formatter prepared for it when
// prepared (which may be null) has its options, else one made here;
// mloc_messageFaultUnsupportedOperation when the formatter cannot be made
// (the data without the unit names, options in conflict as ECMA-402
// refuses them).
mlocMessageFaultKind mlocWriteDurationValue(const DurationValue* value,
                                            const PreparedDuration* prepared,
                                            const mlocDataSet* data, const mlocLocale* locale,
                                            mlocOutput* output);

// Writes a duration argument in ISO 8601 (Temporal's form) into text:
// the length, or 0 when capacity is too small.
size_t mlocWriteIsoDuration(const mlocDurationValues* values, char* text, size_t capacity);

#endif
