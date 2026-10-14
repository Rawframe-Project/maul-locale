// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Adding durations to dates (record mloc-0040): mlocAddToDate against the
// Intl Era and Month Code proposal's NonISODateAdd transcribed step by
// step, its BalanceNonISODate loops included, over the public functions;
// Temporal's duration limits; and results at the range's ends.

#include "test_harness.h"

#include "maul-locale/calendar.h"

enum
{
    calendarCount = 18,
};

static mlocDateFields Fields(mlocDate date)
{
    mlocDateFields f = {0};
    CHECK(mlocGetDateFields(&date, &f) == mloc_success, "fields");
    return f;
}

static bool MakeDay(mlocCalendar calendar, int64_t year, uint32_t month, uint32_t day,
                    mlocDate* out)
{
    mlocDateInput input = {.year = (int32_t)year, .month = month, .day = day};
    return mlocMakeDate(calendar, &input, mloc_overflowReject, out) == mloc_success;
}

static uint32_t MonthsInYear(mlocCalendar calendar, int64_t year)
{
    mlocDate d = {0};
    return MakeDay(calendar, year, 1, 1, &d) ? Fields(d).monthsInYear : 0;
}

static uint32_t DaysInMonth(mlocCalendar calendar, int64_t year, uint32_t month)
{
    mlocDate d = {0};
    return MakeDay(calendar, year, month, 1, &d) ? Fields(d).daysInMonth : 0;
}

typedef struct Balanced
{
    int64_t year;
    int64_t month;
    int64_t day;
} Balanced;

// BalanceNonISODate, as the proposal writes it.
static Balanced Balance(mlocCalendar calendar, int64_t year, int64_t month, int64_t day)
{
    int64_t months = MonthsInYear(calendar, year);
    while (month <= 0)
    {
        year -= 1;
        months = MonthsInYear(calendar, year);
        month += months;
    }
    while (month > months)
    {
        month -= months;
        year += 1;
        months = MonthsInYear(calendar, year);
    }
    int64_t days = DaysInMonth(calendar, year, (uint32_t)month);
    while (day <= 0)
    {
        month -= 1;
        if (month == 0)
        {
            year -= 1;
            months = MonthsInYear(calendar, year);
            month = months;
        }
        days = DaysInMonth(calendar, year, (uint32_t)month);
        day += days;
    }
    while (day > days)
    {
        day -= days;
        month += 1;
        if (month > months)
        {
            year += 1;
            months = MonthsInYear(calendar, year);
            month = 1;
        }
        days = DaysInMonth(calendar, year, (uint32_t)month);
    }
    return (Balanced){year, month, day};
}

// NonISODateAdd, as the proposal writes it; false where it throws.
static bool SpecAdd(mlocDate date, const mlocDateDuration* d, mlocOverflow overflow, mlocDate* out)
{
    mlocCalendar calendar = (mlocCalendar)date.calendar;
    mlocDateFields parts = Fields(date);
    int64_t y0 = parts.year + d->years;
    // ConstrainMonthCode and MonthCodeToOrdinal, through mlocMakeDate's
    // month codes on day 1.
    mlocDateInput input = {.year = (int32_t)y0,
                           .monthCode = parts.monthCode,
                           .monthCodeLength = parts.monthCodeLength,
                           .day = 1};
    mlocDate first = {0};
    if (mlocMakeDate(calendar, &input, overflow, &first) != mloc_success)
    {
        return false;
    }
    int64_t m0 = Fields(first).month;
    Balanced end = Balance(calendar, y0, m0 + d->months + 1, 0);
    int64_t day = parts.day;
    if (day > end.day)
    {
        if (overflow == mloc_overflowReject)
        {
            return false;
        }
        day = end.day;
    }
    Balanced b = Balance(calendar, end.year, end.month, day + 7 * d->weeks + d->days);
    return MakeDay(calendar, b.year, (uint32_t)b.month, (uint32_t)b.day, out);
}

