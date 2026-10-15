// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Times of day and date-times (date_time.h).

#include "date_time.h"

#include "calendar_year.h"

bool mlocTimeValid(const mlocTime* time)
{
    return time->hour < 24 && time->minute < 60 && time->second < 60 && time->reserved == 0 &&
           time->nanosecond < 1000000000;
}

bool mlocDateTimeValid(const mlocDateTime* dateTime)
{
    return mlocYearDateValid(&dateTime->date) && mlocTimeValid(&dateTime->time);
}

int64_t mlocDateTimeSeconds(const mlocDateTime* dateTime)
{
    const mlocTime* t = &dateTime->time;
    return ((int64_t)dateTime->date.day - epochDay) * secondsPerDay + t->hour * 3600 +
           t->minute * 60 + t->second;
}

// A field clamped into 0 to limit, or false when rejecting one outside.
static bool Regulate(int64_t value, int64_t limit, bool reject, int64_t* out)
{
    if (reject && (value < 0 || value > limit))
    {
        return false;
    }
    *out = value < 0 ? 0 : (value > limit ? limit : value);
    return true;
}

mlocResult mlocMakeTime(const mlocTimeInput* input, mlocOverflow overflow, mlocTime* timeOut)
{
    if (input == nullptr || timeOut == nullptr ||
        (overflow != mloc_overflowConstrain && overflow != mloc_overflowReject))
    {
        return mloc_errorInvalid;
    }
    bool reject = overflow == mloc_overflowReject;
    int64_t f[6];
    if (!Regulate(input->hour, 23, reject, &f[0]) || !Regulate(input->minute, 59, reject, &f[1]) ||
        !Regulate(input->second, 59, reject, &f[2]) ||
        !Regulate(input->millisecond, 999, reject, &f[3]) ||
        !Regulate(input->microsecond, 999, reject, &f[4]) ||
        !Regulate(input->nanosecond, 999, reject, &f[5]))
    {
        return mloc_errorInvalid;
    }
    *timeOut = (mlocTime){.hour = (uint8_t)f[0],
                          .minute = (uint8_t)f[1],
                          .second = (uint8_t)f[2],
                          .nanosecond = (uint32_t)(f[3] * 1000000 + f[4] * 1000 + f[5])};
    return mloc_success;
}

mlocResult mlocMakeDateTime(const mlocDate* date, const mlocTime* time, mlocDateTime* dateTimeOut)
{
    if (date == nullptr || time == nullptr || dateTimeOut == nullptr || !mlocYearDateValid(date) ||
        !mlocTimeValid(time))
    {
        return mloc_errorInvalid;
    }
    // ISODateTimeWithinLimits: more than a day before the first instant
    // is out, which on the first day is its midnight alone.
    if (date->day == mlocDateMinDay && time->hour == 0 && time->minute == 0 && time->second == 0 &&
        time->nanosecond == 0)
    {
        return mloc_errorInvalid;
    }
    *dateTimeOut = (mlocDateTime){.date = *date, .time = *time};
    return mloc_success;
}
