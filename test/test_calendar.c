// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates (maul-locale/calendar.h): each arithmetic calendar's fields at
// days across Temporal's range as ICU4X 2.3 gives them, dates made back
// from their fields by era and by year, by month code and by place,
// overflow constrained and refused, the other fields, and every refusal.

#include "test_harness.h"

#include "maul-locale/calendar.h"

#include <stdio.h>
#include <string.h>

typedef struct Case
{
    mlocCalendar calendar;
    int32_t day;
    int32_t year;
    const char* era;
    int32_t eraYear;
    uint8_t month;
    uint8_t dayOfMonth;
} Case;

// ICU4X 2.3's fields (icu_calendar's to_calendar) at Temporal's first and
// last days, far back, around day 1 and now.
static const Case s_cases[] = {
    {mloc_calendarGregorian, -99280838, -271821, "bce", 271822, 4, 19},
    {mloc_calendarGregorian, -2000000, -5475, "bce", 5476, 3, 9},
    {mloc_calendarGregorian, 0, 0, "bce", 1, 12, 31},
    {mloc_calendarGregorian, 1, 1, "ce", 1, 1, 1},
    {mloc_calendarGregorian, 730120, 2000, "ce", 2000, 1, 1},
    {mloc_calendarGregorian, 739000, 2024, "ce", 2024, 4, 24},
    {mloc_calendarGregorian, 100719163, 275760, "ce", 275760, 9, 13},
    {mloc_calendarBuddhist, -99280838, -271278, "be", -271278, 4, 19},
    {mloc_calendarBuddhist, -2000000, -4932, "be", -4932, 3, 9},
    {mloc_calendarBuddhist, 0, 543, "be", 543, 12, 31},
    {mloc_calendarBuddhist, 1, 544, "be", 544, 1, 1},
    {mloc_calendarBuddhist, 730120, 2543, "be", 2543, 1, 1},
    {mloc_calendarBuddhist, 739000, 2567, "be", 2567, 4, 24},
    {mloc_calendarBuddhist, 100719163, 276303, "be", 276303, 9, 13},
    {mloc_calendarRoc, -99280838, -273732, "broc", 273733, 4, 19},
    {mloc_calendarRoc, -2000000, -7386, "broc", 7387, 3, 9},
    {mloc_calendarRoc, 0, -1911, "broc", 1912, 12, 31},
    {mloc_calendarRoc, 1, -1910, "broc", 1911, 1, 1},
    {mloc_calendarRoc, 730120, 89, "roc", 89, 1, 1},
    {mloc_calendarRoc, 739000, 113, "roc", 113, 4, 24},
    {mloc_calendarRoc, 100719163, 273849, "roc", 273849, 9, 13},
    {mloc_calendarCoptic, -99280838, -272099, "am", -272099, 3, 23},
    {mloc_calendarCoptic, -2000000, -5759, "am", -5759, 8, 26},
    {mloc_calendarCoptic, 0, -283, "am", -283, 5, 7},
    {mloc_calendarCoptic, 1, -283, "am", -283, 5, 8},
    {mloc_calendarCoptic, 730120, 1716, "am", 1716, 4, 22},
    {mloc_calendarCoptic, 739000, 1740, "am", 1740, 8, 16},
    {mloc_calendarCoptic, 100719163, 275471, "am", 275471, 5, 22},
    {mloc_calendarEthiopic, -99280838, -271823, "aa", -266323, 3, 23},
    {mloc_calendarEthiopic, -2000000, -5483, "aa", 17, 8, 26},
    {mloc_calendarEthiopic, 0, -7, "aa", 5493, 5, 7},
    {mloc_calendarEthiopic, 1, -7, "aa", 5493, 5, 8},
    {mloc_calendarEthiopic, 730120, 1992, "am", 1992, 4, 22},
    {mloc_calendarEthiopic, 739000, 2016, "am", 2016, 8, 16},
    {mloc_calendarEthiopic, 100719163, 275747, "am", 275747, 5, 22},
    {mloc_calendarEthiopicAmeteAlem, -99280838, -266323, "aa", -266323, 3, 23},
    {mloc_calendarEthiopicAmeteAlem, -2000000, 17, "aa", 17, 8, 26},
    {mloc_calendarEthiopicAmeteAlem, 0, 5493, "aa", 5493, 5, 7},
    {mloc_calendarEthiopicAmeteAlem, 1, 5493, "aa", 5493, 5, 8},
    {mloc_calendarEthiopicAmeteAlem, 730120, 7492, "aa", 7492, 4, 22},
    {mloc_calendarEthiopicAmeteAlem, 739000, 7516, "aa", 7516, 8, 16},
    {mloc_calendarEthiopicAmeteAlem, 100719163, 281247, "aa", 281247, 5, 22},
    {mloc_calendarIndian, -99280838, -271899, "shaka", -271899, 1, 29},
    {mloc_calendarIndian, -2000000, -5554, "shaka", -5554, 12, 18},
    {mloc_calendarIndian, 0, -78, "shaka", -78, 10, 10},
    {mloc_calendarIndian, 1, -78, "shaka", -78, 10, 11},
    {mloc_calendarIndian, 730120, 1921, "shaka", 1921, 10, 11},
    {mloc_calendarIndian, 739000, 1946, "shaka", 1946, 2, 4},
    {mloc_calendarIndian, 100719163, 275682, "shaka", 275682, 6, 22},
    {mloc_calendarPersian, -99280838, -272442, "ap", -272442, 1, 9},
    {mloc_calendarPersian, -2000000, -6097, "ap", -6097, 12, 18},
    {mloc_calendarPersian, 0, -621, "ap", -621, 10, 10},
    {mloc_calendarPersian, 1, -621, "ap", -621, 10, 11},
    {mloc_calendarPersian, 730120, 1378, "ap", 1378, 10, 11},
    {mloc_calendarPersian, 739000, 1403, "ap", 1403, 2, 5},
    {mloc_calendarPersian, 100719163, 275139, "ap", 275139, 7, 12},
    {mloc_calendarIslamicCivil, -99280838, -280804, "bh", 280805, 3, 21},
    {mloc_calendarIslamicCivil, -2000000, -6284, "bh", 6285, 7, 4},
    {mloc_calendarIslamicCivil, 0, -640, "bh", 641, 5, 17},
    {mloc_calendarIslamicCivil, 1, -640, "bh", 641, 5, 18},
    {mloc_calendarIslamicCivil, 730120, 1420, "ah", 1420, 9, 24},
    {mloc_calendarIslamicCivil, 739000, 1445, "ah", 1445, 10, 15},
    {mloc_calendarIslamicCivil, 100719163, 283583, "ah", 283583, 5, 23},
    {mloc_calendarIslamicTbla, -99280838, -280804, "bh", 280805, 3, 22},
    {mloc_calendarIslamicTbla, -2000000, -6284, "bh", 6285, 7, 5},
    {mloc_calendarIslamicTbla, 0, -640, "bh", 641, 5, 18},
    {mloc_calendarIslamicTbla, 1, -640, "bh", 641, 5, 19},
    {mloc_calendarIslamicTbla, 730120, 1420, "ah", 1420, 9, 25},
    {mloc_calendarIslamicTbla, 739000, 1445, "ah", 1445, 10, 16},
    {mloc_calendarIslamicTbla, 100719163, 283583, "ah", 283583, 5, 24},
};

