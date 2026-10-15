// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time zones (maul-locale/zone.h, record mloc-0041): identifiers found in
// the zones unit or read as offsets, and the offsets and transitions
// zone_rules.h reads, as ECMA-262's Temporal gives them.

#include "maul-locale/zone.h"

#include "bytes.h"
#include "data_view.h"
#include "kinds.h"
#include "zone_rules.h"

#include <string.h>

enum
{
    maxOffsetMinutes = 23 * 60 + 59,
};

static const uint8_t s_minusSign[3] = {0xE2, 0x88, 0x92};

static bool ValidInstant(const mlocInstant* i)
{
    return i->nanoseconds >= 0 && i->nanoseconds <= 999999999 && i->seconds >= zoneMinSeconds &&
           (i->seconds < zoneMaxSeconds || (i->seconds == zoneMaxSeconds && i->nanoseconds == 0));
}

// A zone's tables when it is named, after checking that the library made
// it: an offset zone has no unit and an offset within a day, a named one
// an identifier of its unit and no offset.
static bool ReadZone(const mlocTimeZone* zone, ZoneTables* t, bool* namedOut)
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

static bool TwoDigits(const char* p, int limit, int* out)
{
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9')
    {
        return false;
    }
    *out = (p[0] - '0') * 10 + (p[1] - '0');
    return *out <= limit;
}

// An offset: a sign, two digits of hours, then optionally two of minutes
// after a colon or not.
static bool ParseOffset(const char* id, size_t length, int16_t* minutesOut)
{
    size_t sign = length >= 3 && memcmp(id, s_minusSign, 3) == 0 ? 3 : 1;
    bool negative = sign == 3 || id[0] == '-';
    const char* rest = id + sign;
    size_t restLength = length - sign;
    int hours = 0;
    int minutes = 0;
    bool colon = restLength == 5 && rest[2] == ':';
    if ((restLength != 2 && restLength != 4 && !colon) || !TwoDigits(rest, 23, &hours) ||
        (restLength > 2 && !TwoDigits(rest + (colon ? 3 : 2), 59, &minutes)))
    {
        return false;
    }
    int total = hours * 60 + minutes;
    *minutesOut = (int16_t)(negative ? -total : total);
    return true;
}

