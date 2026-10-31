// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The calls whose header says an output may be an input: each made once
// with an output apart and once with the input as its output, which must
// give the same result. A call that wrote its output before its last read
// of the input would give another.

#include "test_harness.h"

#include "maul-locale/calendar.h"
#include "maul-locale/decimal.h"
#include "maul-locale/list.h"
#include "maul-locale/locale.h"
#include "maul-locale/unit.h"
#include "maul-locale/zone.h"

#include <stdint.h>
#include <string.h>

static mlocDataSet s_data;
static mlocTimeZone s_zone;
static mlocDate s_date;
static mlocTime s_time;
static mlocDateTime s_dateTime;
// 2026-10-31T11:22:33.444555666Z, between two of the zone's transitions.
static const mlocInstant s_instant = {1793445753, 444555666};
static const mlocTimeDuration s_duration = {93784, 5006007};

static bool SameDecimal(const mlocDecimal* one, const mlocDecimal* two)
{
    return one->kind == two->kind && one->negative == two->negative && one->count == two->count &&
           one->exponent == two->exponent && one->fractionDigits == two->fractionDigits &&
           memcmp(one->digits, two->digits, one->count) == 0;
}

static bool SameDate(const mlocDate* one, const mlocDate* two)
{
    return one->day == two->day && one->calendar == two->calendar;
}

static bool SameTime(const mlocTime* one, const mlocTime* two)
{
    return one->hour == two->hour && one->minute == two->minute && one->second == two->second &&
           one->nanosecond == two->nanosecond;
}

static bool SameDateTime(const mlocDateTime* one, const mlocDateTime* two)
{
    return SameDate(&one->date, &two->date) && SameTime(&one->time, &two->time);
}

static bool SameInstant(const mlocInstant* one, const mlocInstant* two)
{
    return one->seconds == two->seconds && one->nanoseconds == two->nanoseconds;
}

static bool SameDuration(const mlocTimeDuration* one, const mlocTimeDuration* two)
{
    return one->seconds == two->seconds && one->nanoseconds == two->nanoseconds;
}

static void TestLocale(void)
{
    mlocLocale locale;
    mlocLocale apart;
    CHECK(mlocParseLocale("de-CH-u-co-phonebk", 18, &locale).status == mloc_success &&
              mlocGetParentLocale(&s_data, mloc_componentMain, &locale, &apart) == mloc_success &&
              mlocGetParentLocale(&s_data, mloc_componentMain, &locale, &locale) == mloc_success &&
              memcmp(&apart, &locale, sizeof locale) == 0,
          "mlocGetParentLocale: parentOut may be locale");
}

static void TestDates(void)
{
    mlocDateDuration duration = {.years = 1, .months = 2, .days = 3};
    mlocDateChanges changes = {.given = mloc_givenDay, .day = 1};
    mlocDate apart;
    mlocDate date = s_date;
    CHECK(mlocAddToDate(&s_date, &duration, mloc_overflowConstrain, &apart) == mloc_success &&
              mlocAddToDate(&date, &duration, mloc_overflowConstrain, &date) == mloc_success &&
              SameDate(&apart, &date) && !SameDate(&date, &s_date),
          "mlocAddToDate: dateOut may be date");
    date = s_date;
    CHECK(mlocWithDate(&s_date, &changes, mloc_overflowReject, &apart) == mloc_success &&
              mlocWithDate(&date, &changes, mloc_overflowReject, &date) == mloc_success &&
              SameDate(&apart, &date) && !SameDate(&date, &s_date),
          "mlocWithDate: dateOut may be date");
}

static void TestListGender(void)
{
    mlocLocale fr;
    mlocPersonGender genders[2] = {mloc_personMale, mloc_personFemale};
    mlocPersonGender apart;
    CHECK(mlocParseLocale("fr", 2, &fr).status == mloc_success &&
              mlocGetListGender(&s_data, &fr, genders, 2, &apart) == mloc_success &&
              mlocGetListGender(&s_data, &fr, genders, 2, &genders[0]) == mloc_success &&
              genders[0] == apart && apart == mloc_personMale,
          "mlocGetListGender: genderOut may be one of genders");
    genders[0] = mloc_personFemale;
    CHECK(mlocGetListGender(&s_data, &fr, genders, 1, &genders[0]) == mloc_success &&
              genders[0] == mloc_personFemale,
          "mlocGetListGender: genderOut may be the one gender");
}

