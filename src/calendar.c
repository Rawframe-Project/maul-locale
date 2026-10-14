// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates (maul-locale/calendar.h, record mloc-0040): fields made into a
// day and read back, with Temporal's era codes, month codes and
// overflow.

#include "maul-locale/calendar.h"

#include "calendar_eras.h"
#include "calendar_year.h"

#include <string.h>

enum
{
    // Years past this are refused before any arithmetic; Temporal's range
    // lies well within.
    maxYear = 1000000,
    // January 1 of 1873 (Meiji 6), when Japan took the Gregorian
    // calendar: earlier days read in ce and bce, as Temporal's era table
    // and ICU4X have it.
    japaneseGregorianDay = 683735,
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
    case mloc_calendarJapanese:
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
    case mloc_calendarIslamicUmalqura:
    case mloc_calendarIslamic:
    case mloc_calendarIslamicRgsa:
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
    for (uint32_t i = 0; calendar == mloc_calendarJapanese && i < mlocJapaneseEraCount; i++)
    {
        const JapaneseEra* e = &mlocJapaneseEras[i];
        if (strlen(e->code) == length && memcmp(e->code, era, length) == 0)
        {
            *yearOut = e->year + eraYear - 1;
            return true;
        }
    }
    return false;
}

// The Japanese era a day falls in from Meiji 6, or null.
static const JapaneseEra* JapaneseEraOf(int32_t day)
{
    const JapaneseEra* found = nullptr;
    for (uint32_t i = 0; day >= japaneseGregorianDay && i < mlocJapaneseEraCount; i++)
    {
        if (mlocJapaneseEras[i].day <= day)
        {
            found = &mlocJapaneseEras[i];
        }
    }
    return found;
}

