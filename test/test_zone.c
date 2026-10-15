// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time zones through the public functions (record mloc-0041): finding a
// zone by name in any case or by offset, its identifier and primary
// identifier, equality, the list of primaries, offsets and transitions at
// known instants and at the ends of Temporal's range, and every refusal.

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <string.h>

static mlocData s_builtin;
static mlocDataSet s_data;

static mlocTimeZone Find(const char* id)
{
    mlocTimeZone zone = {0};
    CHECK(mlocFindTimeZone(&s_data, id, strlen(id), &zone) == mloc_success, id);
    return zone;
}

static bool IdIs(const mlocTimeZone* zone, bool primary, const char* want)
{
    char buffer[64];
    mlocOutput output = {.buffer = buffer, .capacity = sizeof(buffer)};
    mlocResult r =
        primary ? mlocFormatPrimaryTimeZoneId(zone, &output) : mlocFormatTimeZoneId(zone, &output);
    return r == mloc_success && output.length == strlen(want) &&
           memcmp(buffer, want, output.length) == 0;
}

static bool Equal(const char* a, const char* b)
{
    mlocTimeZone one = Find(a);
    mlocTimeZone two = Find(b);
    bool equal = false;
    return mlocTimeZonesEqual(&one, &two, &equal) == mloc_success && equal;
}

static void TestFind(void)
{
    mlocTimeZone zone = Find("africa/CAIRO");
    CHECK(IdIs(&zone, false, "Africa/Cairo") && IdIs(&zone, true, "Africa/Cairo"),
          "a name in any case, written in its own");
    zone = Find("Asia/Calcutta");
    CHECK(IdIs(&zone, false, "Asia/Calcutta") && IdIs(&zone, true, "Asia/Kolkata"),
          "a Link kept as given, its primary the Zone");
    zone = Find("Etc/GMT");
    CHECK(IdIs(&zone, true, "UTC"), "Etc/GMT resolves to UTC");
    static const char* const s_offsets[][2] = {
        {"+05:30", "+05:30"},
        {"+0530", "+05:30"},
        {"+05", "+05:00"},
        {"-00", "+00:00"},
        {"-23:59", "-23:59"},
        {"\xE2\x88\x92"
         "08:00",
         "-08:00"},
        {"+00:00", "+00:00"},
    };
    for (size_t i = 0; i < sizeof(s_offsets) / sizeof(s_offsets[0]); i++)
    {
        zone = Find(s_offsets[i][0]);
        CHECK(IdIs(&zone, false, s_offsets[i][1]) && IdIs(&zone, true, s_offsets[i][1]),
              s_offsets[i][0]);
    }
    static const char* const s_bad[] = {"+24",    "+5",    "+05:60", "+05:3", "+05:30:00",
                                        "+0530x", "05:30", "+05-30", "-",     "+"};
    for (size_t i = 0; i < sizeof(s_bad) / sizeof(s_bad[0]); i++)
    {
        mlocResult r = mlocFindTimeZone(&s_data, s_bad[i], strlen(s_bad[i]), &zone);
        CHECK(r == (s_bad[i][0] == '0' ? mloc_errorNotFound : mloc_errorSyntax), s_bad[i]);
    }
    CHECK(mlocFindTimeZone(&s_data, "Mars/Olympus", 12, &zone) == mloc_errorNotFound &&
              mlocFindTimeZone(&s_data, "Factory", 7, &zone) == mloc_errorNotFound &&
              mlocFindTimeZone(&s_data, "", 0, &zone) == mloc_errorNotFound &&
              mlocFindTimeZone(nullptr, "UTC", 3, &zone) == mloc_errorNotFound &&
              mlocFindTimeZone(nullptr, "+01", 3, &zone) == mloc_success &&
              mlocFindTimeZone(&s_data, nullptr, 3, &zone) == mloc_errorInvalid &&
              mlocFindTimeZone(&s_data, "UTC", 3, nullptr) == mloc_errorInvalid,
          "names not found, an offset without data, and null pointers");
}

static void TestEqual(void)
{
    CHECK(Equal("Asia/Calcutta", "Asia/Kolkata") && Equal("UTC", "Etc/GMT") &&
              Equal("Etc/Zulu", "utc") && Equal("+05:30", "+0530") && Equal("-00", "+00:00") &&
              Equal("Atlantic/Jan_Mayen", "Arctic/Longyearbyen"),
          "equal zones");
    CHECK(!Equal("UTC", "+00:00") && !Equal("Europe/Bratislava", "Europe/Prague") &&
              !Equal("Atlantic/Reykjavik", "Africa/Abidjan") && !Equal("+05:30", "+05:31"),
          "unequal zones, though some have the same offsets");
    mlocTimeZone zone = Find("UTC");
    mlocTimeZone forged = zone;
    forged.identifier = 60000;
    bool equal = false;
    mlocOutput output = {0};
    CHECK(mlocTimeZonesEqual(&zone, &forged, &equal) == mloc_errorInvalid &&
              mlocTimeZonesEqual(&zone, nullptr, &equal) == mloc_errorInvalid &&
              mlocTimeZonesEqual(&zone, &zone, nullptr) == mloc_errorInvalid &&
              mlocFormatTimeZoneId(&forged, &output) == mloc_errorInvalid,
          "a zone the library did not make, and null pointers");
    output = (mlocOutput){0};
    CHECK(mlocFormatTimeZoneId(&zone, &output) == mloc_errorCapacity && output.length == 3,
          "a short buffer reports the length");
}

