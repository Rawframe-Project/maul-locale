// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Dangi years from 1600 to 2400 (record mloc-0040),
// which the generator writes to src/generated/calendar_years.c from the
// model in calendar_chinese.h and the overrides in
// tools/calendar-overrides.txt. A year is 24 bits of a uint32_t:
//   bits 0 to 12   bit m - 1 set for a month m of 30 days, else 29
//   bits 13 to 16  the leap month's place, from 1, or 0 in a year of 12
//   bits 17 to 23  the first day's offset from January 1 of the year

#ifndef MLOC_CALENDAR_YEARS_H
#define MLOC_CALENDAR_YEARS_H

#include <stdint.h>

enum
{
    lunarFirstYear = 1600,
    lunarLastYear = 2400,
    lunarYearCount = lunarLastYear - lunarFirstYear + 1,
    lunarLeapShift = 13,
    lunarOffsetShift = 17,
};

extern const uint32_t mlocChineseYears[lunarYearCount];
extern const uint32_t mlocDangiYears[lunarYearCount];

#endif
