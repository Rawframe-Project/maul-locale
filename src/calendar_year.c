// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A year of any calendar (record mloc-0040), over the calendars' files.

#include "calendar_year.h"

#include "calendar_hebrew.h"
#include "calendar_umalqura.h"

CalendarSource mlocYearSource(mlocCalendar calendar)
{
    switch (calendar)
    {
    case mloc_calendarHebrew:
        return sourceHebrew;
    case mloc_calendarIslamicUmalqura:
    case mloc_calendarIslamic:
    case mloc_calendarIslamicRgsa:
        return sourceUmalqura;
    case mloc_calendarChinese:
        return sourceChinese;
    case mloc_calendarDangi:
        return sourceDangi;
    default:
        return sourceArithmetic;
    }
}

bool mlocYearLunisolar(CalendarSource source)
{
    return source == sourceChinese || source == sourceDangi;
}

void mlocYearOpen(CalendarYear* y, mlocCalendar calendar, int64_t year)
{
    y->calendar = calendar;
    y->source = mlocYearSource(calendar);
    y->year = year;
    if (mlocYearLunisolar(y->source))
    {
        mlocLunarYear(y->source == sourceDangi, year, &y->lunar);
    }
}

uint32_t mlocYearMonths(const CalendarYear* y)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewMonths(y->year);
    case sourceChinese:
    case sourceDangi:
        return y->lunar.count;
    default:
        return mlocArithmeticMonths(y->calendar);
    }
}

uint32_t mlocYearDaysInMonth(const CalendarYear* y, uint32_t month)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewDaysInMonth(y->year, month);
    case sourceUmalqura:
        return mlocUmalquraDaysInMonth(y->year, month);
    case sourceChinese:
    case sourceDangi:
        return (uint32_t)(y->lunar.start[month] - y->lunar.start[month - 1]);
    default:
        return mlocArithmeticDaysInMonth(y->calendar, y->year, month);
    }
}

int64_t mlocYearDayOf(const CalendarYear* y, uint32_t month, uint32_t day)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewToDay(y->year, month, day);
    case sourceUmalqura:
        return mlocUmalquraToDay(y->year, month, day);
    case sourceChinese:
    case sourceDangi:
        return (int64_t)y->lunar.start[month - 1] + day - 1;
    default:
        return mlocArithmeticToDay(y->calendar, y->year, month, day);
    }
}

bool mlocYearLeap(const CalendarYear* y)
{
    switch (y->source)
    {
    case sourceHebrew:
        return mlocHebrewLeap(y->year);
    case sourceUmalqura:
        return mlocUmalquraLeap(y->year);
    case sourceChinese:
    case sourceDangi:
        return y->lunar.count == 13;
    default:
        return mlocArithmeticLeap(y->calendar, y->year);
    }
}

uint32_t mlocYearLeapMonth(const CalendarYear* y)
{
    if (y->source == sourceHebrew)
    {
        return mlocHebrewLeap(y->year) ? 6 : 0;
    }
    return mlocYearLunisolar(y->source) ? y->lunar.leap : 0;
}

int64_t mlocYearDays(const CalendarYear* y)
{
    if (mlocYearLunisolar(y->source))
    {
        return y->lunar.start[y->lunar.count] - y->lunar.start[0];
    }
    CalendarYear next;
    mlocYearOpen(&next, y->calendar, y->year + 1);
    return mlocYearDayOf(&next, 1, 1) - mlocYearDayOf(y, 1, 1);
}

CalendarDate mlocYearOpenOf(CalendarYear* y, mlocCalendar calendar, int32_t day)
{
    CalendarSource source = mlocYearSource(calendar);
    if (!mlocYearLunisolar(source))
    {
        CalendarDate d = source == sourceHebrew     ? mlocHebrewFromDay(day)
                         : source == sourceUmalqura ? mlocUmalquraFromDay(day)
                                                    : mlocArithmeticFromDay(calendar, day);
        mlocYearOpen(y, calendar, d.year);
        return d;
    }
    y->calendar = calendar;
    y->source = source;
    mlocLunarYearOf(source == sourceDangi, day, &y->lunar);
    y->year = y->lunar.year;
    uint32_t month = 1;
    while (y->lunar.start[month] <= day)
    {
        month++;
    }
    return (CalendarDate){y->lunar.year, (uint8_t)month,
                          (uint8_t)(day - y->lunar.start[month - 1] + 1)};
}

