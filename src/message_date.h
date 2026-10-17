// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date and time functions of messages (record mloc-0045; UTS #35
// Part 9, "Date and Time Value Formatting", Draft): a date value, its
// operands (date/time literals and the date, time and date-time
// arguments), the options each function takes, and its text by the
// library's date, time and date-time formatters.

#ifndef MLOC_SRC_MESSAGE_DATE_H
#define MLOC_SRC_MESSAGE_DATE_H

#include "message_number.h"

#include "maul-locale/datetime.h"
#include "maul-locale/message.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Which function made a date value.
typedef enum DateFunction
{
    dateFunctionDate = 0,
    dateFunctionTime = 1,
    dateFunctionDateTime = 2,
} DateFunction;

// What a date value's operand had, as bits.
enum
{
    dateHasDate = 1u << 0,
    dateHasTime = 1u << 1,
};

// An option a date value was given, as a bit of its given mask.
enum
{
    dateGivenFields = 1u << 0,
    dateGivenLength = 1u << 1,
    dateGivenPrecision = 1u << 2,
    dateGivenHour12 = 1u << 3,
    dateGivenCalendar = 1u << 4,
};

// A date value: the date and time (a time alone on the first day of
// 1970, a date alone at midnight), what the operand had, the expression
// that made it, and its options, each read only where its bit is set.
// Without padding, so two values' options compare by their bytes.
typedef struct DateValue
{
    mlocDateTime value;
    uint32_t origin;
    uint32_t given;
    uint8_t function;
    uint8_t has;
    // An mlocDateFieldSet, an mlocDateLength, an mlocTimePrecision, an
    // mlocHourCycle and an mlocCalendar.
    uint8_t fields;
    uint8_t length;
    uint8_t precision;
    uint8_t hourCycle;
    uint8_t calendar;
    uint8_t reserved;
} DateValue;

// What binding prepares for a date function's expression: the options
// of the value it makes (its date and time, origin and parts cleared)
// and its formatter, of the value's function. A bound message holds the
// bytes up to the end of the formatter it has.
typedef struct PreparedDate
{
    DateValue key;
    union
    {
        mlocDateFormatter date;
        mlocTimeFormatter time;
        mlocDateTimeFormatter dateTime;
    } formatter;
} PreparedDate;

// Reads a date/time literal (or a string argument): an ISO 8601 date, or
// date-time with an optional offset or zone annotation, as
// mlocParseTemporalString reads them, with a year; false for anything
// else or a date no calendar has. The date is the ISO calendar's; hasOut
// receives the parts given.
bool mlocDateLiteral(const uint8_t* text, size_t length, mlocDateTime* valueOut, uint8_t* hasOut);

// Whether a date argument, time argument or date-time argument holds
// what the library makes.
bool mlocGoodDate(const mlocDate* date);
bool mlocGoodTime(const mlocTime* time);

// The date and time of an argument of a date kind, and the parts it has;
// false for another kind.
bool mlocArgumentDateTime(const mlocMessageValue* argument, mlocDateTime* valueOut,
                          uint8_t* hasOut);

// A date value of a function for a date and time with the parts given,
// no option given.
DateValue mlocMakeDateValue(DateFunction function, const mlocDateTime* value, uint8_t has);

// A value of a function from a date value: its date and time, and its
// override options (hour12, calendar); its other options are not passed
// on.
DateValue mlocDateFromValue(DateFunction function, const DateValue* operand);

// Whether a function takes an operand of these parts: :date and
// :datetime need a date, :time takes a date alone at midnight.
bool mlocDateTakes(DateFunction function, uint8_t has);

// Sets an option the expression gives by name: none, also for a name the
// function does not take (ignored); mloc_messageFaultBadOption for a
// value that is not one of the option's, or an option other than an
// override set by a variable (ignored either way).
mlocMessageFaultKind mlocSetDateOption(DateValue* value, DateFunction function, const uint8_t* name,
                                       size_t length, const NumberOption* option);

// The bytes a prepared value of a function takes in a bound message.
size_t mlocPreparedDateSize(DateFunction function);

// Prepares a value as its expression makes it; false when its formatter
// cannot be made.
bool mlocPrepareDate(const DateValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedDate* out);

// Writes a value in a locale, with the formatter prepared for it when
// prepared (which may be null) has its options, else one made here;
// mloc_messageFaultUnsupportedOperation when the formatter cannot be made
// (the data without the dates, a calendar the locale cannot take).
mlocMessageFaultKind mlocWriteDateValue(const DateValue* value, const PreparedDate* prepared,
                                        const mlocDataSet* data, const mlocLocale* locale,
                                        mlocOutput* output);

// Writes a date, time or date-time argument in ISO 8601 into text: the
// length, or 0 when capacity is too small or the kind no date.
size_t mlocWriteIsoDateTime(const mlocMessageValue* argument, char* text, size_t capacity);

#endif
