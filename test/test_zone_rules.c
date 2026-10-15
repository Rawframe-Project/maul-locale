// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The compiled zones (record mloc-0041): offsets at known times from
// zdump on zic's output, the daylight periods of tzdb's rearguard form
// where the source has negative savings, times outside int32_t at both
// ends of a table, the closing rule to the end of Temporal's range, and
// every transition from 1800 to 2100 seen as a change of offset.
// tools/check_zones.py compares every zone with zdump in CI.

#include "data_format.h"
#include "data_view.h"
#include "kinds.h"
#include "test_harness.h"
#include "zone_rules.h"

#include "maul-locale/data.h"

#include <string.h>

static ZoneTables s_tables;
static mlocData s_builtin;

static int32_t Identifier(const char* name)
{
    return mlocZoneFindIdentifier(&s_tables, name, strlen(name));
}

static uint32_t Zone(const char* name)
{
    int32_t identifier = Identifier(name);
    CHECK(identifier >= 0, name);
    return identifier < 0 ? 0 : mlocZoneIdentifierZone(&s_tables, (uint32_t)identifier);
}

static ZoneOffset OffsetAt(uint32_t zone, int64_t seconds)
{
    return mlocZoneOffsetAt(&s_tables, zone, seconds);
}

static bool Next(uint32_t zone, int64_t seconds, int64_t* at, ZoneOffset* o)
{
    return mlocZoneNextTransition(&s_tables, zone, seconds, at, o);
}

static bool Is(ZoneOffset o, int32_t standard, int32_t save)
{
    return o.standard == standard && o.save == save;
}

// Whether a name's primary identifier is the given one.
static bool PrimaryIs(const char* name, const char* primary)
{
    int32_t identifier = Identifier(name);
    if (identifier < 0)
    {
        return false;
    }
    size_t length = 0;
    const char* found = mlocZoneIdentifierName(
        &s_tables, mlocZoneIdentifierPrimary(&s_tables, (uint32_t)identifier), &length);
    return length == strlen(primary) && memcmp(found, primary, length) == 0;
}

static void TestIdentifiers(void)
{
    size_t length = 0;
    const char* name =
        mlocZoneIdentifierName(&s_tables, (uint32_t)Identifier("africa/CAIRO"), &length);
    CHECK(Identifier("africa/CAIRO") >= 0 && length == 12 && memcmp(name, "Africa/Cairo", 12) == 0,
          "a name found with case ignored, given in its own case");
    CHECK(Identifier("America/New_Yor") < 0 && Identifier("Factory") < 0 && Identifier("") < 0 &&
              Identifier("America/New_York/") < 0,
          "a prefix, Factory, nothing and a longer name are not identifiers");
    CHECK(Zone("US/Eastern") == Zone("America/New_York") &&
              PrimaryIs("US/Eastern", "America/New_York"),
          "a Link reads its Zone and resolves to it");
    // ECMA-402's exceptions and test262's cases.
    CHECK(PrimaryIs("Asia/Calcutta", "Asia/Kolkata") && PrimaryIs("Etc/GMT", "UTC") &&
              PrimaryIs("Etc/UTC", "UTC") && PrimaryIs("GMT", "UTC") && PrimaryIs("UTC", "UTC") &&
              PrimaryIs("Europe/Bratislava", "Europe/Bratislava") &&
              Zone("Europe/Bratislava") == Zone("Europe/Prague") &&
              PrimaryIs("Atlantic/Jan_Mayen", "Arctic/Longyearbyen") &&
              PrimaryIs("Atlantic/Reykjavik", "Atlantic/Reykjavik") &&
              PrimaryIs("Asia/Chungking", "Asia/Shanghai") &&
              PrimaryIs("Europe/Kiev", "Europe/Kyiv") && PrimaryIs("CST6CDT", "CST6CDT"),
          "primary identifiers");
    CHECK(s_tables.identifierCount > s_tables.zoneCount && s_tables.zoneCount > 300, "the counts");
}

// A small unit: two zones (the first with a time before and after
// int32_t's range, two between, and a closing rule), three identifiers;
// test/seeds/fuzz_zones/valid holds the same bytes.
static uint8_t s_small[512];

static void Put(size_t at, uint64_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        s_small[at + (size_t)i] = (uint8_t)(value >> (8 * i));
    }
}

