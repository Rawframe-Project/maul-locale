// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date arithmetic: mlocAddToDate and mlocDateUntil against the Intl Era
// and Month Code proposal's NonISODateAdd and NonISODateUntil transcribed
// step by step, their BalanceNonISODate loops and one-unit-at-a-time
// counts included, over the public functions; Temporal's duration limits;
// and results at the range's ends.

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

// Rounding to one unit with nothing larger, in every calendar and mode:
// the result is the truncated count or one increment more: the next one
// when the second date is its date, else the multiple whose date is
// nearer for the half modes (by days), and toward or away from zero as
// the directed modes say.
static void TestRoundingOneUnit(void)
{
    static const uint32_t s_increments[] = {1, 2, 3, 5};
    uint64_t state = UINT64_C(0x243F6A8885A308D3);
    bool all = true;
    long compared = 0;
    long ties = 0;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        for (int i = 0; i < 40; i++)
        {
            state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
            mlocDate one = {.day = 730000 + (int32_t)((state >> 33) % 9000),
                            .calendar = (uint8_t)c};
            mlocDate two = {.day = one.day + (int32_t)((state >> 13) % 4001) - 2000,
                            .calendar = (uint8_t)c};
            if (one.day == two.day)
            {
                continue;
            }
            int sign = two.day > one.day ? 1 : -1;
            for (int u = mloc_dateUnitYear; u <= mloc_dateUnitDay; u++)
            {
                mlocDateDuration exact = {0};
                CHECK(mlocDateUntil(&one, &two, (mlocDateUnit)u, &exact) == mloc_success,
                      "a difference");
                int64_t count = u == 0   ? exact.years
                                : u == 1 ? exact.months
                                : u == 2 ? exact.weeks
                                         : exact.days;
                for (size_t k = 0; k < sizeof(s_increments) / sizeof(s_increments[0]); k++)
                {
                    int64_t inc = s_increments[k];
                    int64_t low = count / inc * inc;
                    int64_t high = low + sign * inc;
                    mlocDateDuration lowD = {0};
                    mlocDateDuration highD = {0};
                    int64_t* lowField = u == 0   ? &lowD.years
                                        : u == 1 ? &lowD.months
                                        : u == 2 ? &lowD.weeks
                                                 : &lowD.days;
                    int64_t* highField = u == 0   ? &highD.years
                                         : u == 1 ? &highD.months
                                         : u == 2 ? &highD.weeks
                                                  : &highD.days;
                    *lowField = low;
                    *highField = high;
                    mlocDate lowDate = one;
                    mlocDate highDate = {0};
                    bool reach = (low == 0 || mlocAddToDate(&one, &lowD, mloc_overflowConstrain,
                                                            &lowDate) == mloc_success) &&
                                 mlocAddToDate(&one, &highD, mloc_overflowConstrain, &highDate) ==
                                     mloc_success;
                    if (!reach)
                    {
                        continue;
                    }
                    int64_t toLow = sign * ((int64_t)two.day - lowDate.day);
                    int64_t toHigh = sign * ((int64_t)highDate.day - two.day);
                    for (int m = 0; m <= mloc_roundTrunc; m++)
                    {
                        mlocDateRounding rounding = {(mlocDateUnit)u, (mlocDateUnit)u,
                                                     (uint32_t)inc, (mlocRoundingMode)m};
                        mlocDateDuration got = {0};
                        if (mlocDateUntilRounded(&one, &two, &rounding, &got) != mloc_success)
                        {
                            all = false;
                            continue;
                        }
                        int64_t value = *(u == 0   ? &got.years
                                          : u == 1 ? &got.months
                                          : u == 2 ? &got.weeks
                                                   : &got.days);
                        bool away = m == mloc_roundExpand || (m == mloc_roundCeil && sign > 0) ||
                                    (m == mloc_roundFloor && sign < 0);
                        bool toward = m == mloc_roundTrunc || (m == mloc_roundCeil && sign < 0) ||
                                      (m == mloc_roundFloor && sign > 0);
                        int64_t want = value;
                        if (toLow == 0)
                        {
                            want = low;
                        }
                        else if (toHigh == 0)
                        {
                            // The second date at the next multiple's own
                            // (a clamped day can put it there): that
                            // multiple, whatever the mode.
                            want = high;
                        }
                        else if (away)
                        {
                            want = high;
                        }
                        else if (toward)
                        {
                            want = low;
                        }
                        else if (toLow != toHigh)
                        {
                            want = toLow < toHigh ? low : high;
                        }
                        else
                        {
                            // A tie, as the half mode names it.
                            bool evenLow = (low / inc) % 2 == 0;
                            bool up = m == mloc_roundHalfExpand ||
                                      (m == mloc_roundHalfCeil && sign > 0) ||
                                      (m == mloc_roundHalfFloor && sign < 0) ||
                                      (m == mloc_roundHalfEven && !evenLow);
                            want = up ? high : low;
                            ties++;
                        }
                        all = all && (value == low || value == high) && value == want &&
                              got.years * (u != 0) == 0;
                        compared++;
                    }
                }
            }
        }
    }
    printf("%ld roundings compared, %ld ties\n", compared, ties);
    CHECK(all && ties > 1000, "one-unit roundings at the multiple the mode and the dates choose");
}

