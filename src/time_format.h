// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time formatter's state made and checked (time_format.c), for the
// time formatter and the date-time formatter (record mloc-0042).

#ifndef MLOC_TIME_FORMAT_H
#define MLOC_TIME_FORMAT_H

#include "time_write.h"
#include "zone_format.h"

// What a state was made from: the time set it took its patterns from,
// with its unit, and whether its clock counts 12 hours.
typedef struct TimeChoice
{
    DatePatterns patterns;
    const uint8_t* set;
    bool twelve;
} TimeChoice;

// Whether time options are in range.
bool mlocValidTimeOptions(const mlocTimeOptions* options);

// Makes a formatter's state from valid options; its result as
// mlocInitTimeFormatter's. choice, when not null, receives what it was
// made from.
mlocResult mlocMakeTimeFormat(TimeFormat* format, const mlocDataSet* data, const mlocLocale* locale,
                              const mlocTimeOptions* options, TimeChoice* choice);

// Whether a state is one mlocMakeTimeFormat made.
bool mlocTimeFormatSound(const TimeFormat* format);

// The state of a formatter the library made without a zone; false for
// any other.
bool mlocReadTimeFormat(const mlocTimeFormatter* opaque, TimeFormat* format);

// Whether a pattern's fields are those a time writer writes, a zone's
// among them.
bool mlocWritableTime(const uint8_t* pattern, uint32_t length, bool* zonedOut);

// Writes a zone field of a pattern: the context is a ZoneWriting.
typedef struct ZoneWriting
{
    const ZoneFormat* zone;
    const mlocTimeZone* timeZone;
    const mlocInstant* instant;
} ZoneWriting;

void mlocWriteZoneField(void* context, const uint8_t* item, NumberWriter* w);

#endif
