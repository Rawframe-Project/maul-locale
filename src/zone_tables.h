// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zones unit of the data file, which the generator compiles from the
// pinned tzdb release's source: each zone a table of transitions in
// universal time, then a closing rule of two yearly changes, or none when
// its last offset stays; and every Zone and Link name with the zone it
// reads and its primary identifier. An offset is a standard offset and a
// saving, never negative, in seconds: the daylight periods are tzdb's
// rearguard ones.
//
// The payload is a header of ten 32-bit counts and the tzdb release the
// unit was compiled from (eight bytes: one to eight ASCII digits and
// lower-case letters, then NULs), then these sections, each starting on
// an 8-byte boundary:
//   zones        24 bytes each: the first of its int32 times (u32), of its
//                offset places (u32), its transition count (u16), the
//                first of its int64 times (u16), the first of its offsets
//                in the list (u16), its name's identifier (u16), how many
//                of its first and last transitions are int64 (u8, u8),
//                its offset count (u8), the offset before its first
//                transition (a place, u8), its closing rule from 1 or 0
//                (u8), three zero bytes
//   identifiers  12 bytes each, sorted by their bytes with ASCII letters
//                folded to lower case: the name's offset in the text
//                (u32), its length (u8), a zero byte, its zone (u16), its
//                primary identifier (u16), two zero bytes
//   offsets      8 bytes each: standard offset and saving (i32, i32)
//   list         the zones' offsets as indexes of the offsets (u16)
//   narrow       transition times in seconds since 1970 (i32)
//   wide         transition times outside int32_t's range (i64)
//   types        each transition's offset as a place in its zone's (u8)
//   closings     28 bytes each: its first year (i32), then two changes
//                of 12 bytes: month 2 to 11 (u8), day of the month (u8),
//                weekday from 0 for Sunday (u8), how the day is named
//                (u8, ZoneDay), the universal time on that day in seconds
//                (i32, which may pass the day's ends), the offset it
//                starts (u16), two zero bytes
//   text         the names, ASCII, unterminated
//   primaries    the primary identifiers, sorted by their bytes (u16), as
//                ECMA-402 lists them
// Within a zone, times ascend; its int64 times are its first few (before
// December 1901) and last few (after January 2038).

#ifndef MLOC_ZONE_TABLES_H
#define MLOC_ZONE_TABLES_H

#include "data_view.h"

#include <stdint.h>

enum
{
    zoneHeaderSize = 48,
    zoneSectionCount = 10,
    zoneRecordSize = 24,
    zoneIdentifierSize = 12,
    zoneOffsetSize = 8,
    zoneClosingSize = 28,
    zoneChangeSize = 12,
};

// How a closing change names its day: the day of the month, or the
// given weekday on or before it, or on or after it. A weekday rule may
// step into the next or previous month.
typedef enum ZoneDay
{
    zoneDayOfMonth,
    zoneOnOrBefore,
    zoneOnOrAfter,
} ZoneDay;

typedef struct ZoneOffset
{
    int32_t standard;
    int32_t save;
} ZoneOffset;

typedef struct ZoneChange
{
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t weekday;
    uint8_t day;
    int32_t time;
    uint16_t offset;
} ZoneChange;

// A zone's record, read out of its 24 bytes.
typedef struct ZoneRecord
{
    uint32_t narrowFirst;
    uint32_t typeFirst;
    uint16_t transitionCount;
    uint16_t wideFirst;
    uint16_t offsetFirst;
    uint16_t name;
    uint8_t wideEarly;
    uint8_t wideLate;
    uint8_t offsetCount;
    uint8_t initial;
    uint8_t closing;
} ZoneRecord;

// The unit's sections and their counts.
typedef struct ZoneTables
{
    const uint8_t* zones;
    const uint8_t* identifiers;
    const uint8_t* offsets;
    const uint8_t* list;
    const uint8_t* narrow;
    const uint8_t* wide;
    const uint8_t* types;
    const uint8_t* closings;
    const uint8_t* text;
    const uint8_t* primaries;
    uint32_t zoneCount;
    uint32_t identifierCount;
    uint32_t offsetCount;
    uint32_t listCount;
    uint32_t narrowCount;
    uint32_t wideCount;
    uint32_t typeCount;
    uint32_t closingCount;
    uint32_t textSize;
    uint32_t primaryCount;
    // The tzdb release's name, not NUL-terminated.
    const uint8_t* release;
    uint32_t releaseLength;
} ZoneTables;

// The sections of a payload whose size their counts give; false when it
// is not.
bool mlocZoneTablesOf(DataSpan payload, ZoneTables* out);

// The validator of the zones unit: the sections, every index and range,
// times ascending within a zone, identifiers in order and unique with
// case folded, each primary its own primary and of its name's zone,
// the primaries listed once each in byte order, offsets within a day,
// closing changes in February to November.
bool mlocValidZones(const DataView* view, DataSpan payload);

// The tables of the first source of a set with a zones unit, or false.
bool mlocFindZoneTables(const mlocDataSet* data, ZoneTables* out);

ZoneRecord mlocZoneRecord(const ZoneTables* t, uint32_t zone);
ZoneOffset mlocZoneOffsetOf(const ZoneTables* t, uint32_t index);
int32_t mlocZoneClosingYear(const ZoneTables* t, uint32_t closing);
ZoneChange mlocZoneClosingChange(const ZoneTables* t, uint32_t closing, uint32_t change);

// An identifier's name, zone and primary identifier.
const char* mlocZoneIdentifierName(const ZoneTables* t, uint32_t identifier, size_t* lengthOut);
uint32_t mlocZoneIdentifierZone(const ZoneTables* t, uint32_t identifier);
uint32_t mlocZoneIdentifierPrimary(const ZoneTables* t, uint32_t identifier);

#endif