// The small unit's sections.
enum
{
    smallZones = 48,
    smallIds = 96,
    smallOffsets = 136,
    smallList = 168,
    smallNarrow = 176,
    smallWide = 184,
    smallTypes = 200,
    smallClosing = 208,
    smallText = 240,
    smallPrimaries = 264,
    smallSize = 268,
};

static DataSpan SmallUnit(void)
{
    memset(s_small, 0, sizeof(s_small));
    static const uint32_t s_counts[10] = {2, 3, 4, 4, 2, 2, 4, 1, 19, 2};
    for (int i = 0; i < 10; i++)
    {
        Put((size_t)i * 4, s_counts[i], 4);
    }
    // Zones: narrow, type and wide firsts, count, list first, name,
    // early, late, offset count, initial, closing.
    static const uint32_t s_zone[2][11] = {{0, 0, 4, 0, 0, 1, 1, 1, 3, 0, 1},
                                           {2, 4, 0, 2, 3, 0, 0, 0, 1, 0, 0}};
    static const int s_widths[11] = {4, 4, 2, 2, 2, 2, 1, 1, 1, 1, 1};
    for (size_t z = 0, at = smallZones; z < 2; z++, at += 3)
    {
        for (int f = 0; f < 11; at += (size_t)s_widths[f], f++)
        {
            Put(at, s_zone[z][f], s_widths[f]);
        }
    }
    // Identifiers: Etc/UTC, Test/Zone, UTC.
    static const uint32_t s_ids[3][4] = {{0, 7, 1, 0}, {7, 9, 0, 1}, {16, 3, 1, 0}};
    for (size_t i = 0; i < 3; i++)
    {
        Put(smallIds + i * 12, s_ids[i][0], 4);
        Put(smallIds + 4 + i * 12, s_ids[i][1], 1);
        Put(smallIds + 6 + i * 12, s_ids[i][2], 2);
        Put(smallIds + 8 + i * 12, s_ids[i][3], 2);
    }
    static const int32_t s_offsets[4][2] = {{-18000, 0}, {-18000, 3600}, {0, 0}, {-17762, 0}};
    for (size_t i = 0; i < 4; i++)
    {
        Put(smallOffsets + i * 8, (uint32_t)s_offsets[i][0], 4);
        Put(smallOffsets + 4 + i * 8, (uint32_t)s_offsets[i][1], 4);
    }
    static const uint16_t s_list[4] = {3, 0, 1, 2};
    for (size_t i = 0; i < 4; i++)
    {
        Put(smallList + i * 2, s_list[i], 2);
    }
    Put(smallNarrow, 100000, 4);
    Put(smallNarrow + 4, 200000, 4);
    Put(smallWide, (uint64_t)INT64_C(-3000000000), 8);
    Put(smallWide + 8, (uint64_t)INT64_C(3000000000), 8);
    Put(smallTypes, 0x02010201, 4);
    // The closing rule: from 2066, the second Sunday of March at 07:00
    // and the first of November at 06:00.
    Put(smallClosing, 2066, 4);
    Put(smallClosing + 4, 0x02000803, 4);
    Put(smallClosing + 8, 25200, 4);
    Put(smallClosing + 12, 2, 2);
    Put(smallClosing + 16, 0x0200010B, 4);
    Put(smallClosing + 20, 21600, 4);
    Put(smallClosing + 24, 1, 2);
    memcpy(s_small + smallText, "Etc/UTCTest/ZoneUTC", 19);
    Put(smallPrimaries, 0x00010000, 4);
    return (DataSpan){s_small, smallSize};
}

// Up to three writes (a byte offset, a value, its width) to a fresh small
// unit, or with no writes a size, and whether the result is valid.
typedef struct Edit
{
    size_t at[3];
    uint64_t value[3];
    int bytes[3];
    bool valid;
    const char* what;
} Edit;

