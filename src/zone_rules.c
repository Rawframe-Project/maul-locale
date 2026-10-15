// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone's offsets read from the compiled tables (zone_rules.h): a binary
// search in the table, then the closing rule's changes counted on the
// Gregorian calendar for the years around the time.

#include "zone_rules.h"

#include "calendar_arith.h"

#include <string.h>

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

static int64_t TimeOf(const ZoneRecord* z, uint32_t i)
{
    uint32_t late = (uint32_t)z->transitionCount - z->wideLate;
    if (i < z->wideEarly)
    {
        return mlocZoneWideTimes[z->wideFirst + i];
    }
    if (i >= late)
    {
        return mlocZoneWideTimes[z->wideFirst + z->wideEarly + (i - late)];
    }
    return mlocZoneTimes[z->narrowFirst + i - z->wideEarly];
}

static ZoneOffset OffsetOf(const ZoneRecord* z, uint32_t place)
{
    return mlocZoneOffsets[mlocZoneOffsetList[z->offsetFirst + place]];
}

static ZoneOffset TypeOf(const ZoneRecord* z, uint32_t i)
{
    return OffsetOf(z, mlocZoneTypes[z->typeFirst + i]);
}

// The number of transitions at or before a time.
static uint32_t CountUpTo(const ZoneRecord* z, int64_t seconds)
{
    uint32_t low = 0;
    uint32_t high = z->transitionCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        if (TimeOf(z, middle) <= seconds)
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

int32_t mlocZoneFind(const char* name, size_t length)
{
    uint32_t low = 0;
    uint32_t high = mlocZoneCount;
    while (low < high)
    {
        uint32_t middle = low + (high - low) / 2;
        const char* candidate = mlocZoneNameText + mlocZones[middle].name;
        size_t candidateLength = strlen(candidate);
        size_t common = candidateLength < length ? candidateLength : length;
        int order = memcmp(candidate, name, common);
        if (order == 0)
        {
            order = (candidateLength > length) - (candidateLength < length);
        }
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

const char* mlocZoneName(uint32_t zone)
{
    return mlocZoneNameText + mlocZones[zone].name;
}

ZoneOffset mlocZoneOffsetAt(uint32_t zone, int64_t seconds)
{
    const ZoneRecord* z = &mlocZones[zone];
    uint32_t count = CountUpTo(z, seconds);
    ZoneOffset offset = count == 0 ? OffsetOf(z, z->initial) : TypeOf(z, count - 1);
    if (count < z->transitionCount || z->closing == 0)
    {
        return offset;
    }
    // Past the table: the latest change at or before the time, among the
    // years either side of the time's year.
    const ZoneClosing* c = &mlocZoneClosings[z->closing - 1];
    int64_t latest = count == 0 ? INT64_MIN : TimeOf(z, count - 1);
    int32_t year = YearOf(seconds);
    for (int32_t y = year - 1; y <= year + 1; y++)
    {
        for (int k = 0; y >= c->firstYear && k < 2; k++)
        {
            int64_t at = ChangeTime(&c->changes[k], y);
            if (at <= seconds && at > latest)
            {
                latest = at;
                offset = mlocZoneOffsets[c->changes[k].offset];
            }
        }
    }
    return offset;
}

bool mlocZoneNextTransition(uint32_t zone, int64_t seconds, int64_t* atOut, ZoneOffset* offsetOut)
{
    const ZoneRecord* z = &mlocZones[zone];
    uint32_t count = CountUpTo(z, seconds);
    int64_t at = 0;
    ZoneOffset offset = {0};
    if (count < z->transitionCount)
    {
        at = TimeOf(z, count);
        offset = TypeOf(z, count);
    }
    else if (z->closing != 0)
    {
        // The changes come in time order: within a year by month, and
        // the years in turn; three years always hold the next one.
        const ZoneClosing* c = &mlocZoneClosings[z->closing - 1];
        int32_t year = YearOf(seconds) - 1;
        year = year < c->firstYear ? c->firstYear : year;
        at = INT64_MAX;
        for (int i = 0; at == INT64_MAX && i < 6; i++)
        {
            int64_t candidate = ChangeTime(&c->changes[i % 2], year + i / 2);
            if (candidate > seconds)
            {
                at = candidate;
                offset = mlocZoneOffsets[c->changes[i % 2].offset];
            }
        }
    }
    else
    {
        return false;
    }
    if (at > zoneMaxSeconds)
    {
        return false;
    }
    *atOut = at;
    *offsetOut = offset;
    return true;
}