// Temporal's RoundRelativeDuration for two dates, transcribed step by step
// (ECMA-262's text): ComputeNudgeWindow, NudgeToCalendarUnit,
// NudgeToDayOrTime, BubbleRelativeDuration, with epoch values as days.
typedef struct SpecWindow
{
    int64_t r1, r2, startDay, endDay;
    mlocDateDuration start, end;
} SpecWindow;

static int64_t SpecTrunc(int64_t x, int64_t increment)
{
    return x / increment * increment;
}

static bool SpecAddDays(mlocDate origin, mlocDateDuration d, int64_t* dayOut)
{
    mlocDate out = {0};
    if (mlocAddToDate(&origin, &d, mloc_overflowConstrain, &out) != mloc_success)
    {
        return false;
    }
    *dayOut = out.day;
    return true;
}

static bool SpecWindowOf(int sign, mlocDateDuration d, mlocDate origin, int unit, int64_t inc,
                         bool shift, SpecWindow* w)
{
    if (unit == mloc_dateUnitYear)
    {
        int64_t years = SpecTrunc(d.years, inc);
        w->r1 = shift ? years + inc * sign : years;
        w->r2 = w->r1 + inc * sign;
        w->start = (mlocDateDuration){w->r1, 0, 0, 0};
        w->end = (mlocDateDuration){w->r2, 0, 0, 0};
    }
    else if (unit == mloc_dateUnitMonth)
    {
        int64_t months = SpecTrunc(d.months, inc);
        w->r1 = shift ? months + inc * sign : months;
        w->r2 = w->r1 + inc * sign;
        w->start = (mlocDateDuration){d.years, w->r1, 0, 0};
        w->end = (mlocDateDuration){d.years, w->r2, 0, 0};
    }
    else
    {
        int64_t weeksStart = 0;
        if (!SpecAddDays(origin, (mlocDateDuration){d.years, d.months, 0, 0}, &weeksStart))
        {
            return false;
        }
        mlocDate from = {.day = (int32_t)weeksStart, .calendar = origin.calendar};
        mlocDate to = {.day = (int32_t)(weeksStart + d.days), .calendar = origin.calendar};
        mlocDateDuration untilResult = {0};
        if (mlocDateUntil(&from, &to, mloc_dateUnitWeek, &untilResult) != mloc_success)
        {
            return false;
        }
        int64_t weeks = SpecTrunc(d.weeks + untilResult.weeks, inc);
        w->r1 = weeks;
        w->r2 = weeks + inc * sign;
        w->start = (mlocDateDuration){d.years, d.months, w->r1, 0};
        w->end = (mlocDateDuration){d.years, d.months, w->r2, 0};
    }
    bool startZero =
        w->start.years == 0 && w->start.months == 0 && w->start.weeks == 0 && w->start.days == 0;
    w->startDay = origin.day;
    if (!startZero && !SpecAddDays(origin, w->start, &w->startDay))
    {
        return false;
    }
    return SpecAddDays(origin, w->end, &w->endDay);
}

// ApplyUnsignedRoundingMode on x = r1 + (r2 - r1) * num / den, num/den in
// [0, 1), with GetUnsignedRoundingMode's table spelled out by mode.
static bool SpecRoundsToR2(int64_t num, int64_t den, int64_t r1Abs, int64_t inc,
                           mlocRoundingMode mode, bool negative)
{
    if (num == 0)
    {
        return false;
    }
    switch (mode)
    {
    case mloc_roundCeil:
        return !negative;
    case mloc_roundFloor:
        return negative;
    case mloc_roundExpand:
        return true;
    case mloc_roundTrunc:
        return false;
    default:
        break;
    }
    if (2 * num < den)
    {
        return false;
    }
    if (2 * num > den)
    {
        return true;
    }
    switch (mode)
    {
    case mloc_roundHalfCeil:
        return !negative;
    case mloc_roundHalfFloor:
        return negative;
    case mloc_roundHalfExpand:
        return true;
    case mloc_roundHalfTrunc:
        return false;
    default:
        return (r1Abs / inc) % 2 != 0;
    }
}