static bool Fields(mlocCalendar calendar, int32_t day, mlocDateFields* out)
{
    mlocDate date;
    return mlocMakeDateFromDay(calendar, day, &date) == mloc_success &&
           mlocGetDateFields(&date, out) == mloc_success;
}

static void TestOracle(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_cases) / sizeof(s_cases[0]); i++)
    {
        const Case* c = &s_cases[i];
        mlocDateFields f;
        bool same = Fields(c->calendar, c->day, &f) && f.year == c->year &&
                    f.eraYear == c->eraYear && f.eraLength == strlen(c->era) &&
                    memcmp(f.era, c->era, f.eraLength) == 0 && f.month == c->month &&
                    f.day == c->dayOfMonth && f.monthCodeLength == 3 &&
                    f.monthCode[1] == '0' + c->month / 10 && f.monthCode[2] == '0' + c->month % 10;
        if (!same)
        {
            printf("calendar %d day %d\n", (int)c->calendar, (int)c->day);
            all = false;
        }
    }
    CHECK(all, "each calendar's fields as ICU4X gives them");
}

// Makes a date from its fields by era and by year, by code and by place.
static bool MadeBack(mlocCalendar calendar, int32_t day)
{
    mlocDateFields f;
    mlocDate byEra;
    mlocDate byYear;
    mlocDateInput era = {
        .era = f.era,
        .monthCode = f.monthCode,
        .monthCodeLength = 3,
    };
    if (!Fields(calendar, day, &f))
    {
        return false;
    }
    era.eraLength = f.eraLength;
    era.era = f.eraLength > 0 ? f.era : nullptr;
    era.year = f.eraLength > 0 ? f.eraYear : f.year;
    era.day = f.day;
    mlocDateInput year = {.year = f.year, .month = f.month, .day = f.day};
    return mlocMakeDate(calendar, &era, mloc_overflowReject, &byEra) == mloc_success &&
           mlocMakeDate(calendar, &year, mloc_overflowReject, &byYear) == mloc_success &&
           byEra.day == day && byYear.day == day && byEra.calendar == calendar;
}

