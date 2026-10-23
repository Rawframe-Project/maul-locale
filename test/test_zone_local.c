// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Times of day, date-times and local times in a zone through the public
// functions: making a time with each overflow, a date-time's limits, the
// instants of a local time in a gap, a fold and an offset zone, each
// disambiguation, the start of a day skipped at midnight, the local time
// at an instant, the ends of Temporal's range, and every refusal.

#include "test_harness.h"

#include "maul-locale/zone.h"

#include <string.h>

enum
{
    epochDay = 719163,
};

static mlocData s_builtin;
static mlocDataSet s_data;

static mlocTimeZone Find(const char* id)
{
    mlocTimeZone zone = {0};
    CHECK(mlocFindTimeZone(&s_data, id, strlen(id), &zone) == mloc_success, id);
    return zone;
}

// A date-time from ISO fields.
static mlocDateTime At(int32_t year, uint32_t month, uint32_t day, int64_t hour, int64_t minute,
                       int64_t nanosecond)
{
    mlocDateInput input = {.year = year, .month = month, .day = day};
    mlocTimeInput time = {.hour = hour, .minute = minute, .nanosecond = nanosecond};
    mlocDate d = {0};
    mlocTime t = {0};
    mlocDateTime out = {0};
    CHECK(mlocMakeDate(mloc_calendarGregorian, &input, mloc_overflowReject, &d) == mloc_success &&
              mlocMakeTime(&time, mloc_overflowReject, &t) == mloc_success &&
              mlocMakeDateTime(&d, &t, &out) == mloc_success,
          "a date-time");
    return out;
}

static bool InstantIs(const mlocInstant* instant, int64_t seconds, int32_t nanoseconds)
{
    return instant->seconds == seconds && instant->nanoseconds == nanoseconds;
}

static bool ChosenIs(const mlocTimeZone* zone, const mlocDateTime* dateTime, mlocDisambiguation how,
                     int64_t seconds)
{
    mlocInstant instant = {0};
    return mlocGetInstantFor(zone, dateTime, how, &instant) == mloc_success &&
           InstantIs(&instant, seconds, dateTime->time.nanosecond);
}

static void TestMakeTime(void)
{
    mlocTime t = {0};
    mlocTimeInput input = {12, 34, 56, 987, 654, 321};
    CHECK(mlocMakeTime(&input, mloc_overflowReject, &t) == mloc_success && t.hour == 12 &&
              t.minute == 34 && t.second == 56 && t.nanosecond == 987654321 && t.reserved == 0,
          "fields within range");
    static const mlocTimeInput s_out[] = {
        {24, 0, 0, 0, 0, 0},   {-1, 0, 0, 0, 0, 0},   {0, 60, 0, 0, 0, 0},   {0, 0, 60, 0, 0, 0},
        {0, 0, 0, 1000, 0, 0}, {0, 0, 0, 0, 1000, 0}, {0, 0, 0, 0, 0, 1000}, {0, 0, 0, 0, 0, -1},
    };
    for (size_t i = 0; i < sizeof s_out / sizeof s_out[0]; i++)
    {
        CHECK(mlocMakeTime(&s_out[i], mloc_overflowReject, &t) == mloc_errorInvalid,
              "a field out of range refused");
    }
    input = (mlocTimeInput){99, -5, 60, 1000, INT64_MAX, INT64_MIN};
    CHECK(mlocMakeTime(&input, mloc_overflowConstrain, &t) == mloc_success && t.hour == 23 &&
              t.minute == 0 && t.second == 59 && t.nanosecond == 999999000,
          "fields clamped, a leap second to 59");
    CHECK(mlocMakeTime(nullptr, mloc_overflowReject, &t) == mloc_errorInvalid &&
              mlocMakeTime(&input, mloc_overflowReject, nullptr) == mloc_errorInvalid &&
              mlocMakeTime(&input, (mlocOverflow)2, &t) == mloc_errorInvalid,
          "null pointers and another overflow");
}

