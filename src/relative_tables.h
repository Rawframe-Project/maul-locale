// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The units of the relative time data file, relative.mloc (record
// mloc-0043), which the generator compiles from the locale files'
// fields. All integers are little-endian.
//
// Relative time, a unit per locale key. The payload is a header (u32
// entry count, u32 string count, u32 text size), the entries' keys (u16
// each, strictly ascending), their strings (u16 each, below the string
// count), the strings' offsets (u32 each, string count + 1, from 0 to the
// text size, ascending), then the text, UTF-8 with no byte below 0x20;
// entries share a string. A key is (unit times relativeWidthCount plus
// width) times relativeSlots plus a slot: the names of the offsets -2 to
// 2 ("yesterday", "today"), then the future patterns of the plural
// categories zero to other ("in {0} days"), then the past ones ("{0} days
// ago"). A pattern holds "{0}" at most once (ar's "خلال سنة واحدة"
// writes no number); a name holds none. An empty string is no value.
//
// The lookup of a width's slot takes the first key along the locale's
// chain that files it, else the next wider width's the same way (narrow,
// short, long), as root's aliases send them; an empty string ends it. An
// entry is CLDR's value resolved along the chain, filed where that lookup
// would give another; a category's pattern equal to other's is not, the
// formatter taking other's where a category has none.

#ifndef MLOC_RELATIVE_TABLES_H
#define MLOC_RELATIVE_TABLES_H

#include "data_view.h"

#include <stdint.h>

// The units, in CLDR's field names: year, quarter, month, week, day,
// hour, minute, second, then the weekdays sun to sat.
typedef enum RelativeUnit
{
    relativeYear = 0,
    relativeQuarter = 1,
    relativeMonth = 2,
    relativeWeek = 3,
    relativeDay = 4,
    relativeHour = 5,
    relativeMinute = 6,
    relativeSecond = 7,
    relativeSunday = 8,
    relativeUnitCount = 15,
} RelativeUnit;

enum
{
    relativeHeaderSize = 12,
    // Long, short, narrow.
    relativeWidthCount = 3,
    // The offsets named, -2 to 2.
    relativeNames = 5,
    relativeFuture = relativeNames,
    relativePast = relativeFuture + 6,
    relativeSlots = relativePast + 6,
    relativeKeyCount = relativeUnitCount * relativeWidthCount * relativeSlots,
};

// Whether a payload is a valid relative time unit.
bool mlocValidRelative(const DataView* view, DataSpan payload);

#endif