static void TestRoundTrip(void)
{
    static const mlocCalendar s_all[] = {
        mloc_calendarGregorian,
        mloc_calendarIso8601,
        mloc_calendarBuddhist,
        mloc_calendarRoc,
        mloc_calendarCoptic,
        mloc_calendarEthiopic,
        mloc_calendarEthiopicAmeteAlem,
        mloc_calendarIndian,
        mloc_calendarPersian,
        mloc_calendarIslamicCivil,
        mloc_calendarIslamicTbla,
    };
    bool all = true;
    for (size_t c = 0; c < sizeof(s_all) / sizeof(s_all[0]); c++)
    {
        for (int64_t day = -800000; all && day <= 1600000; day += 3)
        {
            all = MadeBack(s_all[c], (int32_t)day);
            if (!all)
            {
                printf("calendar %d day %d\n", (int)s_all[c], (int)day);
            }
        }
        all = all && MadeBack(s_all[c], mlocDateMinDay) && MadeBack(s_all[c], mlocDateMaxDay);
    }
    CHECK(all, "dates made back from their fields, by era and year, code and place");
}

static bool Made(mlocCalendar calendar, const mlocDateInput* input, mlocOverflow overflow,
                 mlocResult status, int32_t day)
{
    mlocDate date = {0};
    mlocResult r = mlocMakeDate(calendar, input, overflow, &date);
    bool same = r == status && (status != mloc_success || date.day == day);
    if (!same)
    {
        printf("made %d %d\n", (int)r, (int)date.day);
    }
    return same;
}

