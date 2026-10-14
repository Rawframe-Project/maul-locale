// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates from fields given by name (maul-locale/calendar.h, record
// mloc-0040): Temporal's PlainDate.with, its fields merged as
// CalendarMergeFields merges them, and the Intl Era and Month Code
// proposal's NonISOMonthDayToISOReferenceDate.

#include "calendar_year.h"

#include "maul-locale/calendar.h"

enum
{
    givenYears = mloc_givenEra | mloc_givenEraYear | mloc_givenYear,
    givenMonths = mloc_givenMonth | mloc_givenMonthCode,
    givenAll = givenYears | givenMonths | mloc_givenDay,
    // The ISO years the reference search runs over: 1900 to 1972, then
    // 1973 to 2035.
    firstReferenceYear = 1900,
    lastBackwardYear = 1972,
    lastReferenceYear = 2035,
};

// Makes the date the fields name: a year (or an era and its era year,
// with a year given as well naming the same one), a month or month code,
// and a day.
static mlocResult Resolve(mlocCalendar calendar, const mlocDateChanges* f, mlocOverflow overflow,
                          mlocDate* dateOut)
{
    bool era = (f->given & mloc_givenEra) != 0;
    bool year = (f->given & mloc_givenYear) != 0;
    if (era != ((f->given & mloc_givenEraYear) != 0) || (!era && !year) ||
        (f->given & givenMonths) == 0 || (f->given & mloc_givenDay) == 0 ||
        (era && f->era == nullptr) ||
        ((f->given & mloc_givenMonthCode) != 0 && f->monthCode == nullptr) ||
        ((f->given & mloc_givenMonth) != 0 && f->month == 0))
    {
        return mloc_errorInvalid;
    }
    bool code = (f->given & mloc_givenMonthCode) != 0;
    mlocDateInput input = {
        .era = era ? f->era : nullptr,
        .eraLength = era ? f->eraLength : 0,
        .year = era ? f->eraYear : f->year,
        .monthCode = code ? f->monthCode : nullptr,
        .monthCodeLength = code ? f->monthCodeLength : 0,
        .month = (f->given & mloc_givenMonth) != 0 ? f->month : 0,
        .day = f->day,
    };
    mlocDate date = {0};
    mlocResult r = mlocMakeDate(calendar, &input, overflow, &date);
    if (r != mloc_success)
    {
        return r;
    }
    if (era && year)
    {
        mlocDateFields fields = {0};
        if (mlocGetDateFields(&date, &fields) != mloc_success || fields.year != f->year)
        {
            return mloc_errorInvalid;
        }
    }
    *dateOut = date;
    return mloc_success;
}

mlocResult mlocWithDate(const mlocDate* date, const mlocDateChanges* changes, mlocOverflow overflow,
                        mlocDate* dateOut)
{
    mlocDateFields fields = {0};
    if (changes == nullptr || dateOut == nullptr || (changes->given & ~(uint32_t)givenAll) != 0 ||
        mlocGetDateFields(date, &fields) != mloc_success)
    {
        return mloc_errorInvalid;
    }
    // The date's year, month code and day, less what the changes make
    // stale, under the changes.
    mlocDateChanges merged = *changes;
    if ((changes->given & givenMonths) == 0)
    {
        merged.given |= mloc_givenMonthCode;
        merged.monthCode = fields.monthCode;
        merged.monthCodeLength = fields.monthCodeLength;
    }
    if ((changes->given & givenYears) == 0)
    {
        merged.given |= mloc_givenYear;
        merged.year = fields.year;
    }
    if ((changes->given & mloc_givenDay) == 0)
    {
        merged.given |= mloc_givenDay;
        merged.day = fields.day;
    }
    return Resolve((mlocCalendar)date->calendar, &merged, overflow, dateOut);
}

