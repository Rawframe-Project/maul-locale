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

// What a locale's date and time formatters both read: its preferences,
// its number symbols with their result (a fallback of the numbering
// system asked for), its date units, and the names of its calendar (the
// ISO calendar's the Gregorian's) and of the Gregorian; read once for a
// date-time formatter.
typedef struct DateLocale
{
    mlocLocalePreferences preferences;
    NumberCore core;
    mlocResult digits;
    uint32_t calendar;
    DateUnits units;
    const uint8_t* nameSet;
    const uint8_t* gregorianSet;
} DateLocale;

// Reads what a locale's date and time formatters share: the number
// core's result; mloc_errorNotFound for data without the preferences,
// the number units or the names.
mlocResult mlocReadDateLocale(DateLocale* shared, const mlocDataSet* data,
                              const mlocLocale* locale);

// Whether date options are in range.
bool mlocValidDateOptions(const mlocDateOptions* options);

// Makes a formatter's state from valid options and what the locale's
// formatters share (null to read it); its result as
// mlocInitDateFormatter's.
mlocResult mlocMakeDateFormat(DateFormat* format, const mlocDataSet* data, const mlocLocale* locale,
                              const DateLocale* shared, const mlocDateOptions* options);

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

// Whether glue holds {0} and {1}.
bool mlocValidGlue(const uint8_t* glue, uint32_t length);

#endif
