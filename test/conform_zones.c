// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Prints every zone's offsets from the compiled-in zones unit over a span of
// years, for tools/check_zones.py to compare with zdump on zic's output
// for the same tzdb source: "zone NAME STANDARD SAVE" with the offset at
// the span's start, then "at SECONDS STANDARD SAVE" for each transition
// in the span.
//
// usage: conform_zones <first year> <end year>

#include "calendar_arith.h"
#include "zone_rules.h"

#include "maul-locale/data.h"

#include <stdio.h>
#include <stdlib.h>

static int64_t YearStart(int64_t year)
{
    // Day 719163 of the library's count is January 1 of 1970.
    return (mlocArithmeticToDay(mloc_calendarGregorian, year, 1, 1) - 719163) * 86400;
}

int main(int argc, char** argv)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet data = {&builtin, 1};
    ZoneTables t;
    if (argc != 3 || !mlocFindZoneTables(&data, &t))
    {
        fputs("usage: conform_zones <first year> <end year>\n", stderr);
        return 2;
    }
    int64_t from = YearStart(atoi(argv[1]));
    int64_t end = YearStart(atoi(argv[2]));
    from = from < zoneMinSeconds ? zoneMinSeconds : from;
    for (uint32_t z = 0; z < t.zoneCount; z++)
    {
        size_t length = 0;
        const char* name = mlocZoneIdentifierName(&t, mlocZoneRecord(&t, z).name, &length);
        ZoneOffset o = mlocZoneOffsetAt(&t, z, from);
        printf("zone %.*s %d %d\n", (int)length, name, (int)o.standard, (int)o.save);
        int64_t at = from;
        while (mlocZoneNextTransition(&t, z, at, &at, &o) && at < end)
        {
            printf("at %lld %d %d\n", (long long)at, (int)o.standard, (int)o.save);
        }
    }
    return 0;
}
