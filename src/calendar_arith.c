// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The arithmetic calendars (calendar_arith.h).

#include "calendar_arith.h"

enum
{
    // The days before each calendar's day 1 of year 1, from Calendrical
    // Calculations: in the Julian calendar August 29 of 284 and of 8, and
    // March 19, July 16 and July 15 of 622.
    copticEpoch = 103605,
    ethiopicEpoch = 2796,
    persianEpoch = 226896,
    civilEpoch = 227015,
    astronomicalEpoch = 227014,
    // The years between the Ethiopic eras, the Buddhist and ROC years
    // and the Gregorian, and the Saka and the Gregorian.
    ameteAlemOffset = 5500,
    buddhistOffset = 543,
    rocOffset = -1911,
    sakaOffset = 78,
};

// Persian years the 33-year rule takes as leap that are not; the year
// after each is leap instead (calendrical_calculations 0.2.4 and ICU4C
// 79 have the same 78).
static const int16_t s_nonLeap[] = {
    1502, 1601, 1634, 1667, 1700, 1733, 1766, 1799, 1832, 1865, 1898, 1931, 1964, 1997, 2030, 2059,
    2063, 2096, 2129, 2158, 2162, 2191, 2195, 2224, 2228, 2257, 2261, 2290, 2294, 2323, 2327, 2356,
    2360, 2389, 2393, 2422, 2426, 2455, 2459, 2488, 2492, 2521, 2525, 2554, 2558, 2587, 2591, 2620,
    2624, 2653, 2657, 2686, 2690, 2719, 2723, 2748, 2752, 2756, 2781, 2785, 2789, 2818, 2822, 2847,
    2851, 2855, 2880, 2884, 2888, 2913, 2917, 2921, 2946, 2950, 2954, 2979, 2983, 2987,
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

static bool NonLeapPersian(int64_t year)
{
    size_t low = 0;
    size_t high = sizeof(s_nonLeap) / sizeof(s_nonLeap[0]);
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (s_nonLeap[middle] < year)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return low < sizeof(s_nonLeap) / sizeof(s_nonLeap[0]) && s_nonLeap[low] == year;
}

static bool GregorianLeap(int64_t year)
{
    return FloorMod(year, 4) == 0 && (FloorMod(year, 100) != 0 || FloorMod(year, 400) == 0);
}

static int64_t GregorianDay(int64_t year, int64_t month, int64_t day)
{
    int64_t correction = month <= 2 ? 0 : GregorianLeap(year) ? -1 : -2;
    return 365 * (year - 1) + FloorDiv(year - 1, 4) - FloorDiv(year - 1, 100) +
           FloorDiv(year - 1, 400) + FloorDiv(367 * month - 362, 12) + correction + day;
}

static int64_t GregorianYear(int64_t day)
{
    int64_t d0 = day - 1;
    int64_t n400 = FloorDiv(d0, 146097);
    int64_t d1 = FloorMod(d0, 146097);
    int64_t n100 = d1 / 36524;
    int64_t d2 = d1 % 36524;
    int64_t n4 = d2 / 1461;
    int64_t d3 = d2 % 1461;
    int64_t n1 = d3 / 365;
    int64_t year = 400 * n400 + 100 * n100 + 4 * n4 + n1;
    return n100 == 4 || n1 == 4 ? year : year + 1;
}

static CalendarDate GregorianFromDay(int64_t day, int64_t offset)
{
    int64_t year = GregorianYear(day);
    int64_t prior = day - GregorianDay(year, 1, 1);
    int64_t correction = day < GregorianDay(year, 3, 1) ? 0 : GregorianLeap(year) ? 1 : 2;
    int64_t month = FloorDiv(12 * (prior + correction) + 373, 367);
    return (CalendarDate){(int32_t)(year + offset), (uint8_t)month,
                          (uint8_t)(day - GregorianDay(year, month, 1) + 1)};
}

// Coptic and Ethiopic: twelve months of 30 days and a thirteenth of 5,
// or 6 before a Julian leap year.
static int64_t CopticDay(int64_t epoch, int64_t year, int64_t month, int64_t day)
{
    return epoch - 1 + 365 * (year - 1) + FloorDiv(year, 4) + 30 * (month - 1) + day;
}

static CalendarDate CopticFromDay(int64_t epoch, int64_t day, int64_t offset)
{
    int64_t year = FloorDiv(4 * (day - epoch) + 1463, 1461);
    int64_t month = FloorDiv(day - CopticDay(epoch, year, 1, 1), 30) + 1;
    return (CalendarDate){(int32_t)(year + offset), (uint8_t)month,
                          (uint8_t)(day + 1 - CopticDay(epoch, year, month, 1))};
}

// Indian: a Saka year starts on the Gregorian March 22, or 21 in a
// Gregorian leap year; Chaitra has 30 days (31 in a leap year), the
// next five months 31, the last six 30.
static int64_t IndianYearStart(int64_t year)
{
    int64_t gregorian = year + sakaOffset;
    return GregorianDay(gregorian, 3, 22) - (GregorianLeap(gregorian) ? 1 : 0);
}

static int64_t IndianDaysBefore(int64_t year, int64_t month)
{
    if (month == 1)
    {
        return 0;
    }
    int64_t first = GregorianLeap(year + sakaOffset) ? 31 : 30;
    int64_t middle = (month < 7 ? month : 7) - 2;
    int64_t last = month > 7 ? month - 7 : 0;
    return first + 31 * middle + 30 * last;
}

static CalendarDate IndianFromDay(int64_t day)
{
    int64_t year = GregorianYear(day) - sakaOffset;
    if (day < IndianYearStart(year))
    {
        year--;
    }
    int64_t ordinal = day - IndianYearStart(year);
    int64_t month = 12;
    while (month > 1 && IndianDaysBefore(year, month) > ordinal)
    {
        month--;
    }
    return (CalendarDate){(int32_t)year, (uint8_t)month,
                          (uint8_t)(ordinal - IndianDaysBefore(year, month) + 1)};
}

// Persian: the 33-year rule's new year, a day earlier after a year the
// rule took as leap that is not.
static int64_t PersianDay(int64_t year, int64_t month, int64_t day)
{
    int64_t newYear = persianEpoch - 1 + 365 * (year - 1) + FloorDiv(8 * year + 21, 33);
    if (NonLeapPersian(year - 1))
    {
        newYear--;
    }
    return newYear - 1 + (month <= 7 ? 31 * (month - 1) : 30 * (month - 1) + 6) + day;
}

static CalendarDate PersianFromDay(int64_t day)
{
    int64_t year = 1 + FloorDiv(33 * (day - persianEpoch + 1) + 3, 12053);
    int64_t ordinal = 1 + day - PersianDay(year, 1, 1);
    if (ordinal == 366 && NonLeapPersian(year))
    {
        year++;
        ordinal = 1;
    }
    int64_t month = ordinal <= 186 ? (ordinal + 30) / 31 : (ordinal - 6 + 29) / 30;
    return (CalendarDate){(int32_t)year, (uint8_t)month,
                          (uint8_t)(day - PersianDay(year, month, 1) + 1)};
}

// Tabular Islamic: thirty-year cycles of eleven leap years.
static int64_t IslamicDay(int64_t epoch, int64_t year, int64_t month, int64_t day)
{
    return epoch - 1 + (year - 1) * 354 + FloorDiv(3 + 11 * year, 30) + 29 * (month - 1) +
           month / 2 + day;
}

static CalendarDate IslamicFromDay(int64_t epoch, int64_t day)
{
    int64_t year = FloorDiv(30 * (day - epoch) + 10646, 10631);
    int64_t prior = day - IslamicDay(epoch, year, 1, 1);
    int64_t month = FloorDiv(11 * prior + 330, 325);
    return (CalendarDate){(int32_t)year, (uint8_t)month,
                          (uint8_t)(day - IslamicDay(epoch, year, month, 1) + 1)};
}

// The Gregorian year's offset of a Gregorian-based calendar.
static int64_t GregorianOffset(mlocCalendar calendar)
{
    return calendar == mloc_calendarBuddhist ? buddhistOffset
           : calendar == mloc_calendarRoc    ? rocOffset
                                             : 0;
}

CalendarDate mlocArithmeticFromDay(mlocCalendar calendar, int32_t day)
{
    switch (calendar)
    {
    case mloc_calendarCoptic:
        return CopticFromDay(copticEpoch, day, 0);
    case mloc_calendarEthiopic:
        return CopticFromDay(ethiopicEpoch, day, 0);
    case mloc_calendarEthiopicAmeteAlem:
        return CopticFromDay(ethiopicEpoch, day, ameteAlemOffset);
    case mloc_calendarIndian:
        return IndianFromDay(day);
    case mloc_calendarPersian:
        return PersianFromDay(day);
    case mloc_calendarIslamicCivil:
        return IslamicFromDay(civilEpoch, day);
    case mloc_calendarIslamicTbla:
        return IslamicFromDay(astronomicalEpoch, day);
    default:
        return GregorianFromDay(day, GregorianOffset(calendar));
    }
}

int64_t mlocArithmeticToDay(mlocCalendar calendar, int64_t year, uint32_t month, uint32_t day)
{
    switch (calendar)
    {
    case mloc_calendarCoptic:
        return CopticDay(copticEpoch, year, month, day);
    case mloc_calendarEthiopic:
        return CopticDay(ethiopicEpoch, year, month, day);
    case mloc_calendarEthiopicAmeteAlem:
        return CopticDay(ethiopicEpoch, year - ameteAlemOffset, month, day);
    case mloc_calendarIndian:
        return IndianYearStart(year) + IndianDaysBefore(year, month) + day - 1;
    case mloc_calendarPersian:
        return PersianDay(year, month, day);
    case mloc_calendarIslamicCivil:
        return IslamicDay(civilEpoch, year, month, day);
    case mloc_calendarIslamicTbla:
        return IslamicDay(astronomicalEpoch, year, month, day);
    default:
        return GregorianDay(year - GregorianOffset(calendar), month, day);
    }
}

uint32_t mlocArithmeticMonths(mlocCalendar calendar)
{
    return calendar == mloc_calendarCoptic || calendar == mloc_calendarEthiopic ||
                   calendar == mloc_calendarEthiopicAmeteAlem
               ? 13
               : 12;
}

bool mlocArithmeticLeap(mlocCalendar calendar, int64_t year)
{
    switch (calendar)
    {
    case mloc_calendarCoptic:
    case mloc_calendarEthiopic:
        return FloorMod(year, 4) == 3;
    case mloc_calendarEthiopicAmeteAlem:
        return FloorMod(year - ameteAlemOffset, 4) == 3;
    case mloc_calendarIndian:
        return GregorianLeap(year + sakaOffset);
    case mloc_calendarPersian:
        return NonLeapPersian(year - 1) ||
               (!NonLeapPersian(year) && FloorMod(25 * year + 11, 33) < 8);
    case mloc_calendarIslamicCivil:
    case mloc_calendarIslamicTbla:
        return FloorMod(14 + 11 * year, 30) < 11;
    default:
        return GregorianLeap(year - GregorianOffset(calendar));
    }
}

// Each calendar's month lengths, given whether its year is leap.
static uint32_t CopticMonthDays(uint32_t month, bool leap)
{
    return month < 13 ? 30 : leap ? 6 : 5;
}

static uint32_t IndianMonthDays(uint32_t month, bool leap)
{
    return month == 1 ? (leap ? 31 : 30) : month <= 6 ? 31 : 30;
}

static uint32_t PersianMonthDays(uint32_t month, bool leap)
{
    return month <= 6 ? 31 : month <= 11 ? 30 : leap ? 30 : 29;
}

static uint32_t IslamicMonthDays(uint32_t month, bool leap)
{
    return month % 2 == 1 || (month == 12 && leap) ? 30 : 29;
}

static uint32_t GregorianMonthDays(uint32_t month, bool leap)
{
    static const uint8_t s_days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && leap ? 29 : s_days[month - 1];
}

uint32_t mlocArithmeticDaysInMonth(mlocCalendar calendar, int64_t year, uint32_t month)
{
    bool leap = mlocArithmeticLeap(calendar, year);
    switch (calendar)
    {
    case mloc_calendarCoptic:
    case mloc_calendarEthiopic:
    case mloc_calendarEthiopicAmeteAlem:
        return CopticMonthDays(month, leap);
    case mloc_calendarIndian:
        return IndianMonthDays(month, leap);
    case mloc_calendarPersian:
        return PersianMonthDays(month, leap);
    case mloc_calendarIslamicCivil:
    case mloc_calendarIslamicTbla:
        return IslamicMonthDays(month, leap);
    default:
        return GregorianMonthDays(month, leap);
    }
}