// Each field at its bounds.
static void TestValidatorBounds(void)
{
    static const Edit s_edits[] = {
        {{0}, {0}, {0}, true, "the small unit"},
        {{40}, {1}, {4}, false, "the header's first zero word"},
        {{44}, {1}, {4}, false, "the header's second zero word"},
        {{47}, {0}, {0}, false, "a payload shorter than the header"},
        {{smallZones + 19}, {3}, {1}, false, "an initial offset at the zone's offset count"},
        {{smallZones + 14}, {3}, {2}, false, "a name at the identifier count"},
        {{smallZones + 21}, {1}, {1}, false, "a zone's zero byte"},
        {{smallTypes}, {3}, {1}, false, "a type at the zone's offset count"},
        {{smallNarrow + 4}, {100000}, {4}, false, "two equal times"},
        {{smallIds + 12, smallIds + 16}, {0, 7}, {4, 1}, false, "two equal names"},
        {{smallIds + 12, smallIds + 16}, {7, 3}, {4, 1}, true, "a prefix after a shorter name"},
        {{smallIds + 24, smallIds + 28}, {7, 3}, {4, 1}, false, "a prefix after its longer name"},
        {{smallIds + 12, smallIds + 16, smallIds + 24},
         {16, 3, 7},
         {4, 1, 4},
         false,
         "names out of order"},
        {{smallIds + 12 + 6}, {2}, {2}, false, "an identifier's zone at the zone count"},
        {{smallText + 3}, {' '}, {1}, false, "a space in a name"},
        {{smallText + 3}, {'~'}, {1}, true, "a tilde in a name"},
        {{smallIds + 5}, {1}, {1}, false, "an identifier's zero byte"},
        {{smallClosing}, {(uint32_t)-271822}, {4}, true, "the earliest closing year"},
        {{smallClosing}, {(uint32_t)-271823}, {4}, false, "a closing year before it"},
        {{smallClosing + 4}, {2}, {1}, true, "a closing change in February"},
        {{smallClosing + 8},
         {(uint32_t)(-8 * 86400)},
         {4},
         true,
         "a change eight days before its day"},
        {{smallClosing + 8}, {(uint32_t)(-8 * 86400 - 1)}, {4}, false, "a second more"},
        {{smallOffsets + 16}, {(uint32_t)-86400}, {4}, false, "an offset of a whole day back"},
        {{smallOffsets + 16}, {(uint32_t)-86399}, {4}, true, "a second less"},
        {{smallList}, {4}, {2}, false, "a list entry at the offset count"},
        {{smallPrimaries}, {0x00000001}, {4}, false, "primaries out of byte order"},
        {{smallPrimaries}, {0x00000000}, {4}, false, "a primary twice"},
        {{smallPrimaries}, {0x00020000}, {4}, false, "a listed name that is not primary"},
        {{36}, {1}, {4}, false, "a primary count other than the identifiers'"},
    };
    DataView view = {0};
    for (size_t i = 0; i < sizeof(s_edits) / sizeof(s_edits[0]); i++)
    {
        const Edit* e = &s_edits[i];
        DataSpan span = SmallUnit();
        if (e->bytes[0] == 0 && e->at[0] != 0)
        {
            span.size = (uint32_t)e->at[0];
        }
        for (int k = 0; k < 3 && e->bytes[k] != 0; k++)
        {
            Put(e->at[k], e->value[k], e->bytes[k]);
        }
        CHECK(mlocValidZones(&view, span) == e->valid, e->what);
    }
}

// A copy of the unit with one field changed is refused.
static void TestValidator(void)
{
    DataSpan span;
    DataView view = mlocViewOf(&s_builtin);
    CHECK(mlocFindDataUnit(&view, kindZones, dataNoLocale, &span) && mlocValidZones(&view, span),
          "the compiled-in unit");
    static uint8_t s_copy[1 << 18];
    if (span.size > sizeof(s_copy))
    {
        CHECK(false, "the copy's room");
        return;
    }
    ZoneTables t;
    DataSpan copy = {s_copy, span.size};
    bool all = true;
    // A count one higher, a type past its zone's offsets, two identifiers
    // swapped, a closing month of January, a negative saving.
    for (int c = 0; c < 5; c++)
    {
        memcpy(s_copy, span.bytes, span.size);
        all = all && mlocZoneTablesOf(copy, &t);
        if (c == 0)
        {
            s_copy[0]++;
        }
        else if (c == 1)
        {
            ((uint8_t*)t.types)[0] = 200;
        }
        else if (c == 2)
        {
            uint8_t swap[zoneIdentifierSize];
            memcpy(swap, t.identifiers, zoneIdentifierSize);
            memcpy((uint8_t*)t.identifiers, t.identifiers + zoneIdentifierSize, zoneIdentifierSize);
            memcpy((uint8_t*)t.identifiers + zoneIdentifierSize, swap, zoneIdentifierSize);
        }
        else if (c == 3)
        {
            ((uint8_t*)t.closings)[4] = 1;
        }
        else
        {
            ((uint8_t*)t.offsets)[7] = 0xFF;
        }
        all = all && !mlocValidZones(&view, copy);
    }
    CHECK(all, "a changed count, type, order, month or saving refused");
}