static void TestMakeDateTime(void)
{
    mlocDate first = {0};
    mlocDate last = {0};
    mlocTime midnight = {0};
    mlocTime lastNanosecond = {23, 59, 59, 0, 999999999};
    mlocDateTime out = {0};
    CHECK(mlocMakeDateFromDay(mloc_calendarGregorian, mlocDateMinDay, &first) == mloc_success &&
              mlocMakeDateFromDay(mloc_calendarHebrew, mlocDateMaxDay, &last) == mloc_success,
          "the range's days");
    CHECK(mlocMakeDateTime(&first, &midnight, &out) == mloc_errorInvalid,
          "the first day's midnight is out");
    mlocTime next = {.nanosecond = 1};
    CHECK(mlocMakeDateTime(&first, &next, &out) == mloc_success && out.date.day == first.day &&
              out.time.nanosecond == 1,
          "a nanosecond later is in");
    CHECK(mlocMakeDateTime(&last, &lastNanosecond, &out) == mloc_success &&
              out.date.calendar == mloc_calendarHebrew,
          "the last day's last nanosecond is in, in any calendar");
    mlocTime bad = {.hour = 24};
    mlocTime reserved = {.reserved = 1};
    mlocTime nanosecond = {.nanosecond = 1000000000};
    mlocDate forged = {.day = mlocDateMaxDay + 1};
    CHECK(mlocMakeDateTime(&last, &bad, &out) == mloc_errorInvalid &&
              mlocMakeDateTime(&last, &reserved, &out) == mloc_errorInvalid &&
              mlocMakeDateTime(&last, &nanosecond, &out) == mloc_errorInvalid &&
              mlocMakeDateTime(&forged, &midnight, &out) == mloc_errorInvalid &&
              mlocMakeDateTime(nullptr, &midnight, &out) == mloc_errorInvalid &&
              mlocMakeDateTime(&last, nullptr, &out) == mloc_errorInvalid &&
              mlocMakeDateTime(&last, &midnight, nullptr) == mloc_errorInvalid,
          "a time or date the library did not make, and null pointers");
}

static void TestPossible(void)
{
    mlocTimeZone york = Find("America/New_York");
    mlocPossibleInstants p = {0};
    // 2:30 on March 12 of 2017 was skipped, 1:30 on November 5 repeated.
    mlocDateTime gap = At(2017, 3, 12, 2, 30, 5);
    mlocDateTime fold = At(2017, 11, 5, 1, 30, 5);
    CHECK(mlocGetPossibleInstants(&york, &gap, &p) == mloc_success && p.count == 0 &&
              InstantIs(&p.earliest, 0, 0) && InstantIs(&p.latest, 0, 0),
          "none in a gap");
    CHECK(mlocGetPossibleInstants(&york, &fold, &p) == mloc_success && p.count == 2 &&
              InstantIs(&p.earliest, 1509859800, 5) && InstantIs(&p.latest, 1509863400, 5),
          "two in a fold, in order, with the nanoseconds");
    mlocDateTime plain = At(2017, 7, 1, 12, 0, 0);
    CHECK(mlocGetPossibleInstants(&york, &plain, &p) == mloc_success && p.count == 1 &&
              InstantIs(&p.earliest, 1498924800, 0) && InstantIs(&p.latest, 1498924800, 0),
          "one otherwise");
    mlocTimeZone offset = Find("-05:30");
    CHECK(mlocGetPossibleInstants(&offset, &gap, &p) == mloc_success && p.count == 1 &&
              InstantIs(&p.earliest, 1489285800 + 19800, 5),
          "an offset zone has one always");
    mlocDateTime hebrew = gap;
    hebrew.date.calendar = mloc_calendarHebrew;
    CHECK(mlocGetPossibleInstants(&york, &hebrew, &p) == mloc_success && p.count == 0,
          "the calendar does not matter");
}

