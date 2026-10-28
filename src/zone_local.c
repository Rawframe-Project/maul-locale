// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Local times in a zone (maul-locale/zone.h): the instants a date-time
// names, the one a disambiguation chooses, the start of a day, and the
// date-time at an instant.

#include "zone_local.h"

#include "date_time.h"
#include "invariant.h"
#include "zone_handle.h"
#include "zone_rules.h"

#include "maul-locale/zone.h"

// What a local time's seconds name in a zone: the instants (seconds; the
// nanoseconds pass through), and for a gap the change's length and the
// transition that ends it.
typedef struct Local
{
    uint32_t count;
    int64_t earliest;
    int64_t latest;
    int32_t gapLength;
    int64_t gapEnd;
} Local;

static int32_t Sum(ZoneOffset o)
{
    return o.standard + o.save;
}

// An instant t names the local time when t plus the offset in effect at
// t is it. Offsets are under a day, so every such t lies within a day
// of the local time: the zone's spans of one offset over those two days
// are walked, each giving the local time minus its offset when that
// falls inside the span.
static Local Possible(const mlocTimeZone* zone, const ZoneTables* t, bool named, int64_t local)
{
    Local r = {0};
    if (!named)
    {
        r.count = 1;
        r.earliest = r.latest = local - (int64_t)zone->offsetMinutes * 60;
        return r;
    }
    uint32_t z = mlocZoneIdentifierZone(t, zone->identifier);
    int64_t start = local - secondsPerDay;
    int32_t offset = Sum(mlocZoneOffsetAt(t, z, start));
    for (;;)
    {
        int64_t end = 0;
        ZoneOffset next = {0};
        bool more = mlocZoneFollowingTransition(t, z, start, &end, &next);
        int64_t candidate = local - offset;
        if (candidate >= start && (!more || candidate < end))
        {
            r.earliest = r.count == 0 ? candidate : r.earliest;
            r.latest = candidate;
            r.count += 1;
        }
        if (!more || end > local + secondsPerDay)
        {
            // The spans come in order and each candidate lies in its own.
            MLOC_ASSERT(r.count == 0 || r.earliest <= r.latest);
            return r;
        }
        int32_t after = Sum(next);
        if (local >= end + offset && local < end + after)
        {
            r.gapLength = after - offset;
            r.gapEnd = end;
        }
        start = end;
        offset = after;
    }
}

// The instants found as mlocInstant values, false when either end lies
// outside Temporal's range (ECMA-262 refuses the whole list then).
static bool Instants(const Local* r, uint32_t nanosecond, mlocPossibleInstants* out)
{
    mlocPossibleInstants p = {.count = r->count};
    if (r->count > 0)
    {
        p.earliest = (mlocInstant){r->earliest, (int32_t)nanosecond};
        p.latest = (mlocInstant){r->latest, (int32_t)nanosecond};
        if (!mlocValidInstant(&p.earliest) || !mlocValidInstant(&p.latest))
        {
            return false;
        }
    }
    *out = p;
    return true;
}

mlocResult mlocGetPossibleInstants(const mlocTimeZone* zone, const mlocDateTime* dateTime,
                                   mlocPossibleInstants* instantsOut)
{
    ZoneTables t;
    bool named = false;
    if (dateTime == nullptr || instantsOut == nullptr || !mlocReadTimeZone(zone, &t, &named) ||
        !mlocDateTimeValid(dateTime))
    {
        return mloc_errorInvalid;
    }
    Local r = Possible(zone, &t, named, mlocDateTimeSeconds(dateTime));
    return Instants(&r, dateTime->time.nanosecond, instantsOut) ? mloc_success : mloc_errorInvalid;
}

mlocResult mlocGetInstantFor(const mlocTimeZone* zone, const mlocDateTime* dateTime,
                             mlocDisambiguation disambiguation, mlocInstant* instantOut)
{
    ZoneTables t;
    bool named = false;
    if (dateTime == nullptr || instantOut == nullptr || !mlocReadTimeZone(zone, &t, &named) ||
        !mlocDateTimeValid(dateTime) || (uint32_t)disambiguation > mloc_disambiguationReject)
    {
        return mloc_errorInvalid;
    }
    int64_t local = mlocDateTimeSeconds(dateTime);
    Local r = Possible(zone, &t, named, local);
    bool earlier = disambiguation == mloc_disambiguationEarlier;
    bool gap = r.count == 0;
    if (r.count != 1 && disambiguation == mloc_disambiguationReject)
    {
        return mloc_errorInvalid;
    }
    if (gap)
    {
        // DisambiguatePossibleEpochNanoseconds: the time moved by the
        // gap's length, back for earlier and forward otherwise, then that
        // time's earliest instant for earlier and latest otherwise.
        r = Possible(zone, &t, named, earlier ? local - r.gapLength : local + r.gapLength);
    }
    mlocPossibleInstants p;
    if (r.count == 0 || !Instants(&r, dateTime->time.nanosecond, &p))
    {
        return mloc_errorInvalid;
    }
    bool latest = disambiguation == mloc_disambiguationLater || (gap && !earlier);
    *instantOut = latest ? p.latest : p.earliest;
    return mloc_success;
}

mlocResult mlocGetStartOfDay(const mlocTimeZone* zone, const mlocDate* date,
                             mlocInstant* instantOut)
{
    ZoneTables t;
    bool named = false;
    mlocDateTime midnight = {0};
    if (date == nullptr || instantOut == nullptr || !mlocReadTimeZone(zone, &t, &named))
    {
        return mloc_errorInvalid;
    }
    midnight.date = *date;
    if (!mlocDateTimeValid(&midnight))
    {
        return mloc_errorInvalid;
    }
    Local r = Possible(zone, &t, named, mlocDateTimeSeconds(&midnight));
    mlocPossibleInstants p;
    if (r.count == 0)
    {
        // A change skips midnight: the day starts where the gap ends.
        r = (Local){.count = 1, .earliest = r.gapEnd, .latest = r.gapEnd};
    }
    if (!Instants(&r, 0, &p))
    {
        return mloc_errorInvalid;
    }
    *instantOut = p.earliest;
    return mloc_success;
}

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b < 0 ? 1 : 0);
}

mlocResult mlocGetDateTimeFor(const mlocTimeZone* zone, const mlocInstant* instant,
                              mlocCalendar calendar, mlocDateTime* dateTimeOut)
{
    ZoneTables t;
    bool named = false;
    if (instant == nullptr || dateTimeOut == nullptr || !mlocReadTimeZone(zone, &t, &named) ||
        !mlocValidInstant(instant))
    {
        return mloc_errorInvalid;
    }
    return mlocLocalDateTime(instant, mlocTimeZoneOffsetAt(zone, &t, named, instant->seconds),
                             calendar, dateTimeOut);
}

mlocResult mlocLocalDateTime(const mlocInstant* instant, int32_t offset, mlocCalendar calendar,
                             mlocDateTime* dateTimeOut)
{
    int64_t local = instant->seconds + offset;
    int64_t days = FloorDiv(local, secondsPerDay);
    int64_t second = local - days * secondsPerDay;
    mlocDate date;
    if (mlocMakeDateFromDay(calendar, (int32_t)(days + epochDay), &date) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    *dateTimeOut = (mlocDateTime){.date = date,
                                  .time = {.hour = (uint8_t)(second / 3600),
                                           .minute = (uint8_t)(second / 60 % 60),
                                           .second = (uint8_t)(second % 60),
                                           .nanosecond = (uint32_t)instant->nanoseconds}};
    return mloc_success;
}