static void TestConvertUnit(void)
{
    mlocDecimal value;
    mlocDecimal apart;
    mlocDecimal expected;
    CHECK(mlocParseDecimal("12.5", 4, &value).status == mloc_success &&
              mlocParseDecimal("3810", 4, &expected).status == mloc_success &&
              mlocConvertUnit(&s_data, "foot", 4, "millimeter", 10, nullptr, &value, &apart) ==
                  mloc_success &&
              mlocConvertUnit(&s_data, "foot", 4, "millimeter", 10, nullptr, &value, &value) ==
                  mloc_success &&
              SameDecimal(&apart, &value) && SameDecimal(&value, &expected),
          "mlocConvertUnit: valueOut may be value");
}

static void TestTimes(void)
{
    mlocTimeDuration apart;
    mlocTimeDuration duration = s_duration;
    CHECK(mlocAddTimeDurations(&s_duration, &s_duration, &apart) == mloc_success &&
              mlocAddTimeDurations(&duration, &s_duration, &duration) == mloc_success &&
              SameDuration(&apart, &duration),
          "mlocAddTimeDurations: durationOut may be one");
    duration = s_duration;
    CHECK(mlocAddTimeDurations(&s_duration, &duration, &duration) == mloc_success &&
              SameDuration(&apart, &duration),
          "mlocAddTimeDurations: durationOut may be two");
    duration = s_duration;
    CHECK(mlocAddTimeDurations(&duration, &duration, &duration) == mloc_success &&
              SameDuration(&apart, &duration),
          "mlocAddTimeDurations: durationOut may be both");
    duration = s_duration;
    CHECK(mlocRoundTimeDuration(&s_duration, mloc_timeUnitMinute, 15, mloc_roundHalfExpand,
                                &apart) == mloc_success &&
              mlocRoundTimeDuration(&duration, mloc_timeUnitMinute, 15, mloc_roundHalfExpand,
                                    &duration) == mloc_success &&
              SameDuration(&apart, &duration) && !SameDuration(&duration, &s_duration),
          "mlocRoundTimeDuration: durationOut may be duration");
    mlocTime timeApart;
    mlocTime time = s_time;
    int64_t days = 0;
    CHECK(mlocAddToTime(&s_time, &s_duration, &timeApart, &days) == mloc_success &&
              mlocAddToTime(&time, &s_duration, &time, &days) == mloc_success &&
              SameTime(&timeApart, &time) && !SameTime(&time, &s_time),
          "mlocAddToTime: timeOut may be time");
    time = s_time;
    CHECK(mlocRoundTime(&s_time, mloc_timeUnitHour, 1, mloc_roundHalfExpand, &timeApart, &days) ==
                  mloc_success &&
              mlocRoundTime(&time, mloc_timeUnitHour, 1, mloc_roundHalfExpand, &time, &days) ==
                  mloc_success &&
              SameTime(&timeApart, &time) && !SameTime(&time, &s_time),
          "mlocRoundTime: timeOut may be time");
}

static void TestDateTimes(void)
{
    mlocDateTimeDuration duration = {.date = {.months = 1}, .time = s_duration};
    mlocDateTime apart;
    mlocDateTime dateTime = s_dateTime;
    CHECK(mlocAddToDateTime(&s_dateTime, &duration, mloc_overflowConstrain, &apart) ==
                  mloc_success &&
              mlocAddToDateTime(&dateTime, &duration, mloc_overflowConstrain, &dateTime) ==
                  mloc_success &&
              SameDateTime(&apart, &dateTime) && !SameDateTime(&dateTime, &s_dateTime),
          "mlocAddToDateTime: dateTimeOut may be dateTime");
    dateTime = s_dateTime;
    CHECK(mlocRoundDateTime(&s_dateTime, mloc_timeUnitHour, 1, mloc_roundHalfExpand, &apart) ==
                  mloc_success &&
              mlocRoundDateTime(&dateTime, mloc_timeUnitHour, 1, mloc_roundHalfExpand, &dateTime) ==
                  mloc_success &&
              SameDateTime(&apart, &dateTime) && !SameDateTime(&dateTime, &s_dateTime),
          "mlocRoundDateTime: dateTimeOut may be dateTime");
}

