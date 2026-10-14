// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Dangi years the dates use (record mloc-0040): the
// generated tables from 1600 to 2400, the model outside them.

#ifndef MLOC_CALENDAR_LUNAR_H
#define MLOC_CALENDAR_LUNAR_H

#include "calendar_chinese.h"

enum
{
    // The Chinese and Dangi years the dates use: those holding
    // Temporal's first and last days, and one more each side.
    lunarMinYear = -271822,
    lunarMaxYear = 275761,
    // The numbers of the new moons beginning the first month of
    // lunarMinYear and the last of lunarMaxYear, the same in both
    // calendars.
    lunarMinLunation = -3363514,
    lunarMaxLunation = 3409364,
};

// The year holding a day; korea selects Dangi.
void mlocLunarYearOf(bool korea, int32_t day, LunisolarYear* out);

// The year numbered year, for a year within the dates' range and one more
// each side.
void mlocLunarYear(bool korea, int64_t year, LunisolarYear* out);

// The year holding the month the numbered new moon begins; false for a
// new moon outside the years from lunarMinYear to lunarMaxYear.
bool mlocLunarYearOfLunation(bool korea, int64_t lunation, LunisolarYear* out);

#endif
