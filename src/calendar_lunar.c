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

// The table year at an index from 1600.
static void Unpack(bool korea, size_t index, LunisolarYear* out)
{
    uint32_t bits = (korea ? mlocDangiYears : mlocChineseYears)[index];
    int64_t year = lunarFirstYear + (int64_t)index;
    out->year = (int32_t)year;
    out->lunation = (korea ? mlocDangiFirstLunation : mlocChineseFirstLunation) +
                    (korea ? mlocDangiMonthsBefore : mlocChineseMonthsBefore)[index];
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
        Unpack(korea, (size_t)(year - lunarFirstYear), out);
        return;
    }
    // Before its new year a day is in the year before, if the table
    // holds it and the day is not past its end (January of 2401 is).
    if (InTable(year - 1))
    {
        Unpack(korea, (size_t)(year - 1 - lunarFirstYear), out);
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
        Unpack(korea, (size_t)(year - lunarFirstYear), out);
    }
    else
    {
        mlocChineseModelYear(korea, year, out);
    }
}

bool mlocLunarYearOfLunation(bool korea, int64_t lunation, LunisolarYear* out)
{
    const uint16_t* before = korea ? mlocDangiMonthsBefore : mlocChineseMonthsBefore;
    int64_t first = korea ? mlocDangiFirstLunation : mlocChineseFirstLunation;
    LunisolarYear last;
    Unpack(korea, lunarYearCount - 1, &last);
    if (lunation >= first && lunation < (int64_t)last.lunation + last.count)
    {
        // The last table year whose first month is at or before it.
        size_t low = 0;
        size_t high = lunarYearCount;
        while (high - low > 1)
        {
            size_t middle = low + (high - low) / 2;
            if (first + before[middle] <= lunation)
            {
                low = middle;
            }
            else
            {
                high = middle;
            }
        }
        Unpack(korea, low, out);
        return true;
    }
    if (lunation < lunarMinLunation || lunation > lunarMaxLunation)
    {
        return false;
    }
    mlocChineseModelYearOf(korea, (int32_t)mlocChineseModelMonthStart(korea, lunation), out);
    return true;
}