static void TestInstants(void)
{
    mlocInstant apart;
    mlocInstant instant = s_instant;
    CHECK(mlocGetTimeZoneTransition(&s_zone, &s_instant, mloc_transitionNext, &apart) ==
                  mloc_success &&
              mlocGetTimeZoneTransition(&s_zone, &instant, mloc_transitionNext, &instant) ==
                  mloc_success &&
              SameInstant(&apart, &instant) && !SameInstant(&instant, &s_instant),
          "mlocGetTimeZoneTransition: transitionOut may be instant");
    instant = s_instant;
    CHECK(mlocAddToInstant(&s_instant, &s_duration, &apart) == mloc_success &&
              mlocAddToInstant(&instant, &s_duration, &instant) == mloc_success &&
              SameInstant(&apart, &instant) && !SameInstant(&instant, &s_instant),
          "mlocAddToInstant: instantOut may be instant");
    instant = s_instant;
    CHECK(mlocRoundInstant(&s_instant, mloc_timeUnitMinute, 1, mloc_roundHalfExpand, &apart) ==
                  mloc_success &&
              mlocRoundInstant(&instant, mloc_timeUnitMinute, 1, mloc_roundHalfExpand, &instant) ==
                  mloc_success &&
              SameInstant(&apart, &instant) && !SameInstant(&instant, &s_instant),
          "mlocRoundInstant: instantOut may be instant");
}

static void TestZoned(void)
{
    mlocDateTimeDuration duration = {.date = {.months = 1}, .time = s_duration};
    mlocInstant apart;
    mlocInstant instant = s_instant;
    CHECK(mlocAddToZonedDateTime(&s_zone, mloc_calendarGregorian, &s_instant, &duration,
                                 mloc_overflowConstrain, &apart) == mloc_success &&
              mlocAddToZonedDateTime(&s_zone, mloc_calendarGregorian, &instant, &duration,
                                     mloc_overflowConstrain, &instant) == mloc_success &&
              SameInstant(&apart, &instant) && !SameInstant(&instant, &s_instant),
          "mlocAddToZonedDateTime: instantOut may be instant");
    // A unit of the day, and a smaller one.
    static const mlocTimeUnit s_units[2] = {mloc_timeUnitDay, mloc_timeUnitHour};
    for (size_t i = 0; i < 2; i++)
    {
        instant = s_instant;
        CHECK(mlocRoundZonedDateTime(&s_zone, &s_instant, s_units[i], 1, mloc_roundHalfExpand,
                                     &apart) == mloc_success &&
                  mlocRoundZonedDateTime(&s_zone, &instant, s_units[i], 1, mloc_roundHalfExpand,
                                         &instant) == mloc_success &&
                  SameInstant(&apart, &instant) && !SameInstant(&instant, &s_instant),
              "mlocRoundZonedDateTime: instantOut may be instant");
    }
}

int main(void)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    s_data = (mlocDataSet){&builtin, 1};
    mlocDateInput day = {.year = 2026, .month = 1, .day = 31};
    mlocTimeInput time = {.hour = 11, .minute = 22, .second = 33, .millisecond = 444};
    CHECK(mlocFindTimeZone(&s_data, "Europe/Paris", 12, &s_zone) == mloc_success &&
              mlocMakeDate(mloc_calendarGregorian, &day, mloc_overflowReject, &s_date) ==
                  mloc_success &&
              mlocMakeTime(&time, mloc_overflowReject, &s_time) == mloc_success &&
              mlocMakeDateTime(&s_date, &s_time, &s_dateTime) == mloc_success,
          "a zone, a date and a time to work with");
    TestLocale();
    TestDates();
    TestListGender();
    TestConvertUnit();
    TestTimes();
    TestDateTimes();
    TestInstants();
    TestZoned();
    return s_failures == 0 ? 0 : 1;
}
