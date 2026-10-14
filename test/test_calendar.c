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
    bool leapMonth;
    uint8_t dayOfMonth;
    bool inLeapYear;
    uint16_t daysInYear;
    uint8_t daysInMonth;
} Case;

// ICU4X 2.3's fields (icu_calendar's to_calendar) at Temporal's first and
// last days, far back, around day 1, now, a Coptic leap year's last
// day, Persian years around a correction (1502, not leap, and 1503), and
// years just past the leap rules' bounds (Persian 1407, Islamic 1437);
// Hebrew's Shevat, Adar I ('M05L'), Adar II and a common year's Adar.
static const Case s_cases[] = {
    {mloc_calendarGregorian, -99280838, -271821, "bce", 271822, 4, false, 19, 0, 365, 30},
    {mloc_calendarGregorian, -2000000, -5475, "bce", 5476, 3, false, 9, 0, 365, 31},
    {mloc_calendarGregorian, 0, 0, "bce", 1, 12, false, 31, 1, 366, 31},
    {mloc_calendarGregorian, 1, 1, "ce", 1, 1, false, 1, 0, 365, 31},
    {mloc_calendarGregorian, 730120, 2000, "ce", 2000, 1, false, 1, 1, 366, 31},
    {mloc_calendarGregorian, 738774, 2023, "ce", 2023, 9, false, 11, 0, 365, 30},
    {mloc_calendarGregorian, 739000, 2024, "ce", 2024, 4, false, 24, 1, 366, 30},
    {mloc_calendarGregorian, 739139, 2024, "ce", 2024, 9, false, 10, 1, 366, 30},
    {mloc_calendarGregorian, 775488, 2124, "ce", 2124, 3, false, 19, 1, 366, 31},
    {mloc_calendarGregorian, 775489, 2124, "ce", 2124, 3, false, 20, 1, 366, 31},
    {mloc_calendarGregorian, 775854, 2125, "ce", 2125, 3, false, 20, 0, 365, 31},
    {mloc_calendarGregorian, 811647, 2223, "ce", 2223, 3, false, 20, 0, 365, 31},
    {mloc_calendarGregorian, 850000, 2328, "ce", 2328, 3, false, 22, 1, 366, 31},
    {mloc_calendarGregorian, 100719163, 275760, "ce", 275760, 9, false, 13, 1, 366, 30},
    {mloc_calendarBuddhist, -99280838, -271278, "be", -271278, 4, false, 19, 0, 365, 30},
    {mloc_calendarBuddhist, -2000000, -4932, "be", -4932, 3, false, 9, 0, 365, 31},
    {mloc_calendarBuddhist, 0, 543, "be", 543, 12, false, 31, 1, 366, 31},
    {mloc_calendarBuddhist, 1, 544, "be", 544, 1, false, 1, 0, 365, 31},
    {mloc_calendarBuddhist, 730120, 2543, "be", 2543, 1, false, 1, 1, 366, 31},
    {mloc_calendarBuddhist, 738774, 2566, "be", 2566, 9, false, 11, 0, 365, 30},
    {mloc_calendarBuddhist, 739000, 2567, "be", 2567, 4, false, 24, 1, 366, 30},
    {mloc_calendarBuddhist, 739139, 2567, "be", 2567, 9, false, 10, 1, 366, 30},
    {mloc_calendarBuddhist, 775488, 2667, "be", 2667, 3, false, 19, 1, 366, 31},
    {mloc_calendarBuddhist, 775489, 2667, "be", 2667, 3, false, 20, 1, 366, 31},
    {mloc_calendarBuddhist, 775854, 2668, "be", 2668, 3, false, 20, 0, 365, 31},
    {mloc_calendarBuddhist, 811647, 2766, "be", 2766, 3, false, 20, 0, 365, 31},
    {mloc_calendarBuddhist, 850000, 2871, "be", 2871, 3, false, 22, 1, 366, 31},
    {mloc_calendarBuddhist, 100719163, 276303, "be", 276303, 9, false, 13, 1, 366, 30},
    {mloc_calendarRoc, -99280838, -273732, "broc", 273733, 4, false, 19, 0, 365, 30},
    {mloc_calendarRoc, -2000000, -7386, "broc", 7387, 3, false, 9, 0, 365, 31},
    {mloc_calendarRoc, 0, -1911, "broc", 1912, 12, false, 31, 1, 366, 31},
    {mloc_calendarRoc, 1, -1910, "broc", 1911, 1, false, 1, 0, 365, 31},
    {mloc_calendarRoc, 730120, 89, "roc", 89, 1, false, 1, 1, 366, 31},
    {mloc_calendarRoc, 738774, 112, "roc", 112, 9, false, 11, 0, 365, 30},
    {mloc_calendarRoc, 739000, 113, "roc", 113, 4, false, 24, 1, 366, 30},
    {mloc_calendarRoc, 739139, 113, "roc", 113, 9, false, 10, 1, 366, 30},
    {mloc_calendarRoc, 775488, 213, "roc", 213, 3, false, 19, 1, 366, 31},
    {mloc_calendarRoc, 775489, 213, "roc", 213, 3, false, 20, 1, 366, 31},
    {mloc_calendarRoc, 775854, 214, "roc", 214, 3, false, 20, 0, 365, 31},
    {mloc_calendarRoc, 811647, 312, "roc", 312, 3, false, 20, 0, 365, 31},
    {mloc_calendarRoc, 850000, 417, "roc", 417, 3, false, 22, 1, 366, 31},
    {mloc_calendarRoc, 100719163, 273849, "roc", 273849, 9, false, 13, 1, 366, 30},
    {mloc_calendarCoptic, -99280838, -272099, "am", -272099, 3, false, 23, 0, 365, 30},
    {mloc_calendarCoptic, -2000000, -5759, "am", -5759, 8, false, 26, 0, 365, 30},
    {mloc_calendarCoptic, 0, -283, "am", -283, 5, false, 7, 0, 365, 30},
    {mloc_calendarCoptic, 1, -283, "am", -283, 5, false, 8, 0, 365, 30},
    {mloc_calendarCoptic, 730120, 1716, "am", 1716, 4, false, 22, 0, 365, 30},
    {mloc_calendarCoptic, 738774, 1739, "am", 1739, 13, false, 6, 1, 366, 6},
    {mloc_calendarCoptic, 739000, 1740, "am", 1740, 8, false, 16, 0, 365, 30},
    {mloc_calendarCoptic, 739139, 1740, "am", 1740, 13, false, 5, 0, 365, 5},
    {mloc_calendarCoptic, 775488, 1840, "am", 1840, 7, false, 9, 0, 365, 30},
    {mloc_calendarCoptic, 775489, 1840, "am", 1840, 7, false, 10, 0, 365, 30},
    {mloc_calendarCoptic, 775854, 1841, "am", 1841, 7, false, 10, 0, 365, 30},
    {mloc_calendarCoptic, 811647, 1939, "am", 1939, 7, false, 9, 1, 366, 30},
    {mloc_calendarCoptic, 850000, 2044, "am", 2044, 7, false, 10, 0, 365, 30},
    {mloc_calendarCoptic, 100719163, 275471, "am", 275471, 5, false, 22, 1, 366, 30},
    {mloc_calendarEthiopic, -99280838, -271823, "aa", -266323, 3, false, 23, 0, 365, 30},
    {mloc_calendarEthiopic, -2000000, -5483, "aa", 17, 8, false, 26, 0, 365, 30},
    {mloc_calendarEthiopic, 0, -7, "aa", 5493, 5, false, 7, 0, 365, 30},
    {mloc_calendarEthiopic, 1, -7, "aa", 5493, 5, false, 8, 0, 365, 30},
    {mloc_calendarEthiopic, 730120, 1992, "am", 1992, 4, false, 22, 0, 365, 30},
    {mloc_calendarEthiopic, 738774, 2015, "am", 2015, 13, false, 6, 1, 366, 6},
    {mloc_calendarEthiopic, 739000, 2016, "am", 2016, 8, false, 16, 0, 365, 30},
    {mloc_calendarEthiopic, 739139, 2016, "am", 2016, 13, false, 5, 0, 365, 5},
    {mloc_calendarEthiopic, 775488, 2116, "am", 2116, 7, false, 9, 0, 365, 30},
    {mloc_calendarEthiopic, 775489, 2116, "am", 2116, 7, false, 10, 0, 365, 30},
    {mloc_calendarEthiopic, 775854, 2117, "am", 2117, 7, false, 10, 0, 365, 30},
    {mloc_calendarEthiopic, 811647, 2215, "am", 2215, 7, false, 9, 1, 366, 30},
    {mloc_calendarEthiopic, 850000, 2320, "am", 2320, 7, false, 10, 0, 365, 30},
    {mloc_calendarEthiopic, 100719163, 275747, "am", 275747, 5, false, 22, 1, 366, 30},
    {mloc_calendarEthiopicAmeteAlem, -99280838, -266323, "aa", -266323, 3, false, 23, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, -2000000, 17, "aa", 17, 8, false, 26, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 0, 5493, "aa", 5493, 5, false, 7, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 1, 5493, "aa", 5493, 5, false, 8, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 730120, 7492, "aa", 7492, 4, false, 22, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 738774, 7515, "aa", 7515, 13, false, 6, 1, 366, 6},
    {mloc_calendarEthiopicAmeteAlem, 739000, 7516, "aa", 7516, 8, false, 16, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 739139, 7516, "aa", 7516, 13, false, 5, 0, 365, 5},
    {mloc_calendarEthiopicAmeteAlem, 775488, 7616, "aa", 7616, 7, false, 9, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 775489, 7616, "aa", 7616, 7, false, 10, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 775854, 7617, "aa", 7617, 7, false, 10, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 811647, 7715, "aa", 7715, 7, false, 9, 1, 366, 30},
    {mloc_calendarEthiopicAmeteAlem, 850000, 7820, "aa", 7820, 7, false, 10, 0, 365, 30},
    {mloc_calendarEthiopicAmeteAlem, 100719163, 281247, "aa", 281247, 5, false, 22, 1, 366, 30},
    {mloc_calendarIndian, -99280838, -271899, "shaka", -271899, 1, false, 29, 0, 365, 30},
    {mloc_calendarIndian, -2000000, -5554, "shaka", -5554, 12, false, 18, 1, 366, 30},
    {mloc_calendarIndian, 0, -78, "shaka", -78, 10, false, 10, 1, 366, 30},
    {mloc_calendarIndian, 1, -78, "shaka", -78, 10, false, 11, 1, 366, 30},
    {mloc_calendarIndian, 730120, 1921, "shaka", 1921, 10, false, 11, 0, 365, 30},
    {mloc_calendarIndian, 738774, 1945, "shaka", 1945, 6, false, 20, 0, 365, 31},
    {mloc_calendarIndian, 739000, 1946, "shaka", 1946, 2, false, 4, 1, 366, 31},
    {mloc_calendarIndian, 739139, 1946, "shaka", 1946, 6, false, 19, 1, 366, 31},
    {mloc_calendarIndian, 775488, 2045, "shaka", 2045, 12, false, 29, 0, 365, 30},
    {mloc_calendarIndian, 775489, 2045, "shaka", 2045, 12, false, 30, 0, 365, 30},
    {mloc_calendarIndian, 775854, 2046, "shaka", 2046, 12, false, 29, 1, 366, 30},
    {mloc_calendarIndian, 811647, 2144, "shaka", 2144, 12, false, 29, 0, 365, 30},
    {mloc_calendarIndian, 850000, 2250, "shaka", 2250, 1, false, 2, 1, 366, 31},
    {mloc_calendarIndian, 100719163, 275682, "shaka", 275682, 6, false, 22, 1, 366, 31},
    {mloc_calendarPersian, -99280838, -272442, "ap", -272442, 1, false, 9, 0, 365, 31},
    {mloc_calendarPersian, -2000000, -6097, "ap", -6097, 12, false, 18, 0, 365, 29},
    {mloc_calendarPersian, 0, -621, "ap", -621, 10, false, 10, 0, 365, 30},
    {mloc_calendarPersian, 1, -621, "ap", -621, 10, false, 11, 0, 365, 30},
    {mloc_calendarPersian, 730120, 1378, "ap", 1378, 10, false, 11, 0, 365, 30},
    {mloc_calendarPersian, 738774, 1402, "ap", 1402, 6, false, 20, 0, 365, 31},
    {mloc_calendarPersian, 739000, 1403, "ap", 1403, 2, false, 5, 1, 366, 31},
    {mloc_calendarPersian, 739139, 1403, "ap", 1403, 6, false, 20, 1, 366, 31},
    {mloc_calendarPersian, 775488, 1502, "ap", 1502, 12, false, 29, 0, 365, 29},
    {mloc_calendarPersian, 775489, 1503, "ap", 1503, 1, false, 1, 1, 366, 31},
    {mloc_calendarPersian, 775854, 1503, "ap", 1503, 12, false, 30, 1, 366, 30},
    {mloc_calendarPersian, 811647, 1601, "ap", 1601, 12, false, 29, 0, 365, 29},
    {mloc_calendarPersian, 850000, 1707, "ap", 1707, 1, false, 2, 0, 365, 31},
    {mloc_calendarPersian, 100719163, 275139, "ap", 275139, 7, false, 12, 0, 365, 30},
    {mloc_calendarIslamicCivil, -99280838, -280804, "bh", 280805, 3, false, 21, 1, 355, 30},
    {mloc_calendarIslamicCivil, -2000000, -6284, "bh", 6285, 7, false, 4, 1, 355, 30},
    {mloc_calendarIslamicCivil, 0, -640, "bh", 641, 5, false, 17, 0, 354, 30},
    {mloc_calendarIslamicCivil, 1, -640, "bh", 641, 5, false, 18, 0, 354, 30},
    {mloc_calendarIslamicCivil, 730120, 1420, "ah", 1420, 9, false, 24, 1, 355, 30},
    {mloc_calendarIslamicCivil, 738774, 1445, "ah", 1445, 2, false, 25, 1, 355, 29},
    {mloc_calendarIslamicCivil, 739000, 1445, "ah", 1445, 10, false, 15, 1, 355, 29},
    {mloc_calendarIslamicCivil, 739139, 1446, "ah", 1446, 3, false, 6, 0, 354, 30},
    {mloc_calendarIslamicCivil, 775488, 1548, "ah", 1548, 10, false, 3, 1, 355, 29},
    {mloc_calendarIslamicCivil, 775489, 1548, "ah", 1548, 10, false, 4, 1, 355, 29},
    {mloc_calendarIslamicCivil, 775854, 1549, "ah", 1549, 10, false, 14, 0, 354, 29},
    {mloc_calendarIslamicCivil, 811647, 1650, "ah", 1650, 10, false, 16, 0, 354, 29},
    {mloc_calendarIslamicCivil, 850000, 1759, "ah", 1759, 1, false, 9, 0, 354, 30},
    {mloc_calendarIslamicCivil, 100719163, 283583, "ah", 283583, 5, false, 23, 0, 354, 30},
    {mloc_calendarIslamicTbla, -99280838, -280804, "bh", 280805, 3, false, 22, 1, 355, 30},
    {mloc_calendarIslamicTbla, -2000000, -6284, "bh", 6285, 7, false, 5, 1, 355, 30},
    {mloc_calendarIslamicTbla, 0, -640, "bh", 641, 5, false, 18, 0, 354, 30},
    {mloc_calendarIslamicTbla, 1, -640, "bh", 641, 5, false, 19, 0, 354, 30},
    {mloc_calendarIslamicTbla, 730120, 1420, "ah", 1420, 9, false, 25, 1, 355, 30},
    {mloc_calendarIslamicTbla, 738774, 1445, "ah", 1445, 2, false, 26, 1, 355, 29},
    {mloc_calendarIslamicTbla, 739000, 1445, "ah", 1445, 10, false, 16, 1, 355, 29},
    {mloc_calendarIslamicTbla, 739139, 1446, "ah", 1446, 3, false, 7, 0, 354, 30},
    {mloc_calendarIslamicTbla, 775488, 1548, "ah", 1548, 10, false, 4, 1, 355, 29},
    {mloc_calendarIslamicTbla, 775489, 1548, "ah", 1548, 10, false, 5, 1, 355, 29},
    {mloc_calendarIslamicTbla, 775854, 1549, "ah", 1549, 10, false, 15, 0, 354, 29},
    {mloc_calendarIslamicTbla, 811647, 1650, "ah", 1650, 10, false, 17, 0, 354, 29},
    {mloc_calendarIslamicTbla, 850000, 1759, "ah", 1759, 1, false, 10, 0, 354, 30},
    {mloc_calendarIslamicTbla, 100719163, 283583, "ah", 283583, 5, false, 24, 0, 354, 30},
    {mloc_calendarPersian, 740450, 1407, "ap", 1407, 1, false, 25, 0, 365, 31},
    {mloc_calendarIslamicCivil, 735900, 1437, "ah", 1437, 1, false, 15, 0, 354, 30},
    {mloc_calendarIslamicTbla, 735900, 1437, "ah", 1437, 1, false, 16, 0, 354, 30},
    {mloc_calendarHebrew, -99280838, -268058, "am", -268058, 11, false, 4, 0, 354, 30},
    {mloc_calendarHebrew, -2000000, -1715, "am", -1715, 7, false, 7, 1, 385, 30},
    {mloc_calendarHebrew, 0, 3761, "am", 3761, 4, false, 17, 0, 355, 29},
    {mloc_calendarHebrew, 1, 3761, "am", 3761, 4, false, 18, 0, 355, 29},
    {mloc_calendarHebrew, 730120, 5760, "am", 5760, 4, false, 23, 1, 385, 29},
    {mloc_calendarHebrew, 738774, 5783, "am", 5783, 12, false, 25, 0, 355, 29},
    {mloc_calendarHebrew, 739000, 5784, "am", 5784, 7, false, 16, 1, 383, 30},
    {mloc_calendarHebrew, 739139, 5784, "am", 5784, 12, false, 7, 1, 383, 29},
    {mloc_calendarHebrew, 775488, 5884, "am", 5884, 7, false, 4, 0, 354, 30},
    {mloc_calendarHebrew, 775489, 5884, "am", 5884, 7, false, 5, 0, 354, 30},
    {mloc_calendarHebrew, 775854, 5885, "am", 5885, 6, false, 14, 1, 385, 29},
    {mloc_calendarHebrew, 811647, 5983, "am", 5983, 6, false, 16, 1, 385, 29},
    {mloc_calendarHebrew, 850000, 6088, "am", 6088, 6, false, 9, 1, 385, 29},
    {mloc_calendarHebrew, 100719163, 279517, "am", 279517, 9, false, 11, 1, 383, 30},
    {mloc_calendarHebrew, 738550, 5783, "am", 5783, 5, false, 8, 0, 355, 30},
    {mloc_calendarHebrew, 738929, 5784, "am", 5784, 5, true, 4, 1, 383, 30},
    {mloc_calendarHebrew, 738930, 5784, "am", 5784, 5, true, 5, 1, 383, 30},
    {mloc_calendarHebrew, 738960, 5784, "am", 5784, 6, false, 5, 1, 383, 29},
    {mloc_calendarHebrew, 739320, 5785, "am", 5785, 6, false, 10, 0, 355, 29},
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
        bool same =
            Fields(c->calendar, c->day, &f) && f.year == c->year && f.eraYear == c->eraYear &&
            f.eraLength == strlen(c->era) && memcmp(f.era, c->era, f.eraLength) == 0 &&
            f.monthCodeLength == (c->leapMonth ? 4 : 3) && f.monthCode[1] == '0' + c->month / 10 &&
            f.monthCode[2] == '0' + c->month % 10 && f.day == c->dayOfMonth &&
            (f.inLeapYear != 0) == c->inLeapYear && f.daysInYear == c->daysInYear &&
            f.daysInMonth == c->daysInMonth;
        if (!same)
        {
            printf("calendar %d day %d\n", (int)c->calendar, (int)c->day);
            all = false;
        }
    }
    CHECK(all, "each calendar's fields, leap years and lengths as ICU4X gives them");
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
    };
    if (!Fields(calendar, day, &f))
    {
        return false;
    }
    era.eraLength = f.eraLength;
    era.monthCodeLength = f.monthCodeLength;
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
        mloc_calendarHebrew,
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

