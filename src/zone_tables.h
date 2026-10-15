// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time zones of the pinned tzdb release (record mloc-0041), which the
// generator compiles from tzdb's source to src/generated/zone_data.c.
// Each zone is a table of transitions in universal time, then a closing
// rule of two yearly changes, or none when its last offset stays. An
// offset is a standard offset and a saving, never negative, in seconds:
// the daylight periods are tzdb's rearguard ones.
//
// Transition times are int32_t seconds since 1970, except those outside
// that range (before December 1901 or after January 2038), which are the
// zone's first few or last few and are kept as int64_t.

#ifndef MLOC_ZONE_TABLES_H
#define MLOC_ZONE_TABLES_H

#include <stdint.h>

typedef struct ZoneOffset
{
    int32_t standard;
    int32_t save;
} ZoneOffset;

// How a closing change names its day: the day of the month, or the
// given weekday on or before it, or on or after it. A weekday rule may
// step into the next or previous month.
typedef enum ZoneDay
{
    zoneDayOfMonth,
    zoneOnOrBefore,
    zoneOnOrAfter,
} ZoneDay;

// One yearly change: its day (month 1 to 12, weekday 0 for Sunday to 6),
// its time in universal time on that day (seconds, which may pass the
// day's ends), and the offset it starts, in mlocZoneOffsets.
typedef struct ZoneChange
{
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t weekday;
    uint8_t day;
    int32_t time;
    uint16_t offset;
} ZoneChange;

// The changes apply every year from firstYear on, in this order within a
// year.
typedef struct ZoneClosing
{
    int32_t firstYear;
    ZoneChange changes[2];
} ZoneClosing;

typedef struct ZoneRecord
{
    // The zone's int32_t times in mlocZoneTimes, and its transitions'
    // offsets in mlocZoneTypes.
    uint32_t narrowFirst;
    uint32_t typeFirst;
    uint16_t transitionCount;
    // Its int64_t times in mlocZoneWideTimes: its first wideEarly
    // transitions' and its last wideLate transitions'.
    uint16_t wideFirst;
    uint8_t wideEarly;
    uint8_t wideLate;
    // Its offsets, as indexes of mlocZoneOffsets in mlocZoneOffsetList.
    // A transition's offset (in mlocZoneTypes) and the offset before the
    // first transition are places in that list.
    uint16_t offsetFirst;
    uint8_t initial;
    // Its closing rule in mlocZoneClosings, from 1, or 0.
    uint8_t closing;
    // Its name in mlocZoneNameText, which ends with a NUL.
    uint16_t name;
} ZoneRecord;

// The zones, by name in byte order.
extern const ZoneRecord mlocZones[];
extern const uint16_t mlocZoneCount;

extern const ZoneOffset mlocZoneOffsets[];
extern const uint16_t mlocZoneOffsetList[];
extern const int32_t mlocZoneTimes[];
extern const int64_t mlocZoneWideTimes[];
extern const uint8_t mlocZoneTypes[];
extern const ZoneClosing mlocZoneClosings[];
extern const char mlocZoneNameText[];

// The tzdb release, such as "2026e".
extern const char mlocZoneRelease[];

#endif
