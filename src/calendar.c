// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates (maul-locale/calendar.h, record mloc-0040): fields made into a
// day and read back, with Temporal's era codes, month codes and
// overflow.

#include "maul-locale/calendar.h"

#include "calendar_arith.h"
#include "calendar_hebrew.h"

#include <string.h>

enum
{
    calendarCount = 18,
    // Years past this are refused before any arithmetic; Temporal's range
    // lies well within.
    maxYear = 1000000,
};

// An era: year = base + sign * eraYear.
typedef struct Era
{
    char code[8];
    int32_t base;
    int8_t sign;
} Era;

// A calendar's eras: the first for years from start, the second before;
// the rest are aliases accepted in input.
typedef struct Eras
{
    Era eras[4];
    uint8_t count;
    int32_t start;
} Eras;

static const Era s_ce = {"ce", 0, 1};
static const Era s_bce = {"bce", 1, -1};

static Eras ErasOf(mlocCalendar calendar)
{
    switch (calendar)
    {
    case mloc_calendarGregorian:
        return (Eras){{s_ce, s_bce, {"ad", 0, 1}, {"bc", 1, -1}}, 4, 1};
    case mloc_calendarBuddhist:
        return (Eras){{{"be", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarRoc:
        return (Eras){{{"roc", 0, 1}, {"broc", 1, -1}}, 2, 1};
    case mloc_calendarCoptic:
    case mloc_calendarHebrew:
        return (Eras){{{"am", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarEthiopic:
        return (Eras){{{"am", 0, 1}, {"aa", -5500, 1}}, 2, 1};
    case mloc_calendarEthiopicAmeteAlem:
        return (Eras){{{"aa", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarIndian:
        return (Eras){{{"shaka", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarPersian:
        return (Eras){{{"ap", 0, 1}}, 1, INT32_MIN};
    case mloc_calendarIslamicCivil:
    case mloc_calendarIslamicTbla:
        return (Eras){{{"ah", 0, 1}, {"bh", 1, -1}}, 2, 1};
    default:
        return (Eras){0};
    }
}

// The year an era and a year in it name; false for an era not of the
// calendar.
static bool YearInEra(mlocCalendar calendar, const char* era, size_t length, int64_t eraYear,
                      int64_t* yearOut)
{
    Eras eras = ErasOf(calendar);
    for (uint32_t i = 0; i < eras.count; i++)
    {
        const Era* e = &eras.eras[i];
        if (strlen(e->code) == length && memcmp(e->code, era, length) == 0)
        {
            *yearOut = e->base + e->sign * eraYear;
            return true;
        }
    }
    return false;
}

// A month code's number, 1 to 13, and whether it names a leap month;
// false when it is not of the form "Mnn" or "MnnL".
static bool ParseMonthCode(const char* code, size_t length, uint32_t* numberOut, bool* leapOut)
{
    if ((length != 3 && length != 4) || code[0] != 'M' || code[1] < '0' || code[1] > '9' ||
        code[2] < '0' || code[2] > '9' || (length == 4 && code[3] != 'L'))
    {
        return false;
    }
    *numberOut = (uint32_t)(code[1] - '0') * 10 + (uint32_t)(code[2] - '0');
    *leapOut = length == 4;
    return *numberOut >= 1 && *numberOut <= 13;
}

static bool Built(mlocCalendar calendar)
{
    return mlocArithmetic(calendar) || calendar == mloc_calendarHebrew;
}

// Each calendar's arithmetic, by the file that has it.
static CalendarDate FromDay(mlocCalendar calendar, int32_t day)
{
    return calendar == mloc_calendarHebrew ? mlocHebrewFromDay(day)
                                           : mlocArithmeticFromDay(calendar, day);
}

static int64_t ToDay(mlocCalendar calendar, int64_t year, uint32_t month, uint32_t day)
{
    return calendar == mloc_calendarHebrew ? mlocHebrewToDay(year, month, day)
                                           : mlocArithmeticToDay(calendar, year, month, day);
}

static uint32_t MonthsInYear(mlocCalendar calendar, int64_t year)
{
    return calendar == mloc_calendarHebrew ? mlocHebrewMonths(year)
                                           : mlocArithmeticMonths(calendar);
}

static uint32_t DaysInMonth(mlocCalendar calendar, int64_t year, uint32_t month)
{
    return calendar == mloc_calendarHebrew ? mlocHebrewDaysInMonth(year, month)
                                           : mlocArithmeticDaysInMonth(calendar, year, month);
}

static bool Leap(mlocCalendar calendar, int64_t year)
{
    return calendar == mloc_calendarHebrew ? mlocHebrewLeap(year)
                                           : mlocArithmeticLeap(calendar, year);
}

// A month's code: its number and whether it is the leap month. In a
// Hebrew leap year Adar I, the sixth month, is 'M05L' and the months
// after it take the code of the place before theirs.
static void CodeOf(mlocCalendar calendar, int64_t year, uint32_t month, uint32_t* numberOut,
                   bool* leapOut)
{
    bool hebrewLeap = calendar == mloc_calendarHebrew && mlocHebrewLeap(year);
    *leapOut = hebrewLeap && month == 6;
    *numberOut = hebrewLeap && month >= 6 ? (month == 6 ? 5 : month - 1) : month;
}

// The month a code names in a year: mloc_success; for a leap month the
// year lacks, the month after it (constrain) or mloc_errorInvalid
// (reject); mloc_errorSyntax for a code no year of the calendar has.
static mlocResult MonthOfCode(mlocCalendar calendar, int64_t year, uint32_t number, bool leap,
                              mlocOverflow overflow, uint32_t* monthOut)
{
    bool hebrew = calendar == mloc_calendarHebrew;
    uint32_t most = hebrew ? 12 : mlocArithmeticMonths(calendar);
    if (number > most || (leap && !(hebrew && number == 5)))
    {
        return mloc_errorSyntax;
    }
    bool hebrewLeap = hebrew && mlocHebrewLeap(year);
    if (leap && !hebrewLeap && overflow == mloc_overflowReject)
    {
        return mloc_errorInvalid;
    }
    *monthOut = hebrewLeap && (leap || number >= 6) ? number + 1 : leap ? 6 : number;
    return mloc_success;
}

// The month's place the input names, by code, by place, or by both
// when they agree; 0 when it names none.
static mlocResult MonthOf(mlocCalendar calendar, int64_t year, const mlocDateInput* input,
                          mlocOverflow overflow, uint32_t* monthOut)
{
    *monthOut = input->month;
    if (input->monthCode == nullptr)
    {
        return mloc_success;
    }
    uint32_t number = 0;
    bool leap = false;
    uint32_t coded = 0;
    if (!ParseMonthCode(input->monthCode, input->monthCodeLength, &number, &leap))
    {
        return mloc_errorSyntax;
    }
    mlocResult r = MonthOfCode(calendar, year, number, leap, overflow, &coded);
    if (r != mloc_success)
    {
        return r;
    }
    if (input->month != 0 && input->month != coded)
    {
        return mloc_errorInvalid;
    }
    *monthOut = coded;
    return mloc_success;
}

mlocResult mlocMakeDate(mlocCalendar calendar, const mlocDateInput* input, mlocOverflow overflow,
                        mlocDate* dateOut)
{
    if (input == nullptr || dateOut == nullptr || (uint32_t)calendar >= calendarCount ||
        (uint32_t)overflow > mloc_overflowReject ||
        (input->era == nullptr && input->eraLength != 0) ||
        (input->monthCode == nullptr && input->monthCodeLength != 0))
    {
        return mloc_errorInvalid;
    }
    if (!Built(calendar))
    {
        return mloc_errorNotFound;
    }
    int64_t year = input->year;
    if (input->era != nullptr &&
        !YearInEra(calendar, input->era, input->eraLength, input->year, &year))
    {
        return mloc_errorSyntax;
    }
    if (year < -maxYear || year > maxYear)
    {
        return mloc_errorInvalid;
    }
    uint32_t months = MonthsInYear(calendar, year);
    uint32_t month = 0;
    mlocResult r = MonthOf(calendar, year, input, overflow, &month);
    if (r != mloc_success)
    {
        return r;
    }
    if (month == 0 || input->day == 0 || (overflow == mloc_overflowReject && month > months))
    {
        return mloc_errorInvalid;
    }
    month = month < months ? month : months;
    uint32_t last = DaysInMonth(calendar, year, month);
    if (overflow == mloc_overflowReject && input->day > last)
    {
        return mloc_errorInvalid;
    }
    uint32_t day = input->day < last ? input->day : last;
    int64_t fixed = ToDay(calendar, year, month, day);
    if (fixed < mlocDateMinDay || fixed > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    *dateOut = (mlocDate){.day = (int32_t)fixed, .calendar = (uint8_t)calendar};
    return mloc_success;
}

mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day, mlocDate* dateOut)
{
    if (dateOut == nullptr || (uint32_t)calendar >= calendarCount || day < mlocDateMinDay ||
        day > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    if (!Built(calendar))
    {
        return mloc_errorNotFound;
    }
    *dateOut = (mlocDate){.day = day, .calendar = (uint8_t)calendar};
    return mloc_success;
}

mlocResult mlocGetDateFields(const mlocDate* date, mlocDateFields* fieldsOut)
{
    if (date == nullptr || fieldsOut == nullptr || date->calendar >= calendarCount ||
        date->day < mlocDateMinDay || date->day > mlocDateMaxDay || date->reserved[0] != 0 ||
        date->reserved[1] != 0 || date->reserved[2] != 0)
    {
        return mloc_errorInvalid;
    }
    mlocCalendar calendar = (mlocCalendar)date->calendar;
    if (!Built(calendar))
    {
        return mloc_errorNotFound;
    }
    CalendarDate d = FromDay(calendar, date->day);
    mlocDateFields f = {
        .year = d.year,
        .eraYear = d.year,
        .month = d.month,
        .day = d.day,
        .monthsInYear = (uint8_t)MonthsInYear(calendar, d.year),
        .daysInMonth = (uint8_t)DaysInMonth(calendar, d.year, d.month),
        .inLeapYear = Leap(calendar, d.year) ? 1 : 0,
    };
    Eras eras = ErasOf(calendar);
    if (eras.count > 0)
    {
        const Era* e = d.year >= eras.start || eras.count == 1 ? &eras.eras[0] : &eras.eras[1];
        f.eraYear = e->sign * (d.year - e->base);
        f.eraLength = (uint8_t)strlen(e->code);
        memcpy(f.era, e->code, f.eraLength);
    }
    uint32_t number = 0;
    bool leapMonth = false;
    CodeOf(calendar, d.year, d.month, &number, &leapMonth);
    f.monthCode[0] = 'M';
    f.monthCode[1] = (char)('0' + number / 10);
    f.monthCode[2] = (char)('0' + number % 10);
    f.monthCode[3] = leapMonth ? 'L' : '\0';
    f.monthCodeLength = leapMonth ? 4 : 3;
    // January 1 of year 1, day 1, was a Monday.
    int64_t weekday = ((int64_t)date->day - 1) % 7;
    f.dayOfWeek = (uint8_t)((weekday < 0 ? weekday + 7 : weekday) + 1);
    int64_t yearStart = ToDay(calendar, d.year, 1, 1);
    f.dayOfYear = (uint16_t)(date->day - yearStart + 1);
    f.daysInYear = (uint16_t)(ToDay(calendar, (int64_t)d.year + 1, 1, 1) - yearStart);
    *fieldsOut = f;
    return mloc_success;
}