static void TestOverflow(void)
{
    mlocDateInput feb30 = {.year = 2023, .month = 2, .day = 30};
    mlocDateInput feb29 = {.year = 2024, .month = 2, .day = 29};
    mlocDateInput month13 = {.year = 2023, .month = 13, .day = 31};
    mlocDateInput m13 = {.year = 1740, .monthCode = "M13", .monthCodeLength = 3, .day = 6};
    mlocDateInput leapM13 = {.year = 1739, .monthCode = "M13", .monthCodeLength = 3, .day = 6};
    mlocDateInput esfand = {.year = 1403, .month = 12, .day = 30};
    CHECK(
        Made(mloc_calendarGregorian, &feb30, mloc_overflowConstrain, mloc_success, 738579) &&
            Made(mloc_calendarGregorian, &feb30, mloc_overflowReject, mloc_errorInvalid, 0) &&
            Made(mloc_calendarGregorian, &feb29, mloc_overflowReject, mloc_success, 738945) &&
            Made(mloc_calendarGregorian, &month13, mloc_overflowConstrain, mloc_success, 738885) &&
            Made(mloc_calendarGregorian, &month13, mloc_overflowReject, mloc_errorInvalid, 0) &&
            Made(mloc_calendarCoptic, &m13, mloc_overflowConstrain, mloc_success, 739139) &&
            Made(mloc_calendarCoptic, &m13, mloc_overflowReject, mloc_errorInvalid, 0) &&
            Made(mloc_calendarCoptic, &leapM13, mloc_overflowReject, mloc_success, 738774) &&
            Made(mloc_calendarPersian, &esfand, mloc_overflowReject, mloc_success, 739330),
        "a day or month past the calendar's clamped or refused; Coptic's sixth epagomenal "
        "day in a leap year only; a Persian leap year's 30 Esfand");
}

static void TestEras(void)
{
    mlocDateInput bc = {.era = "bc", .eraLength = 2, .year = 1, .month = 12, .day = 31};
    mlocDateInput bce = {.era = "bce", .eraLength = 3, .year = 1, .month = 12, .day = 31};
    mlocDateInput broc = {.era = "broc", .eraLength = 4, .year = 1911, .month = 1, .day = 1};
    mlocDateInput aa = {.era = "aa", .eraLength = 2, .year = 5493, .month = 5, .day = 8};
    mlocDateInput bh = {.era = "bh", .eraLength = 2, .year = 641, .month = 5, .day = 18};
    mlocDateInput other = {.era = "ah", .eraLength = 2, .year = 1, .month = 1, .day = 1};
    mlocDateInput leap = {.year = 2024, .monthCode = "M02L", .monthCodeLength = 4, .day = 1};
    mlocDateInput bad = {.year = 2024, .monthCode = "M2", .monthCodeLength = 2, .day = 1};
    mlocDateInput both = {
        .year = 2024, .monthCode = "M02", .monthCodeLength = 3, .month = 3, .day = 1};
    CHECK(Made(mloc_calendarGregorian, &bc, mloc_overflowReject, mloc_success, 0) &&
              Made(mloc_calendarGregorian, &bce, mloc_overflowReject, mloc_success, 0) &&
              Made(mloc_calendarRoc, &broc, mloc_overflowReject, mloc_success, 1) &&
              Made(mloc_calendarEthiopic, &aa, mloc_overflowReject, mloc_success, 1) &&
              Made(mloc_calendarIslamicCivil, &bh, mloc_overflowReject, mloc_success, 1) &&
              Made(mloc_calendarGregorian, &other, mloc_overflowReject, mloc_errorSyntax, 0) &&
              Made(mloc_calendarIso8601, &bce, mloc_overflowReject, mloc_errorSyntax, 0) &&
              Made(mloc_calendarGregorian, &leap, mloc_overflowConstrain, mloc_errorSyntax, 0) &&
              Made(mloc_calendarGregorian, &bad, mloc_overflowConstrain, mloc_errorSyntax, 0) &&
              Made(mloc_calendarGregorian, &both, mloc_overflowConstrain, mloc_errorInvalid, 0),
          "eras and their aliases; an era of another calendar; month codes leap, malformed, "
          "or naming another month than the place");
}

static void TestFields(void)
{
    mlocDateFields f;
    CHECK(Fields(mloc_calendarGregorian, 730120, &f) && f.dayOfWeek == 6 && f.dayOfYear == 1 &&
              f.daysInYear == 366 && f.inLeapYear && f.daysInMonth == 31 && f.monthsInYear == 12 &&
              Fields(mloc_calendarIso8601, 730120, &f) && f.eraLength == 0 && f.eraYear == 2000 &&
              Fields(mloc_calendarCoptic, 738774, &f) && f.monthsInYear == 13 && f.month == 13 &&
              f.daysInMonth == 6 && f.dayOfYear == 366 && f.daysInYear == 366 &&
              Fields(mloc_calendarGregorian, 0, &f) && f.dayOfWeek == 7 &&
              Fields(mloc_calendarGregorian, mlocDateMinDay, &f) && f.dayOfWeek == 1,
          "weekday, day of year and the year's and month's lengths");
}