static void TestKnownTimes(void)
{
    uint32_t newYork = Zone("America/New_York");
    // Local mean time until November 18 of 1883 at 17:00, before int32_t's
    // range.
    CHECK(Is(OffsetAt(newYork, INT64_C(-2717650801)), -17762, 0) &&
              Is(OffsetAt(newYork, INT64_C(-2717650800)), -18000, 0) &&
              Is(OffsetAt(newYork, zoneMinSeconds), -17762, 0),
          "New York's local mean time");
    CHECK(Is(OffsetAt(newYork, 1710053999), -18000, 0) &&
              Is(OffsetAt(newYork, 1710054000), -18000, 3600) &&
              Is(OffsetAt(newYork, 1730613599), -18000, 3600) &&
              Is(OffsetAt(newYork, 1730613600), -18000, 0),
          "New York in 2024");
    // The closing rule near the end of the range: March 12 of 275758.
    CHECK(Is(OffsetAt(newYork, INT64_C(8639920882799)), -18000, 0) &&
              Is(OffsetAt(newYork, INT64_C(8639920882800)), -18000, 3600),
          "New York in 275758");
    uint32_t gaza = Zone("Asia/Gaza");
    int64_t at = 0;
    ZoneOffset o = {0};
    CHECK(Next(gaza, INT64_C(2208988800), &at, &o) && at == INT64_C(2216160000) &&
              Is(o, 7200, 3600),
          "Gaza in 2040, after int32_t's range");
}

// Where tzdb's source has negative savings, the rearguard periods.
static void TestRearguard(void)
{
    uint32_t dublin = Zone("Europe/Dublin");
    CHECK(Is(OffsetAt(dublin, 1751328000), 0, 3600) && Is(OffsetAt(dublin, 1735689600), 0, 0) &&
              Is(OffsetAt(dublin, -15897600), 3600, 0) &&
              Is(OffsetAt(dublin, 1509238799), 0, 3600) && Is(OffsetAt(dublin, 1509238800), 0, 0),
          "Dublin: summer is daylight time, and 1968 to 1971 standard");
    uint32_t windhoek = Zone("Africa/Windhoek");
    CHECK(Is(OffsetAt(windhoek, 1467331200), 3600, 0) &&
              Is(OffsetAt(windhoek, 1504400400), 3600, 3600) &&
              Is(OffsetAt(windhoek, 1508795999), 3600, 3600) &&
              Is(OffsetAt(windhoek, 1508796000), 7200, 0),
          "Windhoek: daylight time until 2017-10-24, then standard");
    uint32_t casablanca = Zone("Africa/Casablanca");
    CHECK(Is(OffsetAt(casablanca, 1751328000), 0, 3600) &&
              Is(OffsetAt(casablanca, 1740787200), 0, 0) &&
              Is(OffsetAt(casablanca, 1790812800), 0, 0),
          "Casablanca: +01 as daylight time outside Ramadan, until 2026-09-20");
}

// Each transition changes the offset, and nothing changes between two.
static void TestTransitions(void)
{
    bool all = true;
    long count = 0;
    for (uint32_t z = 0; z < s_tables.zoneCount; z++)
    {
        int64_t at = INT64_C(-5364662400);
        ZoneOffset before = OffsetAt(z, at);
        ZoneOffset o = {0};
        while (Next(z, at, &at, &o) && at < INT64_C(4102444800))
        {
            ZoneOffset previous = OffsetAt(z, at - 1);
            all = all && Is(previous, before.standard, before.save) &&
                  !Is(o, before.standard, before.save) && Is(OffsetAt(z, at), o.standard, o.save);
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
    CHECK(!Next(newYork, zoneMaxSeconds, &at, &o) && !Next(utc, zoneMinSeconds, &at, &o) &&
              Is(OffsetAt(utc, zoneMaxSeconds), 0, 0),
          "no transition past the range's end, nor in UTC");
    CHECK(Next(newYork, zoneMaxSeconds - INT64_C(31622400), &at, &o) && at <= zoneMaxSeconds,
          "the last year's transitions");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    mlocDataSet data = {&s_builtin, 1};
    CHECK(mlocFindZoneTables(&data, &s_tables), "the zones unit");
    TestIdentifiers();
    TestValidator();
    TestValidatorBounds();
    TestKnownTimes();
    TestRearguard();
    TestTransitions();
    TestRangeEnd();
    return s_failures == 0 ? 0 : 1;
}