// Adar I, 'M05L', in a Hebrew leap year (5784) and a common one (5785).
static void TestHebrewLeapMonth(void)
{
    mlocDateInput adarI = {.year = 5784, .monthCode = "M05L", .monthCodeLength = 4, .day = 5};
    mlocDateInput adarII = {.year = 5784, .monthCode = "M06", .monthCodeLength = 3, .day = 5};
    mlocDateInput sixth = {
        .year = 5784, .monthCode = "M05L", .monthCodeLength = 4, .month = 6, .day = 5};
    mlocDateInput seventh = {
        .year = 5784, .monthCode = "M05L", .monthCodeLength = 4, .month = 7, .day = 5};
    mlocDateInput common = {.year = 5785, .monthCode = "M05L", .monthCodeLength = 4, .day = 10};
    mlocDateInput thirteenth = {.year = 5785, .month = 13, .day = 1};
    mlocDateInput otherLeap = {.year = 5784, .monthCode = "M06L", .monthCodeLength = 4, .day = 1};
    CHECK(Made(mloc_calendarHebrew, &adarI, mloc_overflowReject, mloc_success, 738930) &&
              Made(mloc_calendarHebrew, &adarII, mloc_overflowReject, mloc_success, 738960) &&
              Made(mloc_calendarHebrew, &sixth, mloc_overflowReject, mloc_success, 738930) &&
              Made(mloc_calendarHebrew, &seventh, mloc_overflowReject, mloc_errorInvalid, 0) &&
              Made(mloc_calendarHebrew, &common, mloc_overflowConstrain, mloc_success, 739320) &&
              Made(mloc_calendarHebrew, &common, mloc_overflowReject, mloc_errorInvalid, 0) &&
              Made(mloc_calendarHebrew, &thirteenth, mloc_overflowReject, mloc_errorInvalid, 0) &&
              Made(mloc_calendarHebrew, &otherLeap, mloc_overflowConstrain, mloc_errorSyntax, 0),
          "Hebrew's leap month: the sixth place in a leap year, Adar in a common one or "
          "refused; no other leap month");
}