static void TestRefusals(void)
{
    mlocDate date;
    mlocDateFields f;
    mlocDateInput ok = {.year = 2024, .month = 1, .day = 1};
    mlocDateInput dayZero = {.year = 2024, .month = 1, .day = 0};
    mlocDateInput monthZero = {.year = 2024, .day = 1};
    mlocDateInput far = {.year = 2000000, .month = 1, .day = 1};
    mlocDateInput past = {.year = 275760, .month = 9, .day = 14};
    mlocDateInput eraLength = {.eraLength = 2, .year = 2024, .month = 1, .day = 1};
    mlocDateInput codeLength = {.monthCodeLength = 3, .year = 2024, .day = 1};
    mlocDate bad = {.day = 1, .calendar = 18};
    mlocDate reserved = {.day = 1, .reserved = {1}};
    mlocDate outside = {.day = mlocDateMaxDay + 1};
    mlocDate japanese = {.day = 1, .calendar = mloc_calendarJapanese};
    CHECK(
        mlocMakeDate(mloc_calendarGregorian, nullptr, mloc_overflowReject, &date) ==
                mloc_errorInvalid &&
            mlocMakeDate(mloc_calendarGregorian, &ok, mloc_overflowReject, nullptr) ==
                mloc_errorInvalid &&
            mlocMakeDate((mlocCalendar)18, &ok, mloc_overflowReject, &date) == mloc_errorInvalid &&
            mlocMakeDate(mloc_calendarGregorian, &ok, (mlocOverflow)2, &date) ==
                mloc_errorInvalid &&
            Made(mloc_calendarGregorian, &dayZero, mloc_overflowConstrain, mloc_errorInvalid, 0) &&
            Made(mloc_calendarGregorian, &monthZero, mloc_overflowConstrain, mloc_errorInvalid,
                 0) &&
            Made(mloc_calendarGregorian, &far, mloc_overflowConstrain, mloc_errorInvalid, 0) &&
            Made(mloc_calendarGregorian, &past, mloc_overflowConstrain, mloc_errorInvalid, 0) &&
            Made(mloc_calendarGregorian, &eraLength, mloc_overflowConstrain, mloc_errorInvalid,
                 0) &&
            Made(mloc_calendarGregorian, &codeLength, mloc_overflowConstrain, mloc_errorInvalid,
                 0) &&
            Made(mloc_calendarHebrew, &ok, mloc_overflowConstrain, mloc_errorNotFound, 0) &&
            mlocMakeDateFromDay(mloc_calendarGregorian, mlocDateMinDay - 1, &date) ==
                mloc_errorInvalid &&
            mlocMakeDateFromDay(mloc_calendarGregorian, 1, nullptr) == mloc_errorInvalid &&
            mlocMakeDateFromDay((mlocCalendar)18, 1, &date) == mloc_errorInvalid &&
            mlocMakeDateFromDay(mloc_calendarChinese, 1, &date) == mloc_errorNotFound &&
            mlocGetDateFields(nullptr, &f) == mloc_errorInvalid &&
            mlocGetDateFields(&japanese, nullptr) == mloc_errorInvalid &&
            mlocGetDateFields(&bad, &f) == mloc_errorInvalid &&
            mlocGetDateFields(&reserved, &f) == mloc_errorInvalid &&
            mlocGetDateFields(&outside, &f) == mloc_errorInvalid &&
            mlocGetDateFields(&japanese, &f) == mloc_errorNotFound,
        "every refusal, and the calendars not built yet");
}

int main(void)
{
    TestOracle();
    TestRoundTrip();
    TestOverflow();
    TestEras();
    TestFields();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
