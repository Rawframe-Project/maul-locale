// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date arithmetic (record mloc-0040): mlocAddToDate and mlocDateUntil
// against the Intl Era and Month Code proposal's NonISODateAdd and
// NonISODateUntil transcribed step by step, their BalanceNonISODate loops
// and one-unit-at-a-time counts included, over the public functions;
// Temporal's duration limits; and results at the range's ends.

#include "test_harness.h"

#include "maul-locale/calendar.h"

#include <string.h>

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

// CompareSurpasses: a month code (code not null) compares as text, a
// month place (code null) as a number.
static bool CompareSurpasses(int sign, int64_t year, const char* code, size_t codeLength,
                             int64_t month, int64_t day, const mlocDateFields* target)
{
    if (year != target->year)
    {
        return sign * (year - target->year) > 0;
    }
    if (code != nullptr && !(codeLength == target->monthCodeLength &&
                             memcmp(code, target->monthCode, codeLength) == 0))
    {
        size_t n = codeLength < target->monthCodeLength ? codeLength : target->monthCodeLength;
        int c = memcmp(code, target->monthCode, n);
        bool greater = c > 0 || (c == 0 && codeLength > target->monthCodeLength);
        return sign > 0 ? greater : !greater;
    }
    if (code == nullptr && month != target->month)
    {
        return sign * (month - target->month) > 0;
    }
    if (day != target->day)
    {
        return sign * (day - target->day) > 0;
    }
    return false;
}

// NonISODateSurpasses, as the proposal writes it.
static bool SpecSurpasses(int sign, mlocDate one, mlocDate two, int64_t years, int64_t months,
                          int64_t weeks, int64_t days)
{
    mlocCalendar calendar = (mlocCalendar)one.calendar;
    mlocDateFields parts = Fields(one);
    mlocDateFields target = Fields(two);
    int64_t y0 = parts.year + years;
    if (CompareSurpasses(sign, y0, parts.monthCode, parts.monthCodeLength, 0, parts.day, &target))
    {
        return true;
    }
    mlocDateInput input = {.year = (int32_t)y0,
                           .monthCode = parts.monthCode,
                           .monthCodeLength = parts.monthCodeLength,
                           .day = 1};
    mlocDate first = {0};
    CHECK(mlocMakeDate(calendar, &input, mloc_overflowConstrain, &first) == mloc_success,
          "the month code constrained");
    Balanced added = Balance(calendar, y0, Fields(first).month + months, 1);
    if (CompareSurpasses(sign, added.year, nullptr, 0, added.month, parts.day, &target))
    {
        return true;
    }
    if (weeks == 0 && days == 0)
    {
        return false;
    }
    Balanced end = Balance(calendar, added.year, added.month + 1, 0);
    int64_t day = parts.day <= end.day ? parts.day : end.day;
    Balanced b = Balance(calendar, end.year, end.month, day + 7 * weeks + days);
    return CompareSurpasses(sign, b.year, nullptr, 0, b.month, b.day, &target);
}

// NonISODateUntil, as the proposal writes it: one unit at a time.
static mlocDateDuration SpecUntil(mlocDate one, mlocDate two, mlocDateUnit largest)
{
    mlocDateDuration d = {0};
    int sign = two.day > one.day ? 1 : two.day < one.day ? -1 : 0;
    if (sign == 0)
    {
        return d;
    }
    if (largest == mloc_dateUnitYear)
    {
        for (int64_t c = sign; !SpecSurpasses(sign, one, two, c, 0, 0, 0); c += sign)
        {
            d.years = c;
        }
    }
    if (largest == mloc_dateUnitYear || largest == mloc_dateUnitMonth)
    {
        for (int64_t c = sign; !SpecSurpasses(sign, one, two, d.years, c, 0, 0); c += sign)
        {
            d.months = c;
        }
    }
    if (largest == mloc_dateUnitWeek)
    {
        for (int64_t c = sign; !SpecSurpasses(sign, one, two, d.years, d.months, c, 0); c += sign)
        {
            d.weeks = c;
        }
    }
    for (int64_t c = sign; !SpecSurpasses(sign, one, two, d.years, d.months, d.weeks, c); c += sign)
    {
        d.days = c;
    }
    return d;
}

