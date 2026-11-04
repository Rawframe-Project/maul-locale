// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The public zone values checked before use (zone_handle.h).

#include "zone_handle.h"

#include "data_view.h"
#include "invariant.h"
#include "kinds.h"
#include "zone_rules.h"

#include <string.h>

enum
{
    maxOffsetMinutes = 23 * 60 + 59,
};

bool mlocValidInstant(const mlocInstant* i)
{
    return i->nanoseconds >= 0 && i->nanoseconds <= 999999999 && i->seconds >= zoneMinSeconds &&
           (i->seconds < zoneMaxSeconds || (i->seconds == zoneMaxSeconds && i->nanoseconds == 0));
}

bool mlocReadTimeZone(const mlocTimeZone* zone, ZoneTables* t, bool* namedOut)
{
    if (zone == nullptr)
    {
        return false;
    }
    DataView view = mlocViewOf(&zone->data);
    *namedOut = view.bytes != nullptr;
    if (!*namedOut)
    {
        static const mlocData none;
        return memcmp(&zone->data, &none, sizeof none) == 0 && zone->unit == 0 &&
               zone->identifier == 0 && zone->offsetMinutes >= -maxOffsetMinutes &&
               zone->offsetMinutes <= maxOffsetMinutes;
    }
    // The unit is read through the data and checked here, at each call:
    // the zone's fields may come from another zone.
    uint32_t kind = 0;
    DataSpan span;
    return zone->offsetMinutes == 0 && mlocDataUnitAt(&view, zone->unit, &kind, &span) &&
           kind == kindZones && mlocZoneTablesOf(span, t) && zone->identifier < t->identifierCount;
}

int32_t mlocTimeZoneOffsetAt(const mlocTimeZone* zone, const ZoneTables* t, bool named,
                             int64_t seconds)
{
    if (!named)
    {
        return zone->offsetMinutes * 60;
    }
    // mlocReadTimeZone checked the identifier, and the zones validator its
    // zone.
    uint32_t index = mlocZoneIdentifierZone(t, zone->identifier);
    MLOC_ASSERT(index < t->zoneCount);
    ZoneOffset o = mlocZoneOffsetAt(t, index, seconds);
    return o.standard + o.save;
}
