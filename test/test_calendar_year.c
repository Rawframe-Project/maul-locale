// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A year of any calendar (record mloc-0040): months numbered on from
// year to year in every calendar, each number opening its month's year
// again, and the arithmetic calendars' years whole on 64-bit days as
// far as a Temporal duration reaches (2^33 years).

#include "calendar_year.h"
#include "test_harness.h"

enum
{
    calendarCount = 18,
};

static const int64_t s_far = INT64_C(1) << 33;

// The years each calendar is sampled at: around year 0 and 1, recent
// years, Temporal's ends, and for the arithmetic calendars 2^33.
static void SampleYears(mlocCalendar calendar, int64_t* years, size_t* countOut)
{
    CalendarSource source = mlocYearSource(calendar);
    size_t n = 0;
    const int64_t common[] = {-2, -1, 0, 1, 2, 1600, 1972, 2400, 5779, 5784};
    for (size_t i = 0; i < sizeof(common) / sizeof(common[0]); i++)
    {
        years[n++] = common[i];
    }
    if (mlocYearLunisolar(source))
    {
        years[n++] = lunarMinYear;
        years[n++] = lunarMaxYear - 1;
    }
    else
    {
        years[n++] = -s_far;
        years[n++] = s_far;
        years[n++] = -275000;
        years[n++] = 275000;
    }
    *countOut = n;
}

// Each month's number follows the one before, the last month of a year
// is followed by the next year's first, and each number opens its year
// and month again.
static void TestMonthNumbers(void)
{
    bool all = true;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        mlocCalendar calendar = (mlocCalendar)c;
        int64_t years[16];
        size_t count = 0;
        SampleYears(calendar, years, &count);
        for (size_t i = 0; i < count; i++)
        {
            CalendarYear y;
            CalendarYear next;
            mlocYearOpen(&y, calendar, years[i]);
            mlocYearOpen(&next, calendar, years[i] + 1);
            uint32_t months = mlocYearMonths(&y);
            for (uint32_t m = 1; m <= months; m++)
            {
                int64_t number = mlocYearMonthNumber(&y, m);
                int64_t following =
                    m < months ? mlocYearMonthNumber(&y, m + 1) : mlocYearMonthNumber(&next, 1);
                CalendarYear opened;
                uint32_t month = 0;
                bool ok = mlocYearOpenMonthNumber(&opened, calendar, number, &month);
                all = all && following == number + 1 && ok && opened.year == y.year && month == m;
            }
        }
    }
    CHECK(all, "months numbered on across years, and each number opening its month");
}

// The calendars with a fixed count of months and Hebrew number months
// as the book does; Chinese and Dangi by their new moons.
static void TestKnownNumbers(void)
{
    CalendarYear y;
    mlocYearOpen(&y, mloc_calendarGregorian, 2026);
    CHECK(mlocYearMonthNumber(&y, 10) == 2026 * 12 + 9, "Gregorian: twelve months a year");
    mlocYearOpen(&y, mloc_calendarCoptic, 1742);
    CHECK(mlocYearMonthNumber(&y, 13) == 1742 * 13 + 12, "Coptic: thirteen months a year");
    mlocYearOpen(&y, mloc_calendarHebrew, 5779);
    CHECK(mlocYearMonthNumber(&y, 1) == (235 * 5779 - 234) / 19,
          "Hebrew: the months elapsed before Tishrei");
    mlocYearOpen(&y, mloc_calendarHebrew, s_far);
    CHECK(mlocYearMonthNumber(&y, 1) == INT64_C(106243927836), "Hebrew at 2^33");
    mlocYearOpen(&y, mloc_calendarHebrew, -s_far);
    CHECK(mlocYearMonthNumber(&y, 1) == INT64_C(-106243927861), "Hebrew at -2^33");
    mlocYearOpen(&y, mloc_calendarChinese, 2026);
    CHECK(mlocYearMonthNumber(&y, 3) == y.lunar.lunation + 2, "Chinese: the new moon's number");
}

// Past the years each calendar is computed for, no year is opened.
static void TestRefusals(void)
{
    CalendarYear y;
    uint32_t month = 0;
    CHECK(!mlocYearOpenMonthNumber(&y, mloc_calendarGregorian, (INT64_C(1) << 34) * 12 + 12,
                                   &month) &&
              !mlocYearOpenMonthNumber(&y, mloc_calendarGregorian, -(INT64_C(1) << 34) * 12 - 1,
                                       &month) &&
              !mlocYearOpenMonthNumber(&y, mloc_calendarHebrew, INT64_C(1) << 40, &month) &&
              !mlocYearOpenMonthNumber(&y, mloc_calendarHebrew, -(INT64_C(1) << 40), &month) &&
              !mlocYearOpenMonthNumber(&y, mloc_calendarChinese, lunarMaxLunation + 1, &month) &&
              !mlocYearOpenMonthNumber(&y, mloc_calendarDangi, lunarMinLunation - 1, &month),
          "no year past the computed ones");
    CHECK(
        mlocYearOpenMonthNumber(&y, mloc_calendarGregorian, (INT64_C(1) << 34) * 12 + 11, &month) &&
            y.year == INT64_C(1) << 34 && month == 12,
        "the last arithmetic month opened");
}

// Years of the arithmetic calendars at 2^33 on 64-bit days: each year's
// length is its months' and joins the next year's first day, and the
// Gregorian day is the book's formula, computed in big integers.
static void TestFarYears(void)
{
    bool all = true;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        mlocCalendar calendar = (mlocCalendar)c;
        if (mlocYearLunisolar(mlocYearSource(calendar)))
        {
            continue;
        }
        const int64_t years[] = {-s_far - 1, -s_far, s_far - 1, s_far};
        for (size_t i = 0; i < sizeof(years) / sizeof(years[0]); i++)
        {
            CalendarYear y;
            CalendarYear next;
            mlocYearOpen(&y, calendar, years[i]);
            mlocYearOpen(&next, calendar, years[i] + 1);
            int64_t sum = 0;
            for (uint32_t m = 1; m <= mlocYearMonths(&y); m++)
            {
                uint32_t days = mlocYearDaysInMonth(&y, m);
                all = all && days >= 5 && days <= 31 &&
                      mlocYearDayOf(&y, m, 1) == mlocYearDayOf(&y, 1, 1) + sum;
                sum += days;
            }
            int64_t length = mlocYearDayOf(&next, 1, 1) - mlocYearDayOf(&y, 1, 1);
            all = all && sum == length && length >= 353 && length <= 385;
        }
    }
    CHECK(all, "years at 2^33 whole, their months joining the next year");
    CalendarYear y;
    mlocYearOpen(&y, mloc_calendarGregorian, s_far);
    CHECK(mlocYearDayOf(&y, 1, 1) == INT64_C(3137409184854), "Gregorian January 1 of 2^33");
    mlocYearOpen(&y, mloc_calendarGregorian, -s_far);
    CHECK(mlocYearDayOf(&y, 1, 1) == INT64_C(-3137409185584), "Gregorian January 1 of -2^33");
}

int main(void)
{
    TestMonthNumbers();
    TestKnownNumbers();
    TestRefusals();
    TestFarYears();
    return s_failures == 0 ? 0 : 1;
}
