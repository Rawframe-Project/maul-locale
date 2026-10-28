// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone's offsets read from the compiled tables (zone_rules.h): a binary
// search in the table, then the closing rule's changes counted on the
// Gregorian calendar for the years around the time.

#include "zone_rules.h"

#include "bytes.h"
#include "calendar_arith.h"
#include "invariant.h"

enum
{
    secondsPerDay = 86400,
    // The day of January 1 of 1970 in maul-locale/calendar.h's count.
    epochDay = 719163,
};

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b != 0 && (a < 0) != (b < 0));
}

static int64_t TimeOf(const ZoneTables* t, const ZoneRecord* z, uint32_t i)
{
    MLOC_ASSERT(i < z->transitionCount);
    uint32_t late = (uint32_t)z->transitionCount - z->wideLate;
    if (i < z->wideEarly || i >= late)
    {
        uint32_t wide = z->wideFirst + (i < z->wideEarly ? i : z->wideEarly + i - late);
        return (int64_t)mlocLoad64(t->wide + (size_t)wide * 8);
    }
    return (int32_t)mlocLoad32(t->narrow + (size_t)(z->narrowFirst + i - z->wideEarly) * 4);
}

static ZoneOffset OffsetOf(const ZoneTables* t, const ZoneRecord* z, uint32_t place)
{
    return mlocZoneOffsetOf(t, mlocLoad16(t->list + (size_t)(z->offsetFirst + place) * 2));
}

static ZoneOffset TypeOf(const ZoneTables* t, const ZoneRecord* z, uint32_t i)
{
    MLOC_ASSERT(i < z->transitionCount);
    return OffsetOf(t, z, t->types[z->typeFirst + i]);
}