// A month code's number, 1 to 13, and whether it names a leap month;
// false when it is not of the form "Mnn" or "MnnL".
// The month's place the input names, by code, by place, or by both
// when they agree; 0 when it names none.
static mlocResult MonthOf(const CalendarYear* y, const mlocDateInput* input, mlocOverflow overflow,
                          uint32_t* monthOut)
{
    *monthOut = input->month;
    if (input->monthCode == nullptr)
    {
        return mloc_success;
    }
    uint32_t number = 0;
    bool leap = false;
    uint32_t coded = 0;
    if (!mlocYearParseMonthCode(input->monthCode, input->monthCodeLength, &number, &leap))
    {
        return mloc_errorSyntax;
    }
    mlocResult r = mlocYearMonthOfCode(y, number, leap, overflow, &coded);
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

// The year an input names, checked; mloc_success or the refusal.
static mlocResult YearOfInput(mlocCalendar calendar, const mlocDateInput* input, int64_t* yearOut)
{
    *yearOut = input->year;
    if (input->era != nullptr &&
        !YearInEra(calendar, input->era, input->eraLength, input->year, yearOut))
    {
        return mloc_errorSyntax;
    }
    bool lunisolar = mlocYearLunisolar(mlocYearSource(calendar));
    if (*yearOut < (lunisolar ? lunarMinYear : -maxYear) ||
        *yearOut > (lunisolar ? lunarMaxYear : maxYear))
    {
        return mloc_errorInvalid;
    }
    return mloc_success;
}

mlocResult mlocMakeDate(mlocCalendar calendar, const mlocDateInput* input, mlocOverflow overflow,
                        mlocDate* dateOut)
{
    if (input == nullptr || dateOut == nullptr || (uint32_t)calendar >= yearCalendarCount ||
        (uint32_t)overflow > mloc_overflowReject ||
        (input->era == nullptr && input->eraLength != 0) ||
        (input->monthCode == nullptr && input->monthCodeLength != 0))
    {
        return mloc_errorInvalid;
    }
    int64_t year = 0;
    mlocResult r = YearOfInput(calendar, input, &year);
    if (r != mloc_success)
    {
        return r;
    }
    CalendarYear y;
    mlocYearOpen(&y, calendar, year);
    uint32_t months = mlocYearMonths(&y);
    uint32_t month = 0;
    r = MonthOf(&y, input, overflow, &month);
    if (r != mloc_success)
    {
        return r;
    }
    if (month == 0 || input->day == 0 || (overflow == mloc_overflowReject && month > months))
    {
        return mloc_errorInvalid;
    }
    month = month < months ? month : months;
    uint32_t last = mlocYearDaysInMonth(&y, month);
    if (overflow == mloc_overflowReject && input->day > last)
    {
        return mloc_errorInvalid;
    }
    uint32_t day = input->day < last ? input->day : last;
    int64_t fixed = mlocYearDayOf(&y, month, day);
    if (fixed < mlocDateMinDay || fixed > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    *dateOut = (mlocDate){.day = (int32_t)fixed, .calendar = (uint8_t)calendar};
    return mloc_success;
}

mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day, mlocDate* dateOut)
{
    if (dateOut == nullptr || (uint32_t)calendar >= yearCalendarCount || day < mlocDateMinDay ||
        day > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    *dateOut = (mlocDate){.day = day, .calendar = (uint8_t)calendar};
    return mloc_success;
}

// The era and the year in it of a date.
static void SetEra(mlocCalendar calendar, int32_t day, int32_t year, mlocDateFields* f)
{
    f->eraYear = year;
    Eras eras = ErasOf(calendar);
    if (eras.count > 0)
    {
        const Era* e = year >= eras.start || eras.count == 1 ? &eras.eras[0] : &eras.eras[1];
        f->eraYear = e->sign * (year - e->base);
        f->eraLength = (uint8_t)strlen(e->code);
        memcpy(f->era, e->code, f->eraLength);
    }
    const JapaneseEra* japanese = calendar == mloc_calendarJapanese ? JapaneseEraOf(day) : nullptr;
    if (japanese != nullptr)
    {
        f->eraYear = year - japanese->year + 1;
        f->eraLength = (uint8_t)strlen(japanese->code);
        memset(f->era, 0, sizeof(f->era));
        memcpy(f->era, japanese->code, f->eraLength);
    }
}

mlocResult mlocGetDateFields(const mlocDate* date, mlocDateFields* fieldsOut)
{
    if (!mlocYearDateValid(date) || fieldsOut == nullptr)
    {
        return mloc_errorInvalid;
    }
    mlocCalendar calendar = (mlocCalendar)date->calendar;
    CalendarYear y;
    CalendarDate d = mlocYearOpenOf(&y, calendar, date->day);
    mlocDateFields f = {
        .year = d.year,
        .month = d.month,
        .day = d.day,
        .monthsInYear = (uint8_t)mlocYearMonths(&y),
        .daysInMonth = (uint8_t)mlocYearDaysInMonth(&y, d.month),
        .inLeapYear = mlocYearLeap(&y) ? 1 : 0,
    };
    SetEra(calendar, date->day, d.year, &f);
    uint32_t number = 0;
    bool leapMonth = false;
    mlocYearCodeOf(&y, d.month, &number, &leapMonth);
    f.monthCode[0] = 'M';
    f.monthCode[1] = (char)('0' + number / 10);
    f.monthCode[2] = (char)('0' + number % 10);
    f.monthCode[3] = leapMonth ? 'L' : '\0';
    f.monthCodeLength = leapMonth ? 4 : 3;
    // January 1 of year 1, day 1, was a Monday.
    int64_t weekday = ((int64_t)date->day - 1) % 7;
    f.dayOfWeek = (uint8_t)((weekday < 0 ? weekday + 7 : weekday) + 1);
    f.dayOfYear = (uint16_t)(date->day - mlocYearDayOf(&y, 1, 1) + 1);
    f.daysInYear = (uint16_t)mlocYearDays(&y);
    *fieldsOut = f;
    return mloc_success;
}