// Every calendar, from days across a few years, by small durations of
// either sign in each unit and together, both overflows.
static void TestAgainstSpecification(void)
{
    static const mlocDateDuration s_durations[] = {
        {1, 0, 0, 0},   {-1, 0, 0, 0},  {3, 0, 0, 0},   {-19, 0, 0, 0}, {0, 1, 0, 0},
        {0, -1, 0, 0},  {0, 13, 0, 0},  {0, -25, 0, 0}, {0, 40, 0, 0},  {0, 0, 1, 0},
        {0, 0, -5, 0},  {0, 0, 0, 30},  {0, 0, 0, -45}, {1, 2, 0, 3},   {-2, -11, -1, -9},
        {0, 12, 0, 29}, {5, 70, 3, 100}};
    bool all = true;
    long compared = 0;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        mlocCalendar calendar = (mlocCalendar)c;
        mlocDate start = {0};
        CHECK(mlocMakeDateFromDay(calendar, 738000, &start) == mloc_success, "a start");
        for (int32_t step = 0; step < 1200; step += 7)
        {
            mlocDate date = {.day = start.day + step, .calendar = (uint8_t)calendar};
            for (size_t i = 0; i < sizeof(s_durations) / sizeof(s_durations[0]); i++)
            {
                for (int o = 0; o < 2; o++)
                {
                    mlocOverflow overflow = (mlocOverflow)o;
                    mlocDate want = {0};
                    mlocDate got = {0};
                    bool wantOk = SpecAdd(date, &s_durations[i], overflow, &want);
                    bool gotOk =
                        mlocAddToDate(&date, &s_durations[i], overflow, &got) == mloc_success;
                    all = all && wantOk == gotOk &&
                          (!wantOk || (want.day == got.day && got.calendar == date.calendar));
                    compared++;
                }
            }
        }
    }
    printf("%ld additions compared\n", compared);
    CHECK(all, "every addition as the proposal's steps give it");
}