void mlocYearCodeOf(const CalendarYear* y, uint32_t month, uint32_t* numberOut, bool* leapOut)
{
    uint32_t leap = mlocYearLeapMonth(y);
    *leapOut = leap != 0 && month == leap;
    *numberOut = leap != 0 && month >= leap ? month - 1 : month;
}

bool mlocYearCodeExists(const CalendarYear* y, uint32_t number, bool leap)
{
    switch (y->source)
    {
    case sourceHebrew:
        return number <= 12 && (!leap || number == 5);
    case sourceChinese:
    case sourceDangi:
        return number <= 12;
    default:
        return !leap && number <= mlocArithmeticMonths(y->calendar);
    }
}

mlocResult mlocYearMonthOfCode(const CalendarYear* y, uint32_t number, bool leap,
                               mlocOverflow overflow, uint32_t* monthOut)
{
    if (!mlocYearCodeExists(y, number, leap))
    {
        return mloc_errorSyntax;
    }
    uint32_t leapMonth = mlocYearLeapMonth(y);
    if (leap && leapMonth != 0 && number == leapMonth - 1)
    {
        *monthOut = leapMonth;
        return mloc_success;
    }
    if (leap && overflow == mloc_overflowReject)
    {
        return mloc_errorInvalid;
    }
    if (leap && y->source == sourceHebrew)
    {
        number++;
    }
    *monthOut = leapMonth != 0 && number >= leapMonth ? number + 1 : number;
    return mloc_success;
}

bool mlocYearDateValid(const mlocDate* date)
{
    return date != nullptr && date->calendar < yearCalendarCount && date->day >= mlocDateMinDay &&
           date->day <= mlocDateMaxDay && date->reserved[0] == 0 && date->reserved[1] == 0 &&
           date->reserved[2] == 0;
}

static const int64_t s_monthNumberYears = INT64_C(1) << 34;

static int64_t FloorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return q - (a % b != 0 && (a < 0) != (b < 0));
}

// The Hebrew months before a year's Tishrei, from the epoch.
static int64_t HebrewMonthsBefore(int64_t year)
{
    return FloorDiv(235 * year - 234, 19);
}

int64_t mlocYearMonthNumber(const CalendarYear* y, uint32_t month)
{
    switch (y->source)
    {
    case sourceHebrew:
        return HebrewMonthsBefore(y->year) + month - 1;
    case sourceChinese:
    case sourceDangi:
        return (int64_t)y->lunar.lunation + month - 1;
    default:
        return y->year * mlocYearMonths(y) + month - 1;
    }
}

bool mlocYearOpenMonthNumber(CalendarYear* y, mlocCalendar calendar, int64_t number,
                             uint32_t* monthOut)
{
    CalendarSource source = mlocYearSource(calendar);
    if (mlocYearLunisolar(source))
    {
        y->calendar = calendar;
        y->source = source;
        if (!mlocLunarYearOfLunation(source == sourceDangi, number, &y->lunar))
        {
            return false;
        }
        y->year = y->lunar.year;
        *monthOut = (uint32_t)(number - y->lunar.lunation + 1);
        return true;
    }
    int64_t year = 0;
    if (source == sourceHebrew)
    {
        if (number > 13 * s_monthNumberYears || number < -13 * s_monthNumberYears)
        {
            return false;
        }
        // A mean year is 235 / 19 months; the estimate is off by one at
        // most.
        year = FloorDiv(19 * number, 235) + 1;
        while (HebrewMonthsBefore(year) > number)
        {
            year--;
        }
        while (HebrewMonthsBefore(year + 1) <= number)
        {
            year++;
        }
        mlocYearOpen(y, calendar, year);
        *monthOut = (uint32_t)(number - HebrewMonthsBefore(year) + 1);
        return true;
    }
    mlocYearOpen(y, calendar, 0);
    int64_t months = mlocYearMonths(y);
    year = FloorDiv(number, months);
    if (year > s_monthNumberYears || year < -s_monthNumberYears)
    {
        return false;
    }
    mlocYearOpen(y, calendar, year);
    *monthOut = (uint32_t)(number - year * months + 1);
    return true;
}