// Every calendar, between days a few years apart either way, in every
// largest unit (weeks and days over shorter spans, the loops being one
// day at a time).
static void TestUntilAgainstSpecification(void)
{
    static const int32_t s_offsets[] = {0,   1,   -1,   28,   -29,  30,    31,   -59,
                                        89,  177, -183, 354,  -355, 365,   383,  -384,
                                        385, 730, -731, 1000, 1096, -1461, 1830, -2000};
    bool all = true;
    long compared = 0;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        for (int32_t step = 0; step < 1400; step += 127)
        {
            mlocDate one = {.day = 738000 + step, .calendar = (uint8_t)c};
            for (size_t i = 0; i < sizeof(s_offsets) / sizeof(s_offsets[0]); i++)
            {
                mlocDate two = {.day = one.day + s_offsets[i], .calendar = (uint8_t)c};
                for (int u = 0; u <= mloc_dateUnitDay; u++)
                {
                    if (u >= mloc_dateUnitWeek && (s_offsets[i] > 100 || s_offsets[i] < -100))
                    {
                        continue;
                    }
                    mlocDateDuration want = SpecUntil(one, two, (mlocDateUnit)u);
                    mlocDateDuration got = {-7, -7, -7, -7};
                    bool ok = mlocDateUntil(&one, &two, (mlocDateUnit)u, &got) == mloc_success;
                    all = all && ok && got.years == want.years && got.months == want.months &&
                          got.weeks == want.weeks && got.days == want.days;
                    compared++;
                }
            }
        }
    }
    printf("%ld differences compared\n", compared);
    CHECK(all, "every difference as the proposal's loops count it");
}

// Far apart: Temporal's whole range in every unit and calendar, each
// result added back to the first date reaching the second (exact for
// differences counted to days).
static void TestUntilFar(void)
{
    bool all = true;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        mlocDate first = {.day = mlocDateMinDay, .calendar = (uint8_t)c};
        mlocDate last = {.day = mlocDateMaxDay, .calendar = (uint8_t)c};
        const mlocDate pairs[][2] = {{first, last}, {last, first}};
        for (size_t p = 0; p < 2; p++)
        {
            for (int u = 0; u <= mloc_dateUnitDay; u++)
            {
                mlocDateDuration d = {0};
                mlocDate back = {0};
                all = all &&
                      mlocDateUntil(&pairs[p][0], &pairs[p][1], (mlocDateUnit)u, &d) ==
                          mloc_success &&
                      mlocAddToDate(&pairs[p][0], &d, mloc_overflowConstrain, &back) ==
                          mloc_success &&
                      back.day == pairs[p][1].day;
            }
        }
    }
    CHECK(all, "the range's ends apart in every unit, the difference adding back");
    mlocDate a = {.day = 738000, .calendar = mloc_calendarGregorian};
    mlocDate b = {.day = 738000, .calendar = mloc_calendarHebrew};
    mlocDateDuration d = {0};
    CHECK(mlocDateUntil(&a, &b, mloc_dateUnitDay, &d) == mloc_errorInvalid &&
              mlocDateUntil(&a, &a, (mlocDateUnit)4, &d) == mloc_errorInvalid &&
              mlocDateUntil(nullptr, &a, mloc_dateUnitDay, &d) == mloc_errorInvalid &&
              mlocDateUntil(&a, &a, mloc_dateUnitDay, nullptr) == mloc_errorInvalid,
          "two calendars, an unknown unit and null pointers refused");
}

// Random pairs over Temporal's whole range in every calendar and unit:
// the difference added back to the first date gives the second, as the
// specification's construction of the difference requires.
static void TestUntilRoundTrip(void)
{
    uint64_t state = UINT64_C(0x9E3779B97F4A7C15);
    const int64_t span = (int64_t)mlocDateMaxDay - mlocDateMinDay + 1;
    bool all = true;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        bool lunisolar = c == mloc_calendarChinese || c == mloc_calendarDangi;
        for (int i = 0; i < (lunisolar ? 24 : 900); i++)
        {
            int32_t days[2];
            for (int k = 0; k < 2; k++)
            {
                state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
                days[k] = (int32_t)(mlocDateMinDay + (int64_t)((state >> 16) % (uint64_t)span));
            }
            // Every third pair close together, where months and days matter.
            if (i % 3 == 0)
            {
                int64_t near = days[0] + (int64_t)(state >> 40) % 900 - 450;
                days[1] = (int32_t)(near < mlocDateMinDay   ? mlocDateMinDay
                                    : near > mlocDateMaxDay ? mlocDateMaxDay
                                                            : near);
            }
            mlocDate one = {.day = days[0], .calendar = (uint8_t)c};
            mlocDate two = {.day = days[1], .calendar = (uint8_t)c};
            for (int u = 0; u <= mloc_dateUnitDay; u++)
            {
                mlocDateDuration d = {0};
                mlocDate back = {0};
                bool ok = mlocDateUntil(&one, &two, (mlocDateUnit)u, &d) == mloc_success &&
                          mlocAddToDate(&one, &d, mloc_overflowConstrain, &back) == mloc_success &&
                          back.day == two.day;
                bool oneSign = !((d.years > 0 || d.months > 0 || d.weeks > 0 || d.days > 0) &&
                                 (d.years < 0 || d.months < 0 || d.weeks < 0 || d.days < 0));
                all = all && ok && oneSign;
            }
        }
    }
    CHECK(all, "random differences of one sign, adding back to the second date");
}

int main(void)
{
    TestUntilRoundTrip();
    TestUntilAgainstSpecification();
    TestUntilFar();
    TestAgainstSpecification();
    TestDurationLimits();
    TestRangeEnds();
    return s_failures == 0 ? 0 : 1;
}
