// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The difference of two dates (maul-locale/calendar.h): the Intl Era and
// Month Code proposal's NonISODateUntil. Its loops of one unit at a time
// are counted directly: years from the year difference less one, which
// never passes the second date, and one or two checks by
// NonISODateSurpasses; months as the difference of month numbers
// (calendar_year.h); weeks and days as days.

#include "calendar_year.h"
#include "invariant.h"

#include "maul-locale/calendar.h"

// A date's year, month code (its number and leap flag), month and day.
typedef struct Parts
{
    int64_t year;
    uint32_t number;
    bool leap;
    uint32_t month;
    uint32_t day;
} Parts;

static Parts PartsOf(CalendarYear* y, mlocCalendar calendar, int32_t day)
{
    CalendarDate d = mlocYearOpenOf(y, calendar, day);
    Parts p = {.year = y->year, .month = d.month, .day = d.day};
    mlocYearCodeOf(y, d.month, &p.number, &p.leap);
    return p;
}

// Which of two values passes the other in the difference's direction:
// sign times their difference, as a sign.
static int Past(int sign, int64_t a, int64_t b)
{
    int64_t d = (a > b) - (a < b);
    return sign * (int)d;
}

// Month codes compare as their text does: by number, then 'M05' before
// 'M05L'.
static int64_t CodeOrder(uint32_t number, bool leap)
{
    return (int64_t)number * 2 + (leap ? 1 : 0);
}

// NonISODateSurpasses with years alone: whether the first date with
// years added (a date that need not exist) passes the second, whose year
// is target.
static bool Surpasses(int sign, const Parts* one, const Parts* two, const CalendarYear* target,
                      int64_t years)
{
    int64_t y0 = one->year + years;
    // CompareSurpasses on the year and the month code. Equal codes leave
    // the day, which the comparison by place below makes as well.
    int c = Past(sign, y0, two->year);
    if (c != 0)
    {
        return c > 0;
    }
    if (Past(sign, CodeOrder(one->number, one->leap), CodeOrder(two->number, two->leap)) > 0)
    {
        return true;
    }
    // In the second date's own year, the month code constrained there is
    // compared by place: Hebrew's Adar I code is Adar's place in a common
    // year.
    uint32_t m0 = 0;
    if (mlocYearMonthOfCode(target, one->number, one->leap, mloc_overflowConstrain, &m0) !=
        mloc_success)
    {
        return false;
    }
    c = Past(sign, m0, two->month);
    return c > 0 || (c == 0 && Past(sign, one->day, two->day) > 0);
}

mlocResult mlocDateUntil(const mlocDate* one, const mlocDate* two, mlocDateUnit largestUnit,
                         mlocDateDuration* durationOut)
{
    if (!mlocYearDateValid(one) || !mlocYearDateValid(two) || durationOut == nullptr ||
        one->calendar != two->calendar || (uint32_t)largestUnit > mloc_dateUnitDay)
    {
        return mloc_errorInvalid;
    }
    int64_t span = (int64_t)two->day - one->day;
    mlocDateDuration d = {0};
    if (largestUnit == mloc_dateUnitDay || largestUnit == mloc_dateUnitWeek || span == 0)
    {
        d.weeks = largestUnit == mloc_dateUnitWeek ? span / 7 : 0;
        d.days = span - 7 * d.weeks;
        *durationOut = d;
        return mloc_success;
    }
    int sign = span > 0 ? 1 : -1;
    mlocCalendar calendar = (mlocCalendar)one->calendar;
    CalendarYear first;
    CalendarYear target;
    Parts a = PartsOf(&first, calendar, one->day);
    Parts b = PartsOf(&target, calendar, two->day);
    if (largestUnit == mloc_dateUnitYear)
    {
        // The year difference less one never passes; the next one or two
        // candidates settle it.
        int64_t candidate = b.year == a.year ? sign : b.year - a.year - sign;
        candidate = candidate == 0 ? sign : candidate;
        while (!Surpasses(sign, &a, &b, &target, candidate))
        {
            d.years = candidate;
            candidate += sign;
        }
    }
    // The months: the month numbers' difference, less one where the day
    // would pass. The years did not pass the second date, so this is
    // never against the sign.
    CalendarYear y;
    mlocYearOpen(&y, calendar, a.year + d.years);
    uint32_t m0 = 0;
    if (mlocYearMonthOfCode(&y, a.number, a.leap, mloc_overflowConstrain, &m0) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    int64_t months = mlocYearMonthNumber(&target, b.month) - mlocYearMonthNumber(&y, m0);
    if (Past(sign, a.day, b.day) > 0)
    {
        months -= sign;
    }
    d.months = months;
    // The days from the date the months reach, its day clamped. That
    // date is not past the second (its day is at most the one the months
    // were judged by), so the days are never against the sign.
    uint32_t month = m0;
    if (d.months != 0 &&
        !mlocYearOpenMonthNumber(&y, calendar, mlocYearMonthNumber(&y, m0) + d.months, &month))
    {
        return mloc_errorInvalid;
    }
    uint32_t last = mlocYearDaysInMonth(&y, month);
    int64_t reached = mlocYearDayOf(&y, month, a.day < last ? a.day : last);
    d.days = two->day - reached;
    // No field goes against the sign, which is 1 or -1.
    MLOC_ASSERT(d.years * sign >= 0 && d.months * sign >= 0 && d.days * sign >= 0);
    *durationOut = d;
    return mloc_success;
}
