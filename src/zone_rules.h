// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone's offsets read from the compiled tables (zone_tables.h, record
// mloc-0041): the offset at a time and the transitions around it. Times
// are seconds since 1970 in universal time, within Temporal's range
// (±10⁸ days); the closing rule runs to its end.

#ifndef MLOC_ZONE_RULES_H
#define MLOC_ZONE_RULES_H

#include "zone_tables.h"

#include <stddef.h>
#include <stdint.h>

// Temporal's range in seconds: 10⁸ days either side of 1970.
static const int64_t zoneMinSeconds = INT64_C(-8640000000000);
static const int64_t zoneMaxSeconds = INT64_C(8640000000000);

// The zone named exactly, or -1.
int32_t mlocZoneFind(const char* name, size_t length);

const char* mlocZoneName(uint32_t zone);

// The offset in effect at a time.
ZoneOffset mlocZoneOffsetAt(uint32_t zone, int64_t seconds);

// The first transition strictly after a time and no later than
// zoneMaxSeconds, with the offset it starts: false when there is none. A
// transition changes the standard offset or the saving, even where their
// sum stays.
bool mlocZoneNextTransition(uint32_t zone, int64_t seconds, int64_t* atOut, ZoneOffset* offsetOut);

#endif
