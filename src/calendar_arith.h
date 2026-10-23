// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic calendars: a day to its year, month and day and back,
// after Reingold and Dershowitz's Calendrical Calculations, with Persian's
// 33-year rule and its corrections as both ICUs have them. Days are those
// of maul-locale/calendar.h; years are counted on across eras. Every
// function takes a Gregorian-based, Coptic, Ethiopic, Indian, Persian or
// tabular Islamic calendar (the other calendars' files use the Gregorian)
// and values in Temporal's range.

#ifndef MLOC_CALENDAR_ARITH_H
#define MLOC_CALENDAR_ARITH_H

#include "maul-locale/calendar.h"

#include <stdint.h>

// A date's year (counted on across eras), month (its place in the year,
// from 1) and day.
typedef struct CalendarDate
{
    int32_t year;
    uint8_t month;
    uint8_t day;
} CalendarDate;

CalendarDate mlocArithmeticFromDay(mlocCalendar calendar, int32_t day);

// The day of a valid year, month and day.
int64_t mlocArithmeticToDay(mlocCalendar calendar, int64_t year, uint32_t month, uint32_t day);

uint32_t mlocArithmeticMonths(mlocCalendar calendar);
uint32_t mlocArithmeticDaysInMonth(mlocCalendar calendar, int64_t year, uint32_t month);
bool mlocArithmeticLeap(mlocCalendar calendar, int64_t year);

#endif
