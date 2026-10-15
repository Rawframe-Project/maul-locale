// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The compiled zones (record mloc-0041): offsets at known times from
// zdump on zic's output, the daylight periods of tzdb's rearguard form
// where the source has negative savings, times outside int32_t at both
// ends of a table, the closing rule to the end of Temporal's range, and
// every transition from 1800 to 2100 seen as a change of offset.
// tools/check_zones.py compares every zone with zdump in CI.

#include "test_harness.h"
#include "zone_rules.h"

#include <string.h>

static uint32_t Zone(const char* name)
{
    int32_t zone = mlocZoneFind(name, strlen(name));
    CHECK(zone >= 0, name);
    return zone < 0 ? 0 : (uint32_t)zone;
}

static bool Is(ZoneOffset o, int32_t standard, int32_t save)
{
    return o.standard == standard && o.save == save;
}

static void TestFind(void)
{
    CHECK(mlocZoneFind("America/New_York", 16) >= 0 &&
              strcmp(mlocZoneName(Zone("America/New_York")), "America/New_York") == 0,
          "a zone by name");
    CHECK(mlocZoneFind("America/New_Yor", 15) < 0 && mlocZoneFind("america/new_york", 16) < 0 &&
              mlocZoneFind("US/Eastern", 10) < 0 && mlocZoneFind("Factory", 7) < 0,
          "a prefix, another case, a link and Factory are not zones here");
    CHECK(mlocZoneFind(mlocZoneName(0), strlen(mlocZoneName(0))) == 0 &&
              mlocZoneFind(mlocZoneName(mlocZoneCount - 1),
                           strlen(mlocZoneName(mlocZoneCount - 1))) == mlocZoneCount - 1,
          "the first and last zones");
    CHECK(strlen(mlocZoneRelease) == 5, "the release");
}

static void TestKnownTimes(void)
{
    uint32_t newYork = Zone("America/New_York");
    // Local mean time until November 18 of 1883 at 17:00, before int32_t's
    // range.
    CHECK(Is(mlocZoneOffsetAt(newYork, INT64_C(-2717650801)), -17762, 0) &&
              Is(mlocZoneOffsetAt(newYork, INT64_C(-2717650800)), -18000, 0) &&
              Is(mlocZoneOffsetAt(newYork, zoneMinSeconds), -17762, 0),
          "New York's local mean time");
    CHECK(Is(mlocZoneOffsetAt(newYork, 1710053999), -18000, 0) &&
              Is(mlocZoneOffsetAt(newYork, 1710054000), -18000, 3600) &&
              Is(mlocZoneOffsetAt(newYork, 1730613599), -18000, 3600) &&
              Is(mlocZoneOffsetAt(newYork, 1730613600), -18000, 0),
          "New York in 2024");
    // The closing rule near the end of the range: March 12 of 275758.
    CHECK(Is(mlocZoneOffsetAt(newYork, INT64_C(8639920882799)), -18000, 0) &&
              Is(mlocZoneOffsetAt(newYork, INT64_C(8639920882800)), -18000, 3600),
          "New York in 275758");
    uint32_t gaza = Zone("Asia/Gaza");
    int64_t at = 0;
    ZoneOffset o = {0};
    CHECK(mlocZoneNextTransition(gaza, INT64_C(2208988800), &at, &o) && at == INT64_C(2216160000) &&
              Is(o, 7200, 3600),
          "Gaza in 2040, after int32_t's range");
}

// Where tzdb's source has negative savings, the rearguard periods.
static void TestRearguard(void)
{
    uint32_t dublin = Zone("Europe/Dublin");
    CHECK(Is(mlocZoneOffsetAt(dublin, 1751328000), 0, 3600) &&
              Is(mlocZoneOffsetAt(dublin, 1735689600), 0, 0) &&
              Is(mlocZoneOffsetAt(dublin, -15897600), 3600, 0) &&
              Is(mlocZoneOffsetAt(dublin, 1509238799), 0, 3600) &&
              Is(mlocZoneOffsetAt(dublin, 1509238800), 0, 0),
          "Dublin: summer is daylight time, and 1968 to 1971 standard");
    uint32_t windhoek = Zone("Africa/Windhoek");
    CHECK(Is(mlocZoneOffsetAt(windhoek, 1467331200), 3600, 0) &&
              Is(mlocZoneOffsetAt(windhoek, 1504400400), 3600, 3600) &&
              Is(mlocZoneOffsetAt(windhoek, 1508795999), 3600, 3600) &&
              Is(mlocZoneOffsetAt(windhoek, 1508796000), 7200, 0),
          "Windhoek: daylight time until 2017-10-24, then standard");
    uint32_t casablanca = Zone("Africa/Casablanca");
    CHECK(Is(mlocZoneOffsetAt(casablanca, 1751328000), 0, 3600) &&
              Is(mlocZoneOffsetAt(casablanca, 1740787200), 0, 0) &&
              Is(mlocZoneOffsetAt(casablanca, 1790812800), 0, 0),
          "Casablanca: +01 as daylight time outside Ramadan, until 2026-09-20");
}

// Each transition changes the offset, and nothing changes between two.
static void TestTransitions(void)
{
    bool all = true;
    long count = 0;
    for (uint32_t z = 0; z < mlocZoneCount; z++)
    {
        int64_t at = INT64_C(-5364662400);
        ZoneOffset before = mlocZoneOffsetAt(z, at);
        ZoneOffset o = {0};
        while (mlocZoneNextTransition(z, at, &at, &o) && at < INT64_C(4102444800))
        {
            ZoneOffset previous = mlocZoneOffsetAt(z, at - 1);
            all = all && Is(previous, before.standard, before.save) &&
                  !Is(o, before.standard, before.save) &&
                  Is(mlocZoneOffsetAt(z, at), o.standard, o.save);
            before = o;
            count++;
        }
    }
    printf("%ld transitions from 1800 to 2100\n", count);
    CHECK(all, "every transition changes the offset, and the offset holds between");
}

static void TestRangeEnd(void)
{
    uint32_t newYork = Zone("America/New_York");
    uint32_t utc = Zone("Etc/UTC");
    int64_t at = 0;
    ZoneOffset o = {0};
    CHECK(!mlocZoneNextTransition(newYork, zoneMaxSeconds, &at, &o) &&
              !mlocZoneNextTransition(utc, zoneMinSeconds, &at, &o) &&
              Is(mlocZoneOffsetAt(utc, zoneMaxSeconds), 0, 0),
          "no transition past the range's end, nor in UTC");
    CHECK(mlocZoneNextTransition(newYork, zoneMaxSeconds - INT64_C(31622400), &at, &o) &&
              at <= zoneMaxSeconds,
          "the last year's transitions");
}

int main(void)
{
    TestFind();
    TestKnownTimes();
    TestRearguard();
    TestTransitions();
    TestRangeEnd();
    return s_failures == 0 ? 0 : 1;
}