// The most days a month of the code has in any year.
static uint32_t LongestMonth(mlocCalendar calendar, uint32_t number, bool leap)
{
    static const uint8_t s_gregorian[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const uint8_t s_indian[12] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 30};
    static const uint8_t s_persian[12] = {31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 30};
    static const uint8_t s_hebrew[12] = {30, 30, 30, 29, 30, 29, 30, 29, 30, 29, 30, 29};
    switch (calendar)
    {
    case mloc_calendarCoptic:
    case mloc_calendarEthiopic:
    case mloc_calendarEthiopicAmeteAlem:
        return number == 13 ? 6 : 30;
    case mloc_calendarIndian:
        return s_indian[number - 1];
    case mloc_calendarPersian:
        return s_persian[number - 1];
    case mloc_calendarHebrew:
        return leap ? 30 : s_hebrew[number - 1];
    case mloc_calendarIslamicCivil:
    case mloc_calendarIslamicTbla:
        return number % 2 == 1 || number == 12 ? 30 : 29;
    case mloc_calendarIslamicUmalqura:
    case mloc_calendarIslamic:
    case mloc_calendarIslamicRgsa:
    case mloc_calendarChinese:
    case mloc_calendarDangi:
        return 30;
    default:
        return s_gregorian[number - 1];
    }
}

// The proposal's reference years for Chinese and Dangi month codes: for
// days 1 to 29 and for day 30, 0 where it has none. M03's day 30 is
// 1966 in Chinese and 1968 in Dangi; M11L's days 11 to 29 are 2034.
typedef struct ReferenceYears
{
    uint16_t days;
    uint16_t day30;
} ReferenceYears;

static const ReferenceYears s_common[12] = {
    {1972, 1970}, {1972, 1972}, {1972, 1966}, {1972, 1970}, {1972, 1972}, {1972, 1971},
    {1972, 1972}, {1972, 1971}, {1972, 1972}, {1972, 1972}, {1972, 1970}, {1972, 1972},
};
static const ReferenceYears s_leap[12] = {
    {0, 0},       {1947, 0}, {1966, 1955}, {1963, 1944}, {1971, 1952}, {1960, 1941},
    {1968, 1938}, {1957, 0}, {2014, 0},    {1984, 0},    {2033, 0},    {0, 0},
};

static int32_t LunisolarReferenceYear(bool korea, uint32_t number, bool leap, uint32_t day)
{
    ReferenceYears r = leap ? s_leap[number - 1] : s_common[number - 1];
    if (!leap && number == 3 && day == 30)
    {
        return korea ? 1968 : 1966;
    }
    if (leap && number == 11 && day > 10)
    {
        return day == 30 ? 0 : 2034;
    }
    return day == 30 ? r.day30 : r.days;
}

static int64_t IsoDay(int64_t year, uint32_t month, uint32_t day)
{
    CalendarYear y;
    mlocYearOpen(&y, mloc_calendarGregorian, year);
    return mlocYearDayOf(&y, month, day);
}

// The day of the code and day in a year of the calendar, false when the
// year has no such day.
static bool DayIn(const CalendarYear* y, uint32_t number, bool leap, uint32_t day, int64_t* out)
{
    uint32_t month = 0;
    if (mlocYearMonthOfCode(y, number, leap, mloc_overflowReject, &month) != mloc_success ||
        day > mlocYearDaysInMonth(y, month))
    {
        return false;
    }
    *out = mlocYearDayOf(y, month, day);
    return true;
}

// The latest day from first to last (backward) or the earliest (forward)
// with the code and day, false for none.
static bool Search(mlocCalendar calendar, uint32_t number, bool leap, uint32_t day, int64_t first,
                   int64_t last, bool backward, int64_t* out)
{
    CalendarYear y;
    mlocYearOpenOf(&y, calendar, (int32_t)(backward ? last : first));
    for (int64_t year = y.year;; year += backward ? -1 : 1)
    {
        CalendarYear at;
        mlocYearOpen(&at, calendar, year);
        int64_t yearStart = mlocYearDayOf(&at, 1, 1);
        if (backward ? yearStart + mlocYearDays(&at) <= first : yearStart > last)
        {
            return false;
        }
        int64_t found = 0;
        if (DayIn(&at, number, leap, day, &found) && found >= first && found <= last)
        {
            *out = found;
            return true;
        }
    }
}

