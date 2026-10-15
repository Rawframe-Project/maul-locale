// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The public zone values checked before use (zone_handle.h).

#include "zone_handle.h"

#include "zone_rules.h"

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
    *namedOut = zone->zones != nullptr;
    if (!*namedOut)
    {
        return zone->zonesSize == 0 && zone->identifier == 0 &&
               zone->offsetMinutes >= -maxOffsetMinutes && zone->offsetMinutes <= maxOffsetMinutes;
    }
    return zone->offsetMinutes == 0 &&
           mlocZoneTablesOf((DataSpan){(const uint8_t*)zone->zones, zone->zonesSize}, t) &&
           zone->identifier < t->identifierCount;
}

int32_t mlocTimeZoneOffsetAt(const mlocTimeZone* zone, const ZoneTables* t, bool named,
                             int64_t seconds)
{
    if (!named)
    {
        return zone->offsetMinutes * 60;
    }
    ZoneOffset o = mlocZoneOffsetAt(t, mlocZoneIdentifierZone(t, zone->identifier), seconds);
    return o.standard + o.save;
}
