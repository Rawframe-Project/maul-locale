// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone's offsets read from the zones unit (zone_tables.h): an identifier
// by name, the offset at a time and the next transition. Times are seconds
// since 1970 in universal time, within Temporal's range (±10⁸ days); the
// closing rule runs to its end.

#ifndef MLOC_ZONE_RULES_H
#define MLOC_ZONE_RULES_H

#include "zone_tables.h"

#include <stddef.h>
#include <stdint.h>

// Temporal's range in seconds: 10⁸ days either side of 1970.
static const int64_t zoneMinSeconds = INT64_C(-8640000000000);
static const int64_t zoneMaxSeconds = INT64_C(8640000000000);

// The identifier a name matches with ASCII letters folded (ECMA-262
// compares time zone identifiers so), or -1.
int32_t mlocZoneFindIdentifier(const ZoneTables* t, const char* name, size_t length);

// The offset in effect at a time.
ZoneOffset mlocZoneOffsetAt(const ZoneTables* t, uint32_t zone, int64_t seconds);

// The first transition strictly after a time and no later than
// zoneMaxSeconds, with the offset it starts: false when there is none. A
// transition changes the standard offset or the saving, even where their
// sum stays.
bool mlocZoneNextTransition(const ZoneTables* t, uint32_t zone, int64_t seconds, int64_t* atOut,
                            ZoneOffset* offsetOut);

// The first transition strictly after a time, past zoneMaxSeconds too
// (a local time near the range's end needs it): false when there is
// none.
bool mlocZoneFollowingTransition(const ZoneTables* t, uint32_t zone, int64_t seconds,
                                 int64_t* atOut, ZoneOffset* offsetOut);

// The last transition at or before a time and no earlier than
// zoneMinSeconds, with the offset it starts: false when there is none.
bool mlocZonePreviousTransition(const ZoneTables* t, uint32_t zone, int64_t seconds, int64_t* atOut,
                                ZoneOffset* offsetOut);

#endif
