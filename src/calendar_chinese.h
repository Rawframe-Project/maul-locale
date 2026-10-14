// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Korean (Dangi) calendars (record mloc-0040), after
// Calendrical Calculations: a month begins on the day of a new moon in
// China's or Korea's time, the eleventh month holds the winter solstice,
// and in a solstice-to-solstice span of thirteen months the first month
// with no major solar term is the leap month. Built on calendar_astro.h.
// A year is numbered by the Gregorian year whose July 1 it holds, as
// Temporal and ICU4X number it.

#ifndef MLOC_CALENDAR_CHINESE_H
#define MLOC_CALENDAR_CHINESE_H

#include <stdint.h>

// A year: the first day of each month, its start[count] the next year's
// first day.
typedef struct LunisolarYear
{
    int32_t year;
    int32_t start[14];
    // 12 or 13.
    uint8_t count;
    // The leap month's place, from 1, or 0 in a year of twelve.
    uint8_t leap;
} LunisolarYear;

// The model's year holding a day; korea selects Dangi. The dates' own
// years come from calendar_lunar.h, which reads the tables for 1600 to
// 2400 and calls these outside them.
void mlocChineseModelYearOf(bool korea, int32_t day, LunisolarYear* out);

// The model's year numbered year: the one holding July 1 of that
// Gregorian year.
void mlocChineseModelYear(bool korea, int64_t year, LunisolarYear* out);

#endif