static void TestList(void)
{
    size_t count = mlocGetTimeZoneCount(&s_data);
    char previous[64] = "";
    size_t previousLength = 0;
    bool all = count > 300;
    for (size_t i = 0; i < count; i++)
    {
        mlocTimeZone zone = {0};
        char buffer[64];
        mlocOutput id = {.buffer = buffer, .capacity = sizeof(buffer)};
        mlocOutput primary = {.buffer = previous, .capacity = 0};
        all = all && mlocGetTimeZone(&s_data, i, &zone) == mloc_success &&
              mlocFormatTimeZoneId(&zone, &id) == mloc_success &&
              mlocFormatPrimaryTimeZoneId(&zone, &primary) == mloc_errorCapacity &&
              primary.length == id.length;
        size_t common = id.length < previousLength ? id.length : previousLength;
        int order = memcmp(previous, buffer, common);
        all = all && (i == 0 || order < 0 || (order == 0 && previousLength < id.length));
        memcpy(previous, buffer, id.length);
        previousLength = id.length;
    }
    mlocTimeZone zone = {0};
    CHECK(all, "every primary, in byte order, its own primary");
    CHECK(mlocGetTimeZone(&s_data, count, &zone) == mloc_errorInvalid &&
              mlocGetTimeZoneCount(nullptr) == 0,
          "past the list, and no data");
}

static int32_t Offset(const char* id, int64_t seconds, int32_t nanoseconds)
{
    mlocTimeZone zone = Find(id);
    mlocInstant instant = {seconds, nanoseconds};
    int32_t offset = INT32_MIN;
    CHECK(mlocGetTimeZoneOffset(&zone, &instant, &offset) == mloc_success, id);
    return offset;
}

static bool Transition(const char* id, int64_t seconds, int32_t nanoseconds,
                       mlocTransitionDirection direction, int64_t want)
{
    mlocTimeZone zone = Find(id);
    mlocInstant instant = {seconds, nanoseconds};
    mlocInstant found = {0};
    mlocResult r = mlocGetTimeZoneTransition(&zone, &instant, direction, &found);
    return want == INT64_MIN ? r == mloc_errorNotFound
                             : r == mloc_success && found.seconds == want && found.nanoseconds == 0;
}

static void TestOffsets(void)
{
    CHECK(Offset("America/New_York", 1710053999, 999999999) == -18000 &&
              Offset("America/New_York", 1710054000, 0) == -14400 &&
              Offset("America/New_York", INT64_C(-8640000000000), 0) == -17762 &&
              Offset("Asia/Kolkata", 0, 0) == 19800 && Offset("-08:00", 0, 0) == -28800 &&
              Offset("Europe/Dublin", 1751328000, 0) == 3600,
          "offsets at known instants");
    mlocTimeZone zone = Find("UTC");
    int32_t offset = 0;
    mlocInstant past = {INT64_C(8640000000000), 1};
    mlocInstant before = {INT64_C(-8640000000001), 999999999};
    mlocInstant nanos = {0, 1000000000};
    CHECK(mlocGetTimeZoneOffset(&zone, &past, &offset) == mloc_errorInvalid &&
              mlocGetTimeZoneOffset(&zone, &before, &offset) == mloc_errorInvalid &&
              mlocGetTimeZoneOffset(&zone, &nanos, &offset) == mloc_errorInvalid &&
              mlocGetTimeZoneOffset(&zone, nullptr, &offset) == mloc_errorInvalid,
          "instants out of range refused");
}

static void TestTransitions(void)
{
    // New York's spring forward of 2024 at 07:00 universal time.
    CHECK(Transition("America/New_York", 1710053999, 0, mloc_transitionNext, 1710054000) &&
              Transition("America/New_York", 1710054000, 0, mloc_transitionNext, 1730613600) &&
              Transition("America/New_York", 1710054000, 0, mloc_transitionPrevious, 1699164000) &&
              Transition("America/New_York", 1710054000, 1, mloc_transitionPrevious, 1710054000),
          "next and previous, strictly");
    // London kept its offset in 1968 to 1971 (British Standard Time): the
    // change of name alone is no transition.
    CHECK(Transition("Europe/London", -59004000, 0, mloc_transitionNext, 57722400),
          "a change of daylight flag alone is passed over");
    CHECK(Transition("UTC", 0, 0, mloc_transitionNext, INT64_MIN) &&
              Transition("+05:30", 0, 0, mloc_transitionPrevious, INT64_MIN) &&
              Transition("America/New_York", INT64_C(-8640000000000), 0, mloc_transitionPrevious,
                         INT64_MIN) &&
              Transition("America/New_York", INT64_C(8640000000000), 0, mloc_transitionNext,
                         INT64_MIN),
          "none in UTC, an offset zone, or past the range's ends");
    mlocTimeZone zone = Find("UTC");
    mlocInstant instant = {0};
    mlocInstant found = {0};
    CHECK(mlocGetTimeZoneTransition(&zone, &instant, (mlocTransitionDirection)2, &found) ==
                  mloc_errorInvalid &&
              mlocGetTimeZoneTransition(&zone, &instant, mloc_transitionNext, nullptr) ==
                  mloc_errorInvalid,
          "another direction, and a null pointer");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    TestFind();
    TestEqual();
    TestList();
    TestOffsets();
    TestTransitions();
    return s_failures == 0 ? 0 : 1;
}