mlocResult mlocFindTimeZone(const mlocDataSet* data, const char* id, size_t length,
                            mlocTimeZone* zoneOut)
{
    if ((id == nullptr && length > 0) || zoneOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    bool offset = length > 0 && (id[0] == '+' || id[0] == '-' ||
                                 (length >= 3 && memcmp(id, s_minusSign, 3) == 0));
    if (offset)
    {
        int16_t minutes = 0;
        if (!ParseOffset(id, length, &minutes))
        {
            return mloc_errorSyntax;
        }
        *zoneOut = (mlocTimeZone){.offsetMinutes = minutes};
        return mloc_success;
    }
    DataSpan span;
    ZoneTables t;
    if (data == nullptr || !mlocFindDataSetUnit(data, kindZones, nullptr, 0, &span) ||
        !mlocZoneTablesOf(span, &t))
    {
        return mloc_errorNotFound;
    }
    int32_t identifier = mlocZoneFindIdentifier(&t, id, length);
    if (identifier < 0)
    {
        return mloc_errorNotFound;
    }
    *zoneOut = (mlocTimeZone){
        .zones = span.bytes, .zonesSize = span.size, .identifier = (uint16_t)identifier};
    return mloc_success;
}

// Writes a text, or "+HH:MM" for minutes when text is null.
static mlocResult Write(const char* text, size_t length, int minutes, mlocOutput* output)
{
    char offset[6];
    if (text == nullptr)
    {
        int magnitude = minutes < 0 ? -minutes : minutes;
        offset[0] = minutes < 0 ? '-' : '+';
        offset[1] = (char)('0' + magnitude / 600);
        offset[2] = (char)('0' + magnitude / 60 % 10);
        offset[3] = ':';
        offset[4] = (char)('0' + magnitude % 60 / 10);
        offset[5] = (char)('0' + magnitude % 10);
        text = offset;
        length = sizeof(offset);
    }
    output->length = length;
    if (length > output->capacity)
    {
        return mloc_errorCapacity;
    }
    if (length > 0)
    {
        memcpy(output->buffer, text, length);
    }
    return mloc_success;
}

static mlocResult FormatId(const mlocTimeZone* zone, bool primary, mlocOutput* output)
{
    ZoneTables t;
    bool named = false;
    if (output == nullptr || (output->buffer == nullptr && output->capacity > 0) ||
        !ReadZone(zone, &t, &named))
    {
        return mloc_errorInvalid;
    }
    if (!named)
    {
        return Write(nullptr, 0, zone->offsetMinutes, output);
    }
    uint32_t identifier =
        primary ? mlocZoneIdentifierPrimary(&t, zone->identifier) : zone->identifier;
    size_t length = 0;
    const char* name = mlocZoneIdentifierName(&t, identifier, &length);
    return Write(name, length, 0, output);
}

mlocResult mlocFormatTimeZoneId(const mlocTimeZone* zone, mlocOutput* output)
{
    return FormatId(zone, false, output);
}

mlocResult mlocFormatPrimaryTimeZoneId(const mlocTimeZone* zone, mlocOutput* output)
{
    return FormatId(zone, true, output);
}

mlocResult mlocTimeZonesEqual(const mlocTimeZone* one, const mlocTimeZone* two, bool* equalOut)
{
    ZoneTables a;
    ZoneTables b;
    bool namedOne = false;
    bool namedTwo = false;
    if (equalOut == nullptr || !ReadZone(one, &a, &namedOne) || !ReadZone(two, &b, &namedTwo))
    {
        return mloc_errorInvalid;
    }
    if (!namedOne || !namedTwo)
    {
        *equalOut = !namedOne && !namedTwo && one->offsetMinutes == two->offsetMinutes;
        return mloc_success;
    }
    // By the primary identifiers' names, so zones of two units compare
    // too.
    size_t lengthOne = 0;
    size_t lengthTwo = 0;
    const char* nameOne =
        mlocZoneIdentifierName(&a, mlocZoneIdentifierPrimary(&a, one->identifier), &lengthOne);
    const char* nameTwo =
        mlocZoneIdentifierName(&b, mlocZoneIdentifierPrimary(&b, two->identifier), &lengthTwo);
    *equalOut = lengthOne == lengthTwo && memcmp(nameOne, nameTwo, lengthOne) == 0;
    return mloc_success;
}

size_t mlocGetTimeZoneCount(const mlocDataSet* data)
{
    ZoneTables t;
    return data != nullptr && mlocFindZoneTables(data, &t) ? t.primaryCount : 0;
}

mlocResult mlocGetTimeZone(const mlocDataSet* data, size_t index, mlocTimeZone* zoneOut)
{
    DataSpan span;
    ZoneTables t;
    if (data == nullptr || zoneOut == nullptr ||
        !mlocFindDataSetUnit(data, kindZones, nullptr, 0, &span) || !mlocZoneTablesOf(span, &t) ||
        index >= t.primaryCount)
    {
        return mloc_errorInvalid;
    }
    *zoneOut = (mlocTimeZone){.zones = span.bytes,
                              .zonesSize = span.size,
                              .identifier = mlocLoad16(t.primaries + index * 2)};
    return mloc_success;
}

mlocResult mlocGetTimeZoneOffset(const mlocTimeZone* zone, const mlocInstant* instant,
                                 int32_t* offsetSecondsOut)
{
    ZoneTables t;
    bool named = false;
    if (instant == nullptr || offsetSecondsOut == nullptr || !ReadZone(zone, &t, &named) ||
        !ValidInstant(instant))
    {
        return mloc_errorInvalid;
    }
    if (!named)
    {
        *offsetSecondsOut = zone->offsetMinutes * 60;
        return mloc_success;
    }
    ZoneOffset o =
        mlocZoneOffsetAt(&t, mlocZoneIdentifierZone(&t, zone->identifier), instant->seconds);
    *offsetSecondsOut = o.standard + o.save;
    return mloc_success;
}

// The next or previous change of the offset (standard plus saving) from
// a time: the zone's transitions that keep the sum are passed over. In
// the closing rule's years the changes alternate, so a rule whose two
// offsets have one sum has none; the count bounds the walk.
static bool OffsetChange(const ZoneTables* t, uint32_t zone, int64_t seconds, bool next,
                         int64_t* atOut)
{
    ZoneRecord z = mlocZoneRecord(t, zone);
    int64_t at = seconds;
    ZoneOffset after = {0};
    for (uint32_t steps = 0; steps <= (uint32_t)z.transitionCount + 4; steps++)
    {
        bool found = next ? mlocZoneNextTransition(t, zone, at, &at, &after)
                          : mlocZonePreviousTransition(t, zone, at, &at, &after);
        if (!found)
        {
            return false;
        }
        ZoneOffset before = mlocZoneOffsetAt(t, zone, at - 1);
        if (before.standard + before.save != after.standard + after.save)
        {
            *atOut = at;
            return true;
        }
        at = next ? at : at - 1;
    }
    return false;
}

mlocResult mlocGetTimeZoneTransition(const mlocTimeZone* zone, const mlocInstant* instant,
                                     mlocTransitionDirection direction, mlocInstant* transitionOut)
{
    ZoneTables t;
    bool named = false;
    if (instant == nullptr || transitionOut == nullptr || !ReadZone(zone, &t, &named) ||
        !ValidInstant(instant) || (uint32_t)direction > mloc_transitionPrevious)
    {
        return mloc_errorInvalid;
    }
    if (!named)
    {
        return mloc_errorNotFound;
    }
    // Transitions fall on whole seconds: strictly before an instant with
    // nanoseconds is at or before its second, without them before it.
    bool next = direction == mloc_transitionNext;
    int64_t from = next || instant->nanoseconds > 0 ? instant->seconds : instant->seconds - 1;
    int64_t at = 0;
    if (!OffsetChange(&t, mlocZoneIdentifierZone(&t, zone->identifier), from, next, &at))
    {
        return mloc_errorNotFound;
    }
    *transitionOut = (mlocInstant){.seconds = at};
    return mloc_success;
}