static void TestDisambiguation(void)
{
    mlocTimeZone york = Find("America/New_York");
    mlocDateTime gap = At(2017, 3, 12, 2, 30, 7);
    mlocDateTime fold = At(2017, 11, 5, 1, 30, 7);
    CHECK(ChosenIs(&york, &gap, mloc_disambiguationCompatible, 1489303800) &&
              ChosenIs(&york, &gap, mloc_disambiguationLater, 1489303800) &&
              ChosenIs(&york, &gap, mloc_disambiguationEarlier, 1489300200),
          "a gap: forward by its length, earlier back");
    CHECK(ChosenIs(&york, &fold, mloc_disambiguationCompatible, 1509859800) &&
              ChosenIs(&york, &fold, mloc_disambiguationEarlier, 1509859800) &&
              ChosenIs(&york, &fold, mloc_disambiguationLater, 1509863400),
          "a fold: the earlier but for later");
    mlocInstant instant = {0};
    CHECK(mlocGetInstantFor(&york, &gap, mloc_disambiguationReject, &instant) ==
                  mloc_errorInvalid &&
              mlocGetInstantFor(&york, &fold, mloc_disambiguationReject, &instant) ==
                  mloc_errorInvalid,
          "reject refuses both");
    mlocDateTime plain = At(2017, 7, 1, 12, 0, 0);
    CHECK(ChosenIs(&york, &plain, mloc_disambiguationReject, 1498924800),
          "reject takes a single instant");
    CHECK(mlocGetInstantFor(&york, &plain, (mlocDisambiguation)4, &instant) == mloc_errorInvalid &&
              mlocGetInstantFor(&york, &plain, mloc_disambiguationCompatible, nullptr) ==
                  mloc_errorInvalid &&
              mlocGetInstantFor(&york, nullptr, mloc_disambiguationCompatible, &instant) ==
                  mloc_errorInvalid &&
              mlocGetInstantFor(nullptr, &plain, mloc_disambiguationCompatible, &instant) ==
                  mloc_errorInvalid,
          "another disambiguation and null pointers");
}

static void TestStartOfDay(void)
{
    mlocInstant instant = {0};
    // Sao Paulo's clocks went from 00:00 to 01:00 on November 4 of 2018.
    mlocTimeZone paulo = Find("America/Sao_Paulo");
    mlocDateTime skipped = At(2018, 11, 4, 12, 0, 0);
    CHECK(mlocGetStartOfDay(&paulo, &skipped.date, &instant) == mloc_success &&
              InstantIs(&instant, 1541300400, 0),
          "a skipped midnight: the day starts where the gap ends");
    mlocTimeZone york = Find("America/New_York");
    mlocDateTime fold = At(2017, 11, 5, 12, 0, 0);
    CHECK(mlocGetStartOfDay(&york, &fold.date, &instant) == mloc_success &&
              InstantIs(&instant, 1509854400, 0),
          "an ordinary midnight");
    mlocDate forged = {.day = 1, .reserved = {1}};
    CHECK(mlocGetStartOfDay(&york, &forged, &instant) == mloc_errorInvalid &&
              mlocGetStartOfDay(&york, nullptr, &instant) == mloc_errorInvalid &&
              mlocGetStartOfDay(&york, &fold.date, nullptr) == mloc_errorInvalid,
          "a date the library did not make, and null pointers");
}

static bool LocalIs(const mlocTimeZone* zone, int64_t seconds, int32_t day, uint8_t hour,
                    uint8_t minute, uint8_t second)
{
    mlocInstant instant = {seconds, 42};
    mlocDateTime d = {0};
    return mlocGetDateTimeFor(zone, &instant, mloc_calendarJapanese, &d) == mloc_success &&
           d.date.day == day && d.date.calendar == mloc_calendarJapanese && d.time.hour == hour &&
           d.time.minute == minute && d.time.second == second && d.time.nanosecond == 42;
}

