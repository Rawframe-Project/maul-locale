// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates from fields given by name (record mloc-0040): mlocMakeMonthDay's
// clamp and reference date against a scan of every day from 1900 to
// 2035, for every calendar but Chinese and Dangi (whose reference years
// are the proposal's table) and every month code and day; and
// mlocWithDate's merging of fields.

#include "test_harness.h"

#include "maul-locale/calendar.h"

#include <string.h>

enum
{
    calendarCount = 18,
    // January 1 of 1900, December 31 of 1972 and of 2035.
    day1900 = 693596,
    day1972End = 720258,
    day2035End = 743268,
};

static mlocDateFields Fields(mlocDate d)
{
    mlocDateFields f = {0};
    CHECK(mlocGetDateFields(&d, &f) == mloc_success, "fields");
    return f;
}

// The scan: each calendar's longest month by code over 1900 to 2035, and
// each code and day's latest day to 1972 and earliest after.
typedef struct Scan
{
    uint8_t longest[13][2];
    int32_t latest[13][2][31];
    int32_t earliest[13][2][31];
} Scan;

static void ScanCalendar(mlocCalendar calendar, Scan* scan)
{
    memset(scan, 0, sizeof(*scan));
    for (int32_t day = day1900; day <= day2035End; day++)
    {
        mlocDate d = {.day = day, .calendar = (uint8_t)calendar};
        mlocDateFields f = Fields(d);
        uint32_t number = (uint32_t)(f.monthCode[1] - '0') * 10 + (uint32_t)(f.monthCode[2] - '0');
        int leap = f.monthCodeLength == 4 ? 1 : 0;
        uint8_t* longest = &scan->longest[number - 1][leap];
        *longest = f.daysInMonth > *longest ? f.daysInMonth : *longest;
        int32_t* latest = &scan->latest[number - 1][leap][f.day - 1];
        int32_t* earliest = &scan->earliest[number - 1][leap][f.day - 1];
        if (day <= day1972End)
        {
            *latest = day;
        }
        else if (*earliest == 0)
        {
            *earliest = day;
        }
    }
}

static void TestMonthDays(void)
{
    static Scan s_scan;
    bool all = true;
    long checked = 0;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        if (c == mloc_calendarChinese || c == mloc_calendarDangi)
        {
            continue;
        }
        mlocCalendar calendar = (mlocCalendar)c;
        ScanCalendar(calendar, &s_scan);
        for (uint32_t number = 1; number <= 13; number++)
        {
            for (int leap = 0; leap < 2; leap++)
            {
                uint32_t longest = s_scan.longest[number - 1][leap];
                char code[5] = {'M', (char)('0' + number / 10), (char)('0' + number % 10), 'L', 0};
                mlocDateChanges f = {.given = mloc_givenMonthCode | mloc_givenDay,
                                     .monthCode = code,
                                     .monthCodeLength = leap ? 4 : 3};
                if (longest == 0)
                {
                    // A code no year of the calendar has.
                    f.day = 1;
                    mlocDate out = {0};
                    all = all && mlocMakeMonthDay(calendar, &f, mloc_overflowConstrain, &out) !=
                                     mloc_success;
                    continue;
                }
                for (uint32_t day = 1; day <= 31; day++)
                {
                    f.day = day;
                    mlocDate out = {0};
                    mlocResult rejected = mlocMakeMonthDay(calendar, &f, mloc_overflowReject, &out);
                    mlocResult r = mlocMakeMonthDay(calendar, &f, mloc_overflowConstrain, &out);
                    uint32_t want = day < longest ? day : longest;
                    int32_t latest = s_scan.latest[number - 1][leap][want - 1];
                    int32_t at = latest != 0 ? latest : s_scan.earliest[number - 1][leap][want - 1];
                    all = all &&
                          (day <= longest ? rejected == mloc_success
                                          : rejected == mloc_errorInvalid) &&
                          r == mloc_success && out.day == at && out.calendar == calendar;
                    checked++;
                }
            }
        }
    }
    printf("%ld month days checked\n", checked);
    CHECK(all, "every month code and day's reference date, the latest to 1972 or the next");
}