static bool SpecRound(mlocDate one, mlocDate two, mlocDateRounding r, mlocDateDuration* out)
{
    mlocDateDuration d = {0};
    if (mlocDateUntil(&one, &two, r.largestUnit, &d) != mloc_success)
    {
        return false;
    }
    if (one.day == two.day || (r.smallestUnit == mloc_dateUnitDay && r.increment == 1))
    {
        *out = d;
        return true;
    }
    int sign = (d.years < 0 || d.months < 0 || d.weeks < 0 || d.days < 0) ? -1 : 1;
    int64_t inc = r.increment;
    int64_t nudged = 0;
    bool expanded = false;
    if (r.smallestUnit != mloc_dateUnitDay)
    {
        SpecWindow w = {0};
        if (!SpecWindowOf(sign, d, one, r.smallestUnit, inc, false, &w))
        {
            return false;
        }
        bool inside = sign == 1 ? w.startDay <= two.day && two.day <= w.endDay
                                : w.endDay <= two.day && two.day <= w.startDay;
        if (!inside)
        {
            if (!SpecWindowOf(sign, d, one, r.smallestUnit, inc, true, &w))
            {
                return false;
            }
            expanded = true;
        }
        int64_t num = (int64_t)two.day - w.startDay;
        int64_t den = w.endDay - w.startDay;
        if (den < 0)
        {
            num = -num;
            den = -den;
        }
        bool toR2 =
            num == den || SpecRoundsToR2(num, den, w.r1 < 0 ? -w.r1 : w.r1, inc, r.mode, sign < 0);
        if (toR2)
        {
            expanded = true;
            d = w.end;
            nudged = w.endDay;
        }
        else
        {
            d = w.start;
            nudged = w.startDay;
        }
    }
    else
    {
        int64_t whole = d.days;
        int64_t q = whole < 0 ? -whole : whole;
        int64_t low = q / inc;
        bool up = SpecRoundsToR2(q % inc, inc, low * inc, inc, r.mode, whole < 0);
        int64_t rounded = (low + (up ? 1 : 0)) * inc * (whole < 0 ? -1 : 1);
        int64_t delta = rounded - whole;
        int deltaSign = (delta > 0) - (delta < 0);
        int wholeSign = (whole > 0) - (whole < 0);
        expanded = deltaSign == wholeSign;
        nudged = two.day + delta;
        d.days = rounded;
    }
    if (expanded && r.smallestUnit != mloc_dateUnitWeek && r.smallestUnit != r.largestUnit)
    {
        for (int unit = (int)r.smallestUnit - 1; unit >= (int)r.largestUnit; unit--)
        {
            if (unit == mloc_dateUnitWeek && r.largestUnit != mloc_dateUnitWeek)
            {
                continue;
            }
            mlocDateDuration end = unit == 0 ? (mlocDateDuration){d.years + sign, 0, 0, 0}
                                   : unit == 1
                                       ? (mlocDateDuration){d.years, d.months + sign, 0, 0}
                                       : (mlocDateDuration){d.years, d.months, d.weeks + sign, 0};
            int64_t endDay = 0;
            if (!SpecAddDays(one, end, &endDay))
            {
                return false;
            }
            int64_t beyond = nudged - endDay;
            int beyondSign = (beyond > 0) - (beyond < 0);
            if (beyondSign == -sign)
            {
                break;
            }
            d = end;
        }
    }
    *out = d;
    return true;
}