static void TestDateTimeFor(void)
{
    mlocTimeZone york = Find("America/New_York");
    CHECK(LocalIs(&york, 1509859800, epochDay + 17475, 1, 30, 0) &&
              LocalIs(&york, 1509863400, epochDay + 17475, 1, 30, 0),
          "both instants of a fold read the same local time");
    CHECK(LocalIs(&york, -1, epochDay - 1, 18, 59, 59), "before 1970, across a day");
    mlocTimeZone utc = Find("UTC");
    CHECK(LocalIs(&utc, 86400, epochDay + 1, 0, 0, 0) &&
              LocalIs(&utc, -86400, epochDay - 1, 0, 0, 0),
          "midnight exactly starts its day");
    mlocTimeZone east = Find("+23:59");
    mlocTimeZone west = Find("-23:59");
    mlocInstant last = {INT64_C(8640000000000), 0};
    mlocInstant first = {INT64_C(-8640000000000), 0};
    mlocDateTime d = {0};
    CHECK(mlocGetDateTimeFor(&west, &first, mloc_calendarGregorian, &d) == mloc_success &&
              d.date.day == mlocDateMinDay && d.time.hour == 0 && d.time.minute == 1,
          "the first instant west of universal time is on the first day");
    CHECK(mlocGetDateTimeFor(&east, &last, mloc_calendarGregorian, &d) == mloc_success &&
              d.date.day == mlocDateMaxDay && d.time.hour == 23 && d.time.minute == 59,
          "the last instant east of universal time is on the last day");
    mlocInstant past = {INT64_C(8640000000000), 1};
    CHECK(mlocGetDateTimeFor(&york, &past, mloc_calendarGregorian, &d) == mloc_errorInvalid &&
              mlocGetDateTimeFor(&york, &last, (mlocCalendar)200, &d) == mloc_errorInvalid &&
              mlocGetDateTimeFor(&york, nullptr, mloc_calendarGregorian, &d) == mloc_errorInvalid &&
              mlocGetDateTimeFor(&york, &last, mloc_calendarGregorian, nullptr) ==
                  mloc_errorInvalid,
          "an instant out of range, another calendar, null pointers");
}

static void TestRangeEnds(void)
{
    mlocTimeZone west = Find("-01:00");
    mlocTimeZone east = Find("+01:00");
    mlocDate lastDay = {0};
    mlocDate firstDay = {0};
    CHECK(mlocMakeDateFromDay(mloc_calendarGregorian, mlocDateMaxDay, &lastDay) == mloc_success &&
              mlocMakeDateFromDay(mloc_calendarGregorian, mlocDateMinDay, &firstDay) ==
                  mloc_success,
          "the range's days");
    mlocDateTime late = {.date = lastDay, .time = {.hour = 0, .minute = 30}};
    mlocDateTime early = {.date = firstDay, .time = {.hour = 23, .minute = 30}};
    mlocPossibleInstants p = {0};
    mlocInstant instant = {0};
    CHECK(mlocGetPossibleInstants(&east, &late, &p) == mloc_success &&
              mlocGetPossibleInstants(&west, &late, &p) == mloc_errorInvalid,
          "past the last instant, refused");
    CHECK(mlocGetPossibleInstants(&west, &early, &p) == mloc_success &&
              mlocGetPossibleInstants(&east, &early, &p) == mloc_errorInvalid,
          "before the first instant, refused");
    mlocTimeZone york = Find("America/New_York");
    CHECK(mlocGetStartOfDay(&york, &lastDay, &instant) == mloc_errorInvalid &&
              mlocGetStartOfDay(&york, &firstDay, &instant) == mloc_errorInvalid &&
              mlocGetInstantFor(&york, &late, mloc_disambiguationCompatible, &instant) ==
                  mloc_errorInvalid,
          "the ends' days in a zone west of universal time");
    mlocTimeZone tokyo = Find("Asia/Tokyo");
    CHECK(mlocGetStartOfDay(&tokyo, &lastDay, &instant) == mloc_success &&
              InstantIs(&instant, INT64_C(8640000000000) - 9 * 3600, 0),
          "the last day starts in a zone east of it");
}

int main(void)
{
    mlocOpenBuiltinData(&s_builtin);
    s_data = (mlocDataSet){&s_builtin, 1};
    TestMakeTime();
    TestMakeDateTime();
    TestPossible();
    TestDisambiguation();
    TestStartOfDay();
    TestDateTimeFor();
    TestRangeEnds();
    return s_failures == 0 ? 0 : 1;
}