// The number of transitions at or before a time.
static uint32_t CountUpTo(const ZoneTables* t, const ZoneRecord* z, int64_t seconds)
{
    uint32_t low = 0;
    uint32_t high = z->transitionCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (TimeOf(t, z, middle) <= seconds)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

static int32_t YearOf(int64_t seconds)
{
    int64_t day = FloorDiv(seconds, secondsPerDay) + epochDay;
    return mlocArithmeticFromDay(mloc_calendarGregorian, (int32_t)day).year;
}

// A change's time in a year: its day as zic finds it (a last weekday
// counts back from the month's last day), then its universal time.
static int64_t ChangeTime(const ZoneChange* c, int32_t year)
{
    MLOC_ASSERT(c->month >= 1 && c->month <= 12);
    uint32_t length = mlocArithmeticDaysInMonth(mloc_calendarGregorian, year, c->month);
    uint32_t dayOfMonth = c->dayOfMonth < length ? c->dayOfMonth : length;
    int64_t day = mlocArithmeticToDay(mloc_calendarGregorian, year, c->month, dayOfMonth);
    if (c->day != zoneDayOfMonth)
    {
        // Day 0 of the count is a Sunday.
        int64_t weekday = (day % 7 + 7) % 7;
        int64_t step = (c->weekday - weekday + 7) % 7;
        day += c->day == zoneOnOrAfter ? step : (step == 0 ? 0 : step - 7);
    }
    return (day - epochDay) * secondsPerDay + c->time;
}

static int Folded(char c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : (unsigned char)c;
}

// The order of an identifier's name and a name, letters folded.
static int Order(const char* candidate, size_t candidateLength, const char* name, size_t length)
{
    for (size_t i = 0; i < candidateLength && i < length; i++)
    {
        int difference = Folded(candidate[i]) - Folded(name[i]);
        if (difference != 0)
        {
            return difference;
        }
    }
    return (candidateLength > length) - (candidateLength < length);
}

int32_t mlocZoneFindIdentifier(const ZoneTables* t, const char* name, size_t length)
{
    uint32_t low = 0;
    uint32_t high = t->identifierCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        size_t candidateLength = 0;
        const char* candidate = mlocZoneIdentifierName(t, middle, &candidateLength);
        int order = Order(candidate, candidateLength, name, length);
        if (order == 0)
        {
            return (int32_t)middle;
        }
        if (order < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return -1;
}

// The last transition at or before a time, the table's or the closing
// rule's: false when the time is before the first.
static bool Latest(const ZoneTables* t, const ZoneRecord* z, int64_t seconds, int64_t* atOut,
                   ZoneOffset* offsetOut)
{
    uint32_t count = CountUpTo(t, z, seconds);
    int64_t at = count == 0 ? INT64_MIN : TimeOf(t, z, count - 1);
    ZoneOffset offset = count == 0 ? (ZoneOffset){0} : TypeOf(t, z, count - 1);
    if (count == z->transitionCount && z->closing != 0)
    {
        // Past the table: the latest change at or before the time, of its
        // year or the year before. The changes come in time order (within
        // a year by month), so the first one at or before the time,
        // counting back, is the latest.
        int32_t first = mlocZoneClosingYear(t, z->closing - 1U);
        int32_t year = YearOf(seconds);
        for (uint32_t i = 0; i < 4 && year - (int32_t)(i / 2) >= first; i++)
        {
            ZoneChange c = mlocZoneClosingChange(t, z->closing - 1U, 1 - i % 2);
            int64_t candidate = ChangeTime(&c, year - (int32_t)(i / 2));
            if (candidate <= seconds)
            {
                if (candidate > at)
                {
                    at = candidate;
                    offset = mlocZoneOffsetOf(t, c.offset);
                }
                break;
            }
        }
    }
    MLOC_ASSERT(at <= seconds);
    *atOut = at;
    *offsetOut = offset;
    return at != INT64_MIN;
}

ZoneOffset mlocZoneOffsetAt(const ZoneTables* t, uint32_t zone, int64_t seconds)
{
    ZoneRecord z = mlocZoneRecord(t, zone);
    int64_t at = 0;
    ZoneOffset offset = {0};
    return Latest(t, &z, seconds, &at, &offset) ? offset : OffsetOf(t, &z, z.initial);
}

bool mlocZoneFollowingTransition(const ZoneTables* t, uint32_t zone, int64_t seconds,
                                 int64_t* atOut, ZoneOffset* offsetOut)
{
    ZoneRecord z = mlocZoneRecord(t, zone);
    uint32_t count = CountUpTo(t, &z, seconds);
    int64_t at = 0;
    ZoneOffset offset = {0};
    if (count < z.transitionCount)
    {
        at = TimeOf(t, &z, count);
        offset = TypeOf(t, &z, count);
    }
    else if (z.closing != 0)
    {
        // The changes come in time order: within a year by month, and
        // the years in turn; the time's year and the next hold the next
        // one.
        int32_t first = mlocZoneClosingYear(t, z.closing - 1U);
        int32_t year = YearOf(seconds);
        year = year < first ? first : year;
        at = INT64_MAX;
        for (uint32_t i = 0; at == INT64_MAX && i < 4; i++)
        {
            ZoneChange c = mlocZoneClosingChange(t, z.closing - 1U, i % 2);
            int64_t candidate = ChangeTime(&c, year + (int32_t)(i / 2));
            if (candidate > seconds)
            {
                at = candidate;
                offset = mlocZoneOffsetOf(t, c.offset);
            }
        }
        // The validator keeps a rule's changes from February to November,
        // within a week of their day, so the next year's come after any
        // time of this one.
        MLOC_ASSERT(at != INT64_MAX);
    }
    else
    {
        return false;
    }
    *atOut = at;
    *offsetOut = offset;
    return true;
}

bool mlocZoneNextTransition(const ZoneTables* t, uint32_t zone, int64_t seconds, int64_t* atOut,
                            ZoneOffset* offsetOut)
{
    int64_t at = 0;
    ZoneOffset offset = {0};
    if (!mlocZoneFollowingTransition(t, zone, seconds, &at, &offset) || at > zoneMaxSeconds)
    {
        return false;
    }
    *atOut = at;
    *offsetOut = offset;
    return true;
}

bool mlocZonePreviousTransition(const ZoneTables* t, uint32_t zone, int64_t seconds, int64_t* atOut,
                                ZoneOffset* offsetOut)
{
    ZoneRecord z = mlocZoneRecord(t, zone);
    int64_t at = 0;
    ZoneOffset offset = {0};
    if (!Latest(t, &z, seconds, &at, &offset) || at < zoneMinSeconds)
    {
        return false;
    }
    *atOut = at;
    *offsetOut = offset;
    return true;
}
