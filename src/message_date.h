// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date and time functions of messages (UTS #35 Part 9, "Date and Time
// Value Formatting", Draft): a date value, its operands (date/time
// literals and the date, time and date-time arguments), the options each
// function takes, and its text by the library's date, time and date-time
// formatters.

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

// What a date value's operand had, as bits: an exact time is a zoned one
// (an instant, an offset or a zone given).
enum
{
    dateHasDate = 1u << 0,
    dateHasTime = 1u << 1,
    dateHasZone = 1u << 2,
};

// The zone a value is shown in: the formatter's default, its operand's
// (timeZone=input) or the timeZone option's.
enum
{
    dateZoneDefault = 0,
    dateZoneInput = 1,
    dateZoneGiven = 2,
};

// An option a date value was given, as a bit of its given mask.
enum
{
    dateGivenFields = 1u << 0,
    dateGivenLength = 1u << 1,
    dateGivenPrecision = 1u << 2,
    dateGivenHour12 = 1u << 3,
    dateGivenCalendar = 1u << 4,
    dateGivenTimeZone = 1u << 5,
    dateGivenZoneStyle = 1u << 6,
};

// What an operand holds: its date and time (a zoned one's in its zone),
// and for a zoned one its instant and zone; has says which.
typedef struct DateMoment
{
    mlocDateTime local;
    mlocInstant instant;
    mlocTimeZone zone;
    uint8_t has;
} DateMoment;

// A date value: the date and time (a time alone on the first day of
// 1970, a date alone at midnight; a zoned one's in its operand's zone),
// a zoned one's instant and zone, what the operand had, the expression
// that made it, and its options, each read only where its bit is set.
typedef struct DateValue
{
    mlocDateTime value;
    mlocInstant instant;
    mlocTimeZone zone;
    // The timeZone option's zone, when zoneMode is dateZoneGiven.
    mlocTimeZone timeZone;
    uint32_t origin;
    uint32_t given;
    uint8_t function;
    uint8_t has;
    // An mlocDateFieldSet, an mlocDateLength, an mlocTimePrecision, an
    // mlocHourCycle, an mlocCalendar, a dateZone and an mlocZoneStyle.
    uint8_t fields;
    uint8_t length;
    uint8_t precision;
    uint8_t hourCycle;
    uint8_t calendar;
    uint8_t zoneMode;
    uint8_t zoneStyle;
} DateValue;

// The options a value's formatter is made with, without padding, so two
// compare by their bytes: the given bits other than timeZone's, and the
// function and options.
typedef struct DateKey
{
    uint32_t given;
    uint8_t function;
    uint8_t fields;
    uint8_t length;
    uint8_t precision;
    uint8_t hourCycle;
    uint8_t calendar;
    uint8_t zoneStyle;
    uint8_t reserved;
} DateKey;

// What binding prepares for a date function's expression: the key of
// the value it makes and its formatter, of the value's function. A bound
// message holds the bytes up to the end of the formatter it has.
typedef struct PreparedDate
{
    DateKey key;
    union
    {
        mlocDateFormatter date;
        mlocTimeFormatter time;
        mlocDateTimeFormatter dateTime;
    } formatter;
} PreparedDate;

// Reads a date/time literal (or a string argument): an ISO 8601 date, or
// date-time with an optional Z or offset (to the minute) and zone
// annotation, as mlocParseTemporalString reads them, with a year; false
// for anything else, a date no calendar has, a zone the data lacks, an
// offset the zone did not have then, a calendar annotation other than
// iso8601, or an instant outside Temporal's range. The date is the ISO
// calendar's. Z without a zone is UTC's zone (an offset zone of 0 when
// the data has none).
bool mlocDateLiteral(const uint8_t* text, size_t length, const mlocDataSet* data, DateMoment* out);

// Whether a date argument, time argument or date-time argument holds
// what the library makes.
bool mlocGoodDate(const mlocDate* date);
bool mlocGoodTime(const mlocTime* time);

// Whether an instant is in Temporal's range, and a zoned date-time
// argument's zone one the library made.
bool mlocGoodInstant(const mlocInstant* instant);
bool mlocGoodZoned(const mlocMessageValue* argument);

// What an argument of a date kind holds (an instant's zone UTC's, as for
// a literal with Z); false for another kind.
bool mlocArgumentDateTime(const mlocMessageValue* argument, const mlocDataSet* data,
                          DateMoment* out);

// A date value of a function for what an operand holds, no option given.
DateValue mlocMakeDateValue(DateFunction function, const DateMoment* moment);

// A value of a function from a date value: its date and time, and its
// override options (hour12, calendar, timeZone); its other options are
// not passed on.
DateValue mlocDateFromValue(DateFunction function, const DateValue* operand);

// Whether a function takes an operand of these parts: :date and
// :datetime need a date, :time takes a date alone at midnight.
bool mlocDateTakes(DateFunction function, uint8_t has);

// Sets an option the expression gives by name: none, also for a name the
// function does not take (ignored); mloc_messageFaultBadOption for a
// value that is not one of the option's (a zone the data lacks), or an
// option other than an override set by a variable (ignored either way);
// mloc_messageFaultBadOperand for timeZone=input over a value with no
// zone, which then shows in the default zone.
mlocMessageFaultKind mlocSetDateOption(DateValue* value, DateFunction function, const uint8_t* name,
                                       size_t length, const NumberOption* option,
                                       const mlocDataSet* data);

// After its options: mloc_messageFaultBadOption, the zone style dropped,
// for a zone style over a floating value with no zone to show (none
// given, no default) or no date to name it at (a time alone).
mlocMessageFaultKind mlocCheckDateZone(DateValue* value, bool hasDefault);

// The bytes a prepared value of a function takes in a bound message.
size_t mlocPreparedDateSize(DateFunction function);

// Prepares a value as its expression makes it; false when its formatter
// cannot be made.
bool mlocPrepareDate(const DateValue* value, const mlocDataSet* data, const mlocLocale* locale,
                     PreparedDate* out);

// The zone a value is shown in (the option's, its operand's for input,
// else the default, else UTC's for a zoned value); false for none.
bool mlocDateZone(const DateValue* value, const mlocDataSet* data, const mlocTimeZone* defaultZone,
                  mlocTimeZone* zoneOut);

// Writes a value in a locale, a zoned one in its zone (defaultZone, the
// formatter's, may be null), with the formatter prepared for it when
// prepared (which may be null) has its options, else one made here;
// mloc_messageFaultUnsupportedOperation when the formatter cannot be made
// (the data without the dates or the zone names, a calendar the locale
// cannot take) or the zone's local time is out of range.
mlocMessageFaultKind mlocWriteDateValue(const DateValue* value, const PreparedDate* prepared,
                                        const mlocDataSet* data, const mlocLocale* locale,
                                        const mlocTimeZone* defaultZone, mlocOutput* output);

// Writes a date, time, date-time, instant or zoned date-time argument in
// ISO 8601 (an instant in UTC with Z, a zoned one with its offset and
// zone as Temporal writes it) into text: the length, or 0 when capacity
// is too small or the kind no date.
size_t mlocWriteIsoDateTime(const mlocMessageValue* argument, char* text, size_t capacity);

#endif
