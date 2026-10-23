// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Adding a duration to a date (maul-locale/calendar.h): the Intl Era and
// Month Code proposal's NonISODateAdd, with months carried over as one
// number (calendar_year.h) rather than year by year.

#include "calendar_year.h"

#include "maul-locale/calendar.h"

static const int64_t s_maxYearsMonthsWeeks = INT64_C(4294967295);
static const int64_t s_maxDays = INT64_C(104249991374);

static bool InLimit(int64_t value, int64_t limit)
{
    return value >= -limit && value <= limit;
}

// Temporal's IsValidDuration for a date duration: one sign and the
// fields' limits.
static bool ValidDuration(const mlocDateDuration* d)
{
    bool positive = d->years > 0 || d->months > 0 || d->weeks > 0 || d->days > 0;
    bool negative = d->years < 0 || d->months < 0 || d->weeks < 0 || d->days < 0;
    return !(positive && negative) && InLimit(d->years, s_maxYearsMonthsWeeks) &&
           InLimit(d->months, s_maxYearsMonthsWeeks) && InLimit(d->weeks, s_maxYearsMonthsWeeks) &&
           InLimit(d->days, s_maxDays);
}

mlocResult mlocAddToDate(const mlocDate* date, const mlocDateDuration* duration,
                         mlocOverflow overflow, mlocDate* dateOut)
{
    if (!mlocYearDateValid(date) || duration == nullptr || dateOut == nullptr ||
        (uint32_t)overflow > mloc_overflowReject || !ValidDuration(duration))
    {
        return mloc_errorInvalid;
    }
    mlocCalendar calendar = (mlocCalendar)date->calendar;
    CalendarYear y;
    CalendarDate start = mlocYearOpenOf(&y, calendar, date->day);
    // The years, keeping the month code.
    uint32_t month = start.month;
    if (duration->years != 0)
    {
        uint32_t number = 0;
        bool leap = false;
        mlocYearCodeOf(&y, start.month, &number, &leap);
        int64_t year = y.year + duration->years;
        if (mlocYearLunisolar(y.source) && (year < lunarMinYear || year > lunarMaxYear))
        {
            return mloc_errorInvalid;
        }
        mlocYearOpen(&y, calendar, year);
        if (mlocYearMonthOfCode(&y, number, leap, overflow, &month) != mloc_success)
        {
            return mloc_errorInvalid;
        }
    }
    // The months, as one number.
    if (duration->months != 0 &&
        !mlocYearOpenMonthNumber(&y, calendar, mlocYearMonthNumber(&y, month) + duration->months,
                                 &month))
    {
        return mloc_errorInvalid;
    }
    // The day, then weeks and days.
    uint32_t last = mlocYearDaysInMonth(&y, month);
    if (start.day > last && overflow == mloc_overflowReject)
    {
        return mloc_errorInvalid;
    }
    uint32_t day = start.day < last ? start.day : last;
    int64_t fixed = mlocYearDayOf(&y, month, day) + 7 * duration->weeks + duration->days;
    if (fixed < mlocDateMinDay || fixed > mlocDateMaxDay)
    {
        return mloc_errorInvalid;
    }
    *dateOut = (mlocDate){.day = (int32_t)fixed, .calendar = date->calendar};
    return mloc_success;
}