// With a year, the month is that year's and the day clamps to it; the
// Chinese table and its constraint.
static void TestMonthDayCases(void)
{
    mlocDate out = {0};
    // Hebrew Adar I needs a leap year when a year is given.
    mlocDateChanges adar = {.given = mloc_givenYear | mloc_givenMonthCode | mloc_givenDay,
                            .year = 5783,
                            .monthCode = "M05L",
                            .monthCodeLength = 4,
                            .day = 15};
    CHECK(mlocMakeMonthDay(mloc_calendarHebrew, &adar, mloc_overflowReject, &out) ==
              mloc_errorInvalid,
          "Adar I in a common year refused");
    CHECK(mlocMakeMonthDay(mloc_calendarHebrew, &adar, mloc_overflowConstrain, &out) ==
                  mloc_success &&
              Fields(out).monthCodeLength == 3 && memcmp(Fields(out).monthCode, "M06", 3) == 0,
          "Adar I in a common year clamped to Adar");
    // February 29 with a common year clamps to the 28th.
    mlocDateChanges feb = {.given = mloc_givenYear | mloc_givenMonth | mloc_givenDay,
                           .year = 2021,
                           .month = 2,
                           .day = 29};
    CHECK(mlocMakeMonthDay(mloc_calendarGregorian, &feb, mloc_overflowConstrain, &out) ==
                  mloc_success &&
              Fields(out).day == 28 && Fields(out).year == 1972,
          "February 29 of a common year");
    // A month's place without a year names no month.
    mlocDateChanges place = {.given = mloc_givenMonth | mloc_givenDay, .month = 2, .day = 1};
    mlocDateChanges both = {.given = mloc_givenMonth | mloc_givenMonthCode | mloc_givenDay,
                            .monthCode = "M02",
                            .monthCodeLength = 3,
                            .month = 2,
                            .day = 1};
    CHECK(mlocMakeMonthDay(mloc_calendarGregorian, &place, mloc_overflowConstrain, &out) ==
                  mloc_errorInvalid &&
              mlocMakeMonthDay(mloc_calendarGregorian, &both, mloc_overflowConstrain, &out) ==
                  mloc_errorInvalid,
          "a month without a year refused, beside a code too");
    // Chinese M01L has no reference year: the common month, or refused.
    mlocDateChanges rare = {.given = mloc_givenMonthCode | mloc_givenDay,
                            .monthCode = "M01L",
                            .monthCodeLength = 4,
                            .day = 10};
    CHECK(mlocMakeMonthDay(mloc_calendarChinese, &rare, mloc_overflowReject, &out) ==
              mloc_errorInvalid,
          "Chinese M01L refused");
    CHECK(mlocMakeMonthDay(mloc_calendarChinese, &rare, mloc_overflowConstrain, &out) ==
                  mloc_success &&
              Fields(out).monthCodeLength == 3,
          "Chinese M01L as M01");
    mlocDateChanges m11 = {.given = mloc_givenMonthCode | mloc_givenDay,
                           .monthCode = "M11L",
                           .monthCodeLength = 4,
                           .day = 20};
    mlocDate iso = {0};
    CHECK(mlocMakeMonthDay(mloc_calendarChinese, &m11, mloc_overflowReject, &out) == mloc_success &&
              mlocMakeDateFromDay(mloc_calendarGregorian, out.day, &iso) == mloc_success &&
              Fields(iso).year == 2034,
          "Chinese M11L day 20 in 2034");
}

// PlainDate.with: changed fields over the date's year, month code and day.
static void TestWith(void)
{
    mlocDate date = {0};
    mlocDateInput input = {.year = 5784, .monthCode = "M05L", .monthCodeLength = 4, .day = 30};
    CHECK(mlocMakeDate(mloc_calendarHebrew, &input, mloc_overflowReject, &date) == mloc_success,
          "Adar I 30");
    mlocDate out = {0};
    // A new year keeps the month code: Adar I clamps to Adar in a common
    // year, and its 30th to Adar's 29th.
    mlocDateChanges year = {.given = mloc_givenYear, .year = 5785};
    mlocDateFields f = {0};
    CHECK(mlocWithDate(&date, &year, mloc_overflowConstrain, &out) == mloc_success &&
              (f = Fields(out)).year == 5785 && memcmp(f.monthCode, "M06", 3) == 0 && f.day == 29,
          "a new year keeps the code, clamped");
    CHECK(mlocWithDate(&date, &year, mloc_overflowReject, &out) == mloc_errorInvalid,
          "and is refused under reject");
    // A month's place replaces the code.
    mlocDateChanges month = {.given = mloc_givenMonth, .month = 7};
    CHECK(mlocWithDate(&date, &month, mloc_overflowConstrain, &out) == mloc_success &&
              (f = Fields(out)).month == 7 && memcmp(f.monthCode, "M06", 3) == 0,
          "a month's place replaces the code");
    // An era with its year replaces the year; a year given as well must
    // agree.
    mlocDate gregorian = {0};
    CHECK(mlocMakeDateFromDay(mloc_calendarGregorian, 738000, &gregorian) == mloc_success,
          "a date");
    mlocDateChanges bce = {
        .given = mloc_givenEra | mloc_givenEraYear, .era = "bce", .eraLength = 3, .eraYear = 1};
    CHECK(mlocWithDate(&gregorian, &bce, mloc_overflowConstrain, &out) == mloc_success &&
              Fields(out).year == 0,
          "1 BCE");
    mlocDateChanges disagree = bce;
    disagree.given |= mloc_givenYear;
    disagree.year = 1;
    CHECK(mlocWithDate(&gregorian, &disagree, mloc_overflowConstrain, &out) == mloc_errorInvalid,
          "a year disagreeing with the era refused");
    mlocDateChanges eraAlone = {.given = mloc_givenEra, .era = "ce", .eraLength = 2};
    mlocDateChanges unknown = {.given = 64};
    CHECK(mlocWithDate(&gregorian, &eraAlone, mloc_overflowConstrain, &out) == mloc_errorInvalid &&
              mlocWithDate(&gregorian, &unknown, mloc_overflowConstrain, &out) ==
                  mloc_errorInvalid &&
              mlocWithDate(&gregorian, nullptr, mloc_overflowConstrain, &out) == mloc_errorInvalid,
          "an era alone, an unknown flag and a null pointer refused");
    mlocDateChanges none = {0};
    CHECK(mlocWithDate(&gregorian, &none, mloc_overflowReject, &out) == mloc_success &&
              out.day == gregorian.day,
          "no change");
}

int main(void)
{
    TestMonthDays();
    TestMonthDayCases();
    TestWith();
    return s_failures == 0 ? 0 : 1;
}
