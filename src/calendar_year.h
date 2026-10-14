// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A year of any calendar (record mloc-0040): its months, their lengths
// and codes, and its days, over each calendar's own arithmetic. Making
// dates and the date arithmetic share it.

#ifndef MLOC_CALENDAR_YEAR_H
#define MLOC_CALENDAR_YEAR_H

#include "calendar_arith.h"
#include "calendar_chinese.h"
#include "calendar_lunar.h"

#include "maul-locale/calendar.h"

#include <stdint.h>

// The file that has a calendar's arithmetic.
typedef enum CalendarSource
{
    sourceArithmetic,
    sourceHebrew,
    sourceUmalqura,
    sourceChinese,
    sourceDangi,
} CalendarSource;

// A year of a calendar, with the Chinese and Dangi months computed once.
typedef struct CalendarYear
{
    mlocCalendar calendar;
    CalendarSource source;
    int64_t year;
    LunisolarYear lunar;
} CalendarYear;

enum
{
    yearCalendarCount = 18,
};

// Whether a date is one the library made: not null, a known calendar, a
// day in Temporal's range, reserved bytes zero.
bool mlocYearDateValid(const mlocDate* date);

CalendarSource mlocYearSource(mlocCalendar calendar);
bool mlocYearLunisolar(CalendarSource source);

void mlocYearOpen(CalendarYear* y, mlocCalendar calendar, int64_t year);
uint32_t mlocYearMonths(const CalendarYear* y);
uint32_t mlocYearDaysInMonth(const CalendarYear* y, uint32_t month);
int64_t mlocYearDayOf(const CalendarYear* y, uint32_t month, uint32_t day);
// A leap day or a leap month.
bool mlocYearLeap(const CalendarYear* y);
// The leap month's place in the year, 0 for none: Hebrew's Adar I, the
// sixth month of a leap year, and the Chinese or Dangi leap month.
uint32_t mlocYearLeapMonth(const CalendarYear* y);
int64_t mlocYearDays(const CalendarYear* y);

// The year holding a day, and the day's month and day in it.
CalendarDate mlocYearOpenOf(CalendarYear* y, mlocCalendar calendar, int32_t day);

// A month as one number, so that adding months is an addition: months
// from year 0's first month in the calendars with a fixed count, from
// the Hebrew epoch's in Hebrew, and the number of the new moon that
// begins it in Chinese and Dangi (calendar_astro.h's).
int64_t mlocYearMonthNumber(const CalendarYear* y, uint32_t month);

// Opens the year holding a month number and gives the month's place;
// false for a month past the years the calendar is computed for: 2^34
// years either way in the arithmetic calendars, beyond any a valid
// Temporal duration reaches (2^33), and lunarMinYear to lunarMaxYear in
// Chinese and Dangi.
bool mlocYearOpenMonthNumber(CalendarYear* y, mlocCalendar calendar, int64_t number,
                             uint32_t* monthOut);

// A month's code: its number, and whether it is the leap month, which
// takes the number of the month before it ('M05L' after 'M05'); the
// months after a leap month take the code of the place before theirs.
void mlocYearCodeOf(const CalendarYear* y, uint32_t month, uint32_t* numberOut, bool* leapOut);
// Whether a code is one some year of the calendar has.
bool mlocYearCodeExists(const CalendarYear* y, uint32_t number, bool leap);
// The month a code names in a year: mloc_success; for a leap month the
// year lacks, the month Temporal puts in its place (Hebrew's Adar,
// 'M06'; the Chinese month of the same number) or mloc_errorInvalid
// (reject); mloc_errorSyntax for a code no year of the calendar has.
mlocResult mlocYearMonthOfCode(const CalendarYear* y, uint32_t number, bool leap,
                               mlocOverflow overflow, uint32_t* monthOut);

#endif