// Temporal's limits: one sign; years, months and weeks below 2^32; days
// at most 2^53 seconds.
static void TestDurationLimits(void)
{
    mlocDate date = {0};
    CHECK(mlocMakeDateFromDay(mloc_calendarGregorian, 738000, &date) == mloc_success, "a date");
    mlocDate out = {0};
    const mlocDateDuration mixed = {1, -1, 0, 0};
    const mlocDateDuration mixedDays = {0, 0, -1, 1};
    CHECK(mlocAddToDate(&date, &mixed, mloc_overflowConstrain, &out) == mloc_errorInvalid &&
              mlocAddToDate(&date, &mixedDays, mloc_overflowConstrain, &out) == mloc_errorInvalid,
          "a duration of two signs refused");
    const mlocDateDuration tooMany[] = {{INT64_C(4294967296), 0, 0, 0},
                                        {0, -INT64_C(4294967296), 0, 0},
                                        {0, 0, INT64_C(4294967296), 0},
                                        {0, 0, 0, INT64_C(104249991375)},
                                        {0, 0, 0, INT64_MIN}};
    bool refused = true;
    for (size_t i = 0; i < sizeof(tooMany) / sizeof(tooMany[0]); i++)
    {
        refused = refused && mlocAddToDate(&date, &tooMany[i], mloc_overflowConstrain, &out) ==
                                 mloc_errorInvalid;
    }
    CHECK(refused, "fields past Temporal's limits refused");
    // At the limits the duration is valid; only the result is out of range.
    const mlocDateDuration atLimit = {0, 0, 0, INT64_C(104249991374)};
    CHECK(mlocAddToDate(&date, &atLimit, mloc_overflowConstrain, &out) == mloc_errorInvalid,
          "days at the limit: a result past the range");
    const mlocDateDuration none = {0};
    CHECK(mlocAddToDate(&date, &none, mloc_overflowReject, &out) == mloc_success &&
              out.day == date.day,
          "nothing added");
    CHECK(mlocAddToDate(nullptr, &none, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDate(&date, nullptr, mloc_overflowReject, &out) == mloc_errorInvalid &&
              mlocAddToDate(&date, &none, mloc_overflowReject, nullptr) == mloc_errorInvalid &&
              mlocAddToDate(&date, &none, (mlocOverflow)2, &out) == mloc_errorInvalid,
          "null pointers and an unknown overflow refused");
    mlocDate bad = date;
    bad.reserved[1] = 1;
    CHECK(mlocAddToDate(&bad, &none, mloc_overflowReject, &out) == mloc_errorInvalid,
          "a date the library did not make refused");
}

// The range's ends: the last and first days reached and not passed. A
// duration has one sign, so a year past the range never comes back
// within it: the largest durations are refused in every calendar,
// Chinese and Dangi included, without overflow.
static void TestRangeEnds(void)
{
    static const mlocDateDuration s_far[] = {
        {INT64_C(4294967295), 0, 0, 0},
        {-INT64_C(4294967295), 0, 0, 0},
        {0, INT64_C(4294967295), 0, 0},
        {0, -INT64_C(4294967295), 0, 0},
        {0, 0, INT64_C(4294967295), 0},
        {0, 0, 0, -INT64_C(104249991374)},
        {INT64_C(4294967295), INT64_C(4294967295), INT64_C(4294967295), INT64_C(104249991374)},
        {1000, 0, 0, 0},
        {-1000, 0, 0, 0},
    };
    bool all = true;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        mlocCalendar calendar = (mlocCalendar)c;
        mlocDate last = {0};
        mlocDate first = {0};
        mlocDate out = {0};
        CHECK(mlocMakeDateFromDay(calendar, mlocDateMaxDay, &last) == mloc_success &&
                  mlocMakeDateFromDay(calendar, mlocDateMinDay, &first) == mloc_success,
              "the ends");
        const mlocDateDuration back = {0, 0, 0, -1};
        const mlocDateDuration on = {0, 0, 0, 1};
        all = all && mlocAddToDate(&last, &on, mloc_overflowConstrain, &out) == mloc_errorInvalid &&
              mlocAddToDate(&last, &back, mloc_overflowConstrain, &out) == mloc_success &&
              mlocAddToDate(&out, &on, mloc_overflowConstrain, &out) == mloc_success &&
              out.day == mlocDateMaxDay &&
              mlocAddToDate(&first, &back, mloc_overflowConstrain, &out) == mloc_errorInvalid;
        const mlocDateDuration year = {1, 0, 0, 0};
        const mlocDateDuration yearBack = {-1, 0, 0, 0};
        all = all &&
              mlocAddToDate(&last, &year, mloc_overflowConstrain, &out) == mloc_errorInvalid &&
              mlocAddToDate(&first, &yearBack, mloc_overflowConstrain, &out) == mloc_errorInvalid;
        mlocDate middle = {0};
        CHECK(mlocMakeDateFromDay(calendar, 738000, &middle) == mloc_success, "a middle day");
        // From the middle every far duration but a millennium passes an
        // end; from an end, every one going outward.
        for (size_t j = 0; j < sizeof(s_far) / sizeof(s_far[0]); j++)
        {
            const mlocDateDuration* d = &s_far[j];
            bool forward = d->years > 0 || d->months > 0 || d->weeks > 0 || d->days > 0;
            bool millennium = d->years == 1000 || d->years == -1000;
            mlocResult fromMiddle = mlocAddToDate(&middle, d, mloc_overflowConstrain, &out);
            mlocResult fromEnd =
                mlocAddToDate(forward ? &last : &first, d, mloc_overflowConstrain, &out);
            all = all &&
                  (millennium ? fromMiddle == mloc_success : fromMiddle == mloc_errorInvalid) &&
                  fromEnd == mloc_errorInvalid;
        }
    }
    CHECK(all, "the range's ends reached and not passed, the largest durations refused");
}

int main(void)
{
    TestAgainstSpecification();
    TestDurationLimits();
    TestRangeEnds();
    return s_failures == 0 ? 0 : 1;
}