static void TestEras(void)
{
    mlocDateInput bc = {.era = "bc", .eraLength = 2, .year = 1, .month = 12, .day = 31};
    mlocDateInput bce = {.era = "bce", .eraLength = 3, .year = 1, .month = 12, .day = 31};
    mlocDateInput broc = {.era = "broc", .eraLength = 4, .year = 1911, .month = 1, .day = 1};
    mlocDateInput aa = {.era = "aa", .eraLength = 2, .year = 5493, .month = 5, .day = 8};
    mlocDateInput bh = {.era = "bh", .eraLength = 2, .year = 641, .month = 5, .day = 18};
    mlocDateInput other = {.era = "ah", .eraLength = 2, .year = 1, .month = 1, .day = 1};
    mlocDateInput empty = {.era = "", .year = 1, .month = 1, .day = 1};
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
              Made(mloc_calendarRoc, &empty, mloc_overflowReject, mloc_errorSyntax, 0) &&
              Made(mloc_calendarIso8601, &bce, mloc_overflowReject, mloc_errorSyntax, 0) &&
              Made(mloc_calendarGregorian, &leap, mloc_overflowConstrain, mloc_errorSyntax, 0) &&
              Made(mloc_calendarGregorian, &bad, mloc_overflowConstrain, mloc_errorSyntax, 0) &&
              Made(mloc_calendarGregorian, &both, mloc_overflowConstrain, mloc_errorInvalid, 0),
          "eras and their aliases; an era of another calendar or empty; month codes leap, "
          "malformed, or naming another month than the place");
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
            Made(mloc_calendarJapanese, &ok, mloc_overflowConstrain, mloc_errorNotFound, 0) &&
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
    TestHebrewLeapMonth();
    TestEras();
    TestFields();
    TestRefusals();
    return s_failures == 0 ? 0 : 1;
}