// Random pairs, every calendar, every pair of units, increments and
// modes, against the transcription.
static void TestRoundingAgainstSpecification(void)
{
    static const uint32_t s_increments[] = {1, 2, 3, 4, 5, 7, 10, 12};
    uint64_t state = UINT64_C(0x13198A2E03707344);
    bool all = true;
    long compared = 0;
    for (uint32_t c = 0; c < calendarCount; c++)
    {
        for (int i = 0; i < 24; i++)
        {
            state = state * UINT64_C(6364136223846793005) + UINT64_C(1442695040888963407);
            mlocDate one = {.day = 700000 + (int32_t)((state >> 33) % 60000),
                            .calendar = (uint8_t)c};
            int32_t span = (int32_t)((state >> 11) % 7001) - 3500;
            mlocDate two = {.day = one.day + span, .calendar = (uint8_t)c};
            for (int largest = 0; largest <= mloc_dateUnitDay; largest++)
            {
                for (int smallest = largest; smallest <= mloc_dateUnitDay; smallest++)
                {
                    for (size_t k = 0; k < sizeof(s_increments) / sizeof(s_increments[0]); k++)
                    {
                        for (int m = 0; m <= mloc_roundTrunc; m++)
                        {
                            mlocDateRounding r = {(mlocDateUnit)largest, (mlocDateUnit)smallest,
                                                  s_increments[k], (mlocRoundingMode)m};
                            mlocDateDuration want = {0};
                            mlocDateDuration got = {0};
                            bool wantOk = SpecRound(one, two, r, &want);
                            bool gotOk = mlocDateUntilRounded(&one, &two, &r, &got) == mloc_success;
                            all =
                                all && wantOk == gotOk &&
                                (!wantOk || (want.years == got.years && want.months == got.months &&
                                             want.weeks == got.weeks && want.days == got.days));
                            compared++;
                        }
                    }
                }
            }
        }
    }
    // Leap months: from a day of a Chinese or Dangi leap month (or of
    // Hebrew's Adar I) to days about a year on, where the year's code
    // passes the second date while the clamped date stays short of it, so
    // the window shifts once.
    long shifted = 0;
    const mlocCalendar leapCalendars[] = {mloc_calendarChinese, mloc_calendarDangi,
                                          mloc_calendarHebrew};
    for (size_t c = 0; c < 3; c++)
    {
        for (int32_t day = 730000; day < 745000; day++)
        {
            mlocDate one = {.day = day, .calendar = (uint8_t)leapCalendars[c]};
            mlocDateFields f = Fields(one);
            if (f.monthCodeLength != 4 || f.day % 7 != 3)
            {
                continue;
            }
            for (int32_t later = 340; later <= 420; later += 3)
            {
                mlocDate two = {.day = day + later, .calendar = one.calendar};
                for (int m = 0; m <= mloc_roundTrunc; m++)
                {
                    for (int unit = 0; unit < 6; unit++)
                    {
                        // Years, months and weeks alone, and below years.
                        static const int s_pairs[6][2] = {{0, 0}, {1, 1}, {2, 2},
                                                          {0, 1}, {0, 2}, {1, 2}};
                        mlocDateRounding r = {(mlocDateUnit)s_pairs[unit][0],
                                              (mlocDateUnit)s_pairs[unit][1], 1,
                                              (mlocRoundingMode)m};
                        mlocDateDuration want = {0};
                        mlocDateDuration got = {0};
                        bool wantOk = SpecRound(one, two, r, &want);
                        bool gotOk = mlocDateUntilRounded(&one, &two, &r, &got) == mloc_success;
                        all = all && wantOk == gotOk &&
                              (!wantOk || (want.years == got.years && want.months == got.months &&
                                           want.weeks == got.weeks && want.days == got.days));
                        compared++;
                        shifted++;
                    }
                }
            }
        }
    }
    printf("%ld rounded differences compared, %ld from leap months\n", compared, shifted);
    CHECK(all, "every rounding as Temporal's steps give it");
}

static void TestRoundingRefusals(void)
{
    mlocDate a = {.day = 738000, .calendar = mloc_calendarGregorian};
    mlocDate b = {.day = 738500, .calendar = mloc_calendarGregorian};
    mlocDateDuration d = {0};
    const mlocDateRounding bad[] = {
        {mloc_dateUnitMonth, mloc_dateUnitYear, 1, mloc_roundTrunc},
        {mloc_dateUnitYear, mloc_dateUnitDay, 0, mloc_roundTrunc},
        {mloc_dateUnitYear, mloc_dateUnitDay, 1000000001, mloc_roundTrunc},
        {mloc_dateUnitYear, mloc_dateUnitDay, 1, (mlocRoundingMode)9},
        {(mlocDateUnit)4, mloc_dateUnitDay, 1, mloc_roundTrunc},
    };
    bool refused = true;
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        refused = refused && mlocDateUntilRounded(&a, &b, &bad[i], &d) == mloc_errorInvalid;
    }
    const mlocDateRounding ok = {mloc_dateUnitYear, mloc_dateUnitYear, 1000000000, mloc_roundCeil};
    CHECK(refused && mlocDateUntilRounded(&a, &b, nullptr, &d) == mloc_errorInvalid &&
              mlocDateUntilRounded(&a, &b, &ok, nullptr) == mloc_errorInvalid,
          "bad settings and null pointers refused");
    // A multiple of a billion years lies past the range.
    CHECK(mlocDateUntilRounded(&a, &b, &ok, &d) == mloc_errorInvalid,
          "a multiple past the range refused");
    const mlocDateRounding trunc = {mloc_dateUnitYear, mloc_dateUnitYear, 1000000000,
                                    mloc_roundTrunc};
    CHECK(mlocDateUntilRounded(&a, &b, &trunc, &d) == mloc_errorInvalid,
          "the window's far end past the range refused, as Temporal's add throws");
}

int main(void)
{
    TestRoundingOneUnit();
    TestRoundingAgainstSpecification();
    TestRoundingRefusals();
    TestUntilRoundTrip();
    TestUntilAgainstSpecification();
    TestUntilFar();
    TestAgainstSpecification();
    TestDurationLimits();
    TestRangeEnds();
    return s_failures == 0 ? 0 : 1;
}
