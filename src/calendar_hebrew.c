// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Hebrew calendar (calendar_hebrew.h).

#include "calendar_hebrew.h"

enum
{
    // Tishrei 1 of year 1: October 7 of 3761 BCE in the Julian calendar.
    hebrewEpoch = -1373427,
    // Parts in a day (1,080 an hour); the molad of Tishrei of year 1 as
    // Calendrical Calculations counts it; a month is 29 days 13753
    // parts.
    partsPerDay = 25920,
    firstMolad = 12084,
    monthParts = 13753,
};

static int64_t FloorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return q - (a % b != 0 && (a < 0) != (b < 0));
}

static int64_t FloorMod(int64_t a, int64_t b)
{
    return a - FloorDiv(a, b) * b;
}

bool mlocHebrewLeap(int64_t year)
{
    return FloorMod(7 * year + 1, 19) < 7;
}

uint32_t mlocHebrewMonths(int64_t year)
{
    return mlocHebrewLeap(year) ? 13 : 12;
}

// The days from the epoch to the molad's day of Tishrei, a day later
// when the molad would put Tishrei 1 on a Sunday, Wednesday or Friday.
static int64_t ElapsedDays(int64_t year)
{
    int64_t months = FloorDiv(235 * year - 234, 19);
    int64_t parts = firstMolad + monthParts * months;
    int64_t days = 29 * months + FloorDiv(parts, partsPerDay);
    return FloorMod(3 * (days + 1), 7) < 3 ? days + 1 : days;
}

// Tishrei 1 of a year, put off a day or two so no year has 356 or 382
// days.
static int64_t NewYear(int64_t year)
{
    int64_t before = ElapsedDays(year - 1);
    int64_t now = ElapsedDays(year);
    int64_t after = ElapsedDays(year + 1);
    int64_t delay = after - now == 356 ? 2 : now - before == 382 ? 1 : 0;
    return hebrewEpoch + now + delay;
}

// A year's months in civil order: Heshvan long and Kislev short by the
// year's length, Adar I (30 days) only in a leap year.
static void MonthLengths(int64_t year, uint8_t lengths[13], uint32_t* countOut)
{
    int64_t days = NewYear(year + 1) - NewYear(year);
    bool leap = mlocHebrewLeap(year);
    uint32_t n = 0;
    lengths[n++] = 30;
    lengths[n++] = days % 10 == 5 ? 30 : 29;
    lengths[n++] = days % 10 == 3 ? 29 : 30;
    lengths[n++] = 29;
    lengths[n++] = 30;
    if (leap)
    {
        lengths[n++] = 30;
    }
    static const uint8_t s_rest[] = {29, 30, 29, 30, 29, 30, 29};
    for (uint32_t i = 0; i < sizeof(s_rest); i++)
    {
        lengths[n++] = s_rest[i];
    }
    *countOut = n;
}

uint32_t mlocHebrewDaysInMonth(int64_t year, uint32_t month)
{
    uint8_t lengths[13];
    uint32_t count = 0;
    MonthLengths(year, lengths, &count);
    return lengths[month - 1];
}

int64_t mlocHebrewToDay(int64_t year, uint32_t month, uint32_t day)
{
    uint8_t lengths[13];
    uint32_t count = 0;
    MonthLengths(year, lengths, &count);
    int64_t fixed = NewYear(year) + day - 1;
    for (uint32_t m = 1; m < month; m++)
    {
        fixed += lengths[m - 1];
    }
    return fixed;
}

CalendarDate mlocHebrewFromDay(int32_t day)
{
    // A mean year is 35975351 / 98496 days.
    int64_t year = FloorDiv(((int64_t)day - hebrewEpoch) * 98496, 35975351) + 1;
    while (NewYear(year) > day)
    {
        year--;
    }
    while (NewYear(year + 1) <= day)
    {
        year++;
    }
    uint8_t lengths[13];
    uint32_t count = 0;
    MonthLengths(year, lengths, &count);
    int64_t left = day - NewYear(year);
    uint32_t month = 1;
    while (left >= lengths[month - 1])
    {
        left -= lengths[month - 1];
        month++;
    }
    return (CalendarDate){(int32_t)year, (uint8_t)month, (uint8_t)(left + 1)};
}
