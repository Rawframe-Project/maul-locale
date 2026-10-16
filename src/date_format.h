// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date formatter's state made, checked and read (date_format.c), for
// the date formatter and the date-time formatter (record mloc-0042).

#ifndef MLOC_DATE_FORMAT_H
#define MLOC_DATE_FORMAT_H

#include "date_write.h"
#include "zone_format.h"

// A date read for a formatter: its fields in the formatter's calendar,
// the Gregorian year its year begins in, and the pattern its year needs.
typedef struct DateValue
{
    mlocDateFields fields;
    int32_t related;
    uint32_t pattern;
} DateValue;

// Whether date options are in range.
bool mlocValidDateOptions(const mlocDateOptions* options);

// Makes a formatter's state from valid options; its result as
// mlocInitDateFormatter's.
mlocResult mlocMakeDateFormat(DateFormat* format, const mlocDataSet* data, const mlocLocale* locale,
                              const mlocDateOptions* options);

// Whether a state is one mlocMakeDateFormat made.
bool mlocDateFormatSound(const DateFormat* format);

// Reads a date for a sound state; false for a date outside Temporal's
// range.
bool mlocReadDateValue(const DateFormat* format, const mlocDate* date, DateValue* value);

// The glue's style of date options (glueFull to glueShort): a standard
// format's length, or by the field set's month at its length (four
// letters with a weekday full, four letters long, three medium, else
// short) by a time set's flags, as ICU4C chooses it.
uint32_t mlocDateGlueStyle(const mlocDateOptions* options, uint32_t flags);

// Whether a pattern holds only fields the date formatter writes.
bool mlocWritableDate(const uint8_t* pattern, uint32_t length);

// Whether glue holds {0} and {1}.
bool mlocValidGlue(const uint8_t* glue, uint32_t length);

#endif
