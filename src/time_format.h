// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time formatter's state made and checked (time_format.c), for the
// time formatter and the date-time formatter.

#ifndef MLOC_TIME_FORMAT_H
#define MLOC_TIME_FORMAT_H

#include "date_format.h"
#include "time_write.h"
#include "zone_format.h"

#include "maul-locale/base.h"

// What a state was made from: the time set it took its patterns from,
// with its unit, and whether its clock counts 12 hours.
typedef struct TimeChoice
{
    DatePatterns patterns;
    const uint8_t* set;
    bool twelve;
} TimeChoice;

// Whether time options are in range.
MLOC_NODISCARD bool mlocValidTimeOptions(const mlocTimeOptions* options);

// Makes a formatter's state from valid options and what the locale's
// formatters share (null to read it); its result as
// mlocInitTimeFormatter's. choice, when not null, receives what it was
// made from.
mlocResult mlocMakeTimeFormat(TimeFormat* format, const mlocDataSet* data, const mlocLocale* locale,
                              const DateLocale* shared, const mlocTimeOptions* options,
                              TimeChoice* choice);

// mlocInitTimeFormatter, in calendar (a BCP 47 calendar type) in place of
// the locale's -u-ca, or in its own for null.
// A range across dates in a zone still writes its dates in the locale's
// own calendar.
mlocResult mlocInitTimeFormatterIn(const mlocDataSet* data, const mlocLocale* locale,
                                   const char* calendar, const mlocTimeOptions* options,
                                   mlocTimeFormatter* formatterOut);

// Whether a state is one mlocMakeTimeFormat made.
MLOC_NODISCARD bool mlocTimeFormatSound(const TimeFormat* format);

// The state of a formatter the library made without a zone; false for
// any other.
MLOC_NODISCARD bool mlocReadTimeFormat(const mlocTimeFormatter* opaque, TimeFormat* format);

// Writes a zone field of a pattern: the context is a ZoneWriting.
typedef struct ZoneWriting
{
    const ZoneFormat* zone;
    const ZoneAt* at;
} ZoneWriting;

void mlocWriteZoneField(void* context, const uint8_t* item, NumberWriter* w);

#endif
