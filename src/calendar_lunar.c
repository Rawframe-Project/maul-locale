// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Dangi years the dates use (calendar_lunar.h).

#include "calendar_lunar.h"

#include "calendar_arith.h"
#include "calendar_years.h"

static bool InTable(int64_t year)
{
    return year >= lunarFirstYear && year <= lunarLastYear;
}

static void Unpack(bool korea, int64_t year, LunisolarYear* out)
{
    uint32_t bits = (korea ? mlocDangiYears : mlocChineseYears)[year - lunarFirstYear];
    out->year = (int32_t)year;
    out->leap = (uint8_t)((bits >> lunarLeapShift) & 0xF);
    out->count = out->leap != 0 ? 13 : 12;
    out->start[0] = (int32_t)(mlocArithmeticToDay(mloc_calendarGregorian, year, 1, 1) +
                              (bits >> lunarOffsetShift));
    for (uint32_t m = 0; m < out->count; m++)
    {
        out->start[m + 1] = out->start[m] + 29 + (int32_t)((bits >> m) & 1);
    }
}

// The first day of a table year.
static int64_t NewYear(bool korea, int64_t year)
{
    uint32_t bits = (korea ? mlocDangiYears : mlocChineseYears)[year - lunarFirstYear];
    return mlocArithmeticToDay(mloc_calendarGregorian, year, 1, 1) + (bits >> lunarOffsetShift);
}

void mlocLunarYearOf(bool korea, int32_t day, LunisolarYear* out)
{
    int64_t year = mlocArithmeticFromDay(mloc_calendarGregorian, day).year;
    if (InTable(year) && day >= NewYear(korea, year))
    {
        Unpack(korea, year, out);
        return;
    }
    // Before its new year a day is in the year before, if the table
    // holds it and the day is not past its end (January of 2401 is).
    if (InTable(year - 1))
    {
        Unpack(korea, year - 1, out);
        if (day < out->start[out->count])
        {
            return;
        }
    }
    mlocChineseModelYearOf(korea, day, out);
}

void mlocLunarYear(bool korea, int64_t year, LunisolarYear* out)
{
    if (InTable(year))
    {
        Unpack(korea, year, out);
    }
    else
    {
        mlocChineseModelYear(korea, year, out);
    }
}