// The month code a month-day names and its longest month: with a year,
// that year's month (the date of the fields on day 1 gives the year, the
// place and the code, each refused or clamped); without, the code alone.
static mlocResult MonthOfFields(mlocCalendar calendar, const mlocDateChanges* f,
                                mlocOverflow overflow, uint32_t* numberOut, bool* leapOut,
                                uint32_t* longestOut)
{
    if ((f->given & givenYears) != 0)
    {
        mlocDateChanges first = *f;
        first.day = 1;
        mlocDate date = {0};
        mlocResult r = Resolve(calendar, &first, overflow, &date);
        if (r != mloc_success)
        {
            return r;
        }
        CalendarYear y;
        CalendarDate d = mlocYearOpenOf(&y, calendar, date.day);
        mlocYearCodeOf(&y, d.month, numberOut, leapOut);
        *longestOut = mlocYearDaysInMonth(&y, d.month);
        return mloc_success;
    }
    // Without a year a month's place names no month: a month code is
    // needed, and a place beside it is refused.
    if ((f->given & mloc_givenMonthCode) == 0 || (f->given & mloc_givenMonth) != 0)
    {
        return mloc_errorInvalid;
    }
    CalendarYear any;
    mlocYearOpen(&any, calendar, lastBackwardYear);
    if (!mlocYearParseMonthCode(f->monthCode, f->monthCodeLength, numberOut, leapOut) ||
        !mlocYearCodeExists(&any, *numberOut, *leapOut))
    {
        return mloc_errorSyntax;
    }
    *longestOut = LongestMonth(calendar, *numberOut, *leapOut);
    return mloc_success;
}

// The reference day of a month code and day: the proposal's table year
// in Chinese and Dangi (a rare leap month day becoming the common month's,
// or refused), else the latest to 1972 or the earliest after.
static mlocResult Reference(mlocCalendar calendar, uint32_t number, bool leap, uint32_t day,
                            mlocOverflow overflow, int64_t* found)
{
    if (calendar == mloc_calendarChinese || calendar == mloc_calendarDangi)
    {
        bool korea = calendar == mloc_calendarDangi;
        if (LunisolarReferenceYear(korea, number, leap, day) == 0)
        {
            if (overflow == mloc_overflowReject)
            {
                return mloc_errorInvalid;
            }
            leap = false;
        }
        int64_t year = LunisolarReferenceYear(korea, number, leap, day);
        return Search(calendar, number, leap, day, IsoDay(year, 1, 1), IsoDay(year, 12, 31), true,
                      found)
                   ? mloc_success
                   : mloc_errorInvalid;
    }
    return Search(calendar, number, leap, day, IsoDay(firstReferenceYear, 1, 1),
                  IsoDay(lastBackwardYear, 12, 31), true, found) ||
                   Search(calendar, number, leap, day, IsoDay(lastBackwardYear + 1, 1, 1),
                          IsoDay(lastReferenceYear, 12, 31), false, found)
               ? mloc_success
               : mloc_errorInvalid;
}

mlocResult mlocMakeMonthDay(mlocCalendar calendar, const mlocDateChanges* f, mlocOverflow overflow,
                            mlocDate* referenceOut)
{
    if (f == nullptr || referenceOut == nullptr || (uint32_t)calendar >= yearCalendarCount ||
        (uint32_t)overflow > mloc_overflowReject || (f->given & ~(uint32_t)givenAll) != 0 ||
        (f->given & mloc_givenDay) == 0 || f->day == 0 ||
        ((f->given & mloc_givenMonthCode) != 0 &&
         (f->monthCode == nullptr || f->monthCodeLength > 4)) ||
        ((f->given & mloc_givenMonth) != 0 && f->month == 0))
    {
        return mloc_errorInvalid;
    }
    uint32_t number = 0;
    bool leap = false;
    uint32_t longest = 0;
    mlocResult r = MonthOfFields(calendar, f, overflow, &number, &leap, &longest);
    if (r != mloc_success)
    {
        return r;
    }
    if (f->day > longest && overflow == mloc_overflowReject)
    {
        return mloc_errorInvalid;
    }
    int64_t found = 0;
    r = Reference(calendar, number, leap, f->day < longest ? f->day : longest, overflow, &found);
    if (r == mloc_success)
    {
        *referenceOut = (mlocDate){.day = (int32_t)found, .calendar = (uint8_t)calendar};
    }
    return r;
}
