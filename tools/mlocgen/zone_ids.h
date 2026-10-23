// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone identifiers: every Zone and Link name of the source, the zone
// whose data each reads, and its primary identifier as ECMA-402's
// AvailableNamedTimeZoneIdentifiers resolves it.

#ifndef MLOCGEN_ZONE_IDS_H
#define MLOCGEN_ZONE_IDS_H

#include "tzdb.h"

#include <stddef.h>

typedef struct ZoneIdentifier
{
    char name[tzdbMaxName];
    // The zone in Tzdb.zones the name reaches through its Links.
    int zone;
    // The primary identifier's place in the list.
    int primary;
} ZoneIdentifier;

// The identifiers in *out (the caller frees it), sorted by their bytes
// with ASCII letters folded to lower case. A Link not in zone.tab takes
// its country from CLDR's bcp47/timezone.xml, which groups each place's
// names, under the common directory. Returns the count, or -1 and a
// message: a name twice when case is ignored, a Link that reaches no
// Zone, one whose country neither file gives, or, when strict, a primary
// other than CLDR's for a name CLDR has (otherwise reported on standard
// error: a source of another release than CLDR's may differ).
int ZoneIdentifiers(const Tzdb* tzdb, const char* commonDirectory, bool strict,
                    ZoneIdentifier** out, char* errorOut, size_t errorCapacity);

#endif
