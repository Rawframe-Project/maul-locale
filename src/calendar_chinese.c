// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Korean calendars (calendar_chinese.h). Days are those
// of maul-locale/calendar.h; moments and angles are calendar_astro.h's.

#include "calendar_chinese.h"

#include "calendar_arith.h"
#include "calendar_astro.h"

enum
{
    // The lunations from the month after the eleventh to the next
    // eleventh: 11, or 12 in a span with a leap month; one more is room.
    maxSuiMonths = 13,
    // The days zones changed: January 1 of 1929 in China; April 1 of
    // 1908, January 1 of 1912, March 21 of 1954 and August 10 of 1961 in
    // Korea.
    china1929 = 704188,
    korea1908 = 696608,
    korea1912 = 697978,
    korea1954 = 713398,
    korea1961 = 716097,
};

static const int64_t s_unit = INT64_C(1) << 32;
static const int64_t s_winter = INT64_C(270) << 32;
// Zones as days scaled by 2^32: Beijing's mean time (1397/180 hours),
// UTC+8, Seoul's mean time (3809/450 hours), UTC+8:30 and UTC+9.
static const int64_t s_beijing = INT64_C(1388904933);
static const int64_t s_utc8 = INT64_C(1431655765);
static const int64_t s_seoul = INT64_C(1514771336);
static const int64_t s_utc8h30 = INT64_C(1521134251);
static const int64_t s_utc9 = INT64_C(1610612736);

static int64_t FloorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return q - (a % b != 0 && (a < 0) != (b < 0));
}

// The zone the calendar keeps on a day (Calendrical Calculations'
// chinese-location and korean-location).
static int64_t Zone(bool korea, int64_t day)
{
    if (!korea)
    {
        return day < china1929 ? s_beijing : s_utc8;
    }
    if (day < korea1908)
    {
        return s_seoul;
    }
    if (day < korea1912 || (day >= korea1954 && day < korea1961))
    {
        return s_utc8h30;
    }
    return s_utc9;
}

// The universal moment a day begins.
static int64_t Midnight(bool korea, int64_t day)
{
    return day * s_unit - Zone(korea, day);
}

// The day a new moon falls on there.
static int64_t DayOfMoon(bool korea, int64_t n)
{
    int64_t moon = mlocAstroNewMoon(n);
    return FloorDiv(moon + Zone(korea, FloorDiv(moon, s_unit)), s_unit);
}

// The number of the first new moon on or after a day.
static int64_t NewMoonOnOrAfter(bool korea, int64_t day)
{
    return mlocAstroNewMoonIndex(Midnight(korea, day));
}

// The day of the last new moon before a day.
static int64_t NewMoonBefore(bool korea, int64_t day)
{
    return DayOfMoon(korea, mlocAstroNewMoonIndex(Midnight(korea, day)) - 1);
}

// The major solar term in effect when a day begins, 1 to 12.
static int MajorTerm(bool korea, int64_t day)
{
    int64_t degrees = mlocAstroSolarLongitude(Midnight(korea, day)) >> 32;
    return (int)((1 + degrees / 30) % 12) + 1;
}

// The day of the winter solstice on or before a day: the day at whose
// end the sun's longitude has passed 270 degrees.
static int64_t SolsticeOnOrBefore(bool korea, int64_t day)
{
    int64_t estimate = mlocAstroPriorSolarLongitude(s_winter, Midnight(korea, day + 1));
    int64_t d = FloorDiv(estimate, s_unit) - 1;
    for (int i = 0; i < 8 && mlocAstroSolarLongitude(Midnight(korea, d + 1)) <= s_winter; i++)
    {
        d++;
    }
    return d;
}

// A solstice-to-solstice span: its months from the one after the
// eleventh to the next eleventh, which of them is the leap month (-1
// for none) and which is the first of the year.
typedef struct Sui
{
    int64_t start[maxSuiMonths + 1];
    // The number of the new moon each month begins with.
    int64_t moon[maxSuiMonths + 1];
    int count;
    int leap;
    int newYear;
} Sui;

static void MakeSui(bool korea, int64_t solstice, int64_t nextSolstice, Sui* s)
{
    int64_t nextEleventh = NewMoonBefore(korea, nextSolstice + 1);
    s->moon[0] = NewMoonOnOrAfter(korea, solstice + 1);
    s->start[0] = DayOfMoon(korea, s->moon[0]);
    s->count = 0;
    while (s->count < maxSuiMonths && s->start[s->count] < nextEleventh)
    {
        s->moon[s->count + 1] = NewMoonOnOrAfter(korea, s->start[s->count] + 1);
        s->start[s->count + 1] = DayOfMoon(korea, s->moon[s->count + 1]);
        s->count++;
    }
    // In a span of twelve lunations the first month with no major
    // solar term (the one in effect at its start is in effect at the
    // next month's) is the leap month.
    s->leap = -1;
    if (s->count == 12)
    {
        int term = MajorTerm(korea, s->start[0]);
        for (int j = 0; j < s->count && s->leap < 0; j++)
        {
            int next = MajorTerm(korea, s->start[j + 1]);
            s->leap = next == term ? j : -1;
            term = next;
        }
        // The rule always finds one over the dates' range; were it not
        // to, the month before the eleventh keeps the count of months.
        s->leap = s->leap < 0 ? s->count - 1 : s->leap;
    }
    s->newYear = s->leap == 0 || s->leap == 1 ? 2 : 1;
    s->newYear = s->newYear < s->count ? s->newYear : s->count;
}

// The year from the first month of one span to the first of the next.
static void YearFrom(const Sui* a, const Sui* b, LunisolarYear* out)
{
    int n = 0;
    out->leap = 0;
    for (int j = a->newYear; j <= a->count && n < 13; j++)
    {
        out->leap = j == a->leap ? (uint8_t)(n + 1) : out->leap;
        out->start[n++] = (int32_t)a->start[j];
    }
    for (int j = 0; j < b->newYear && n < 13; j++)
    {
        out->leap = j == b->leap ? (uint8_t)(n + 1) : out->leap;
        out->start[n++] = (int32_t)b->start[j];
    }
    out->count = (uint8_t)n;
    out->start[n] = (int32_t)b->start[b->newYear];
    out->lunation = (int32_t)a->moon[a->newYear];
    // Numbered by the Gregorian year whose July 1 it holds.
    int32_t gregorian = mlocArithmeticFromDay(mloc_calendarGregorian, out->start[0]).year;
    int64_t july = mlocArithmeticToDay(mloc_calendarGregorian, gregorian, 7, 1);
    out->year = july >= out->start[0] ? gregorian : gregorian + 1;
}

void mlocChineseModelYearOf(bool korea, int32_t day, LunisolarYear* out)
{
    int64_t solstice = SolsticeOnOrBefore(korea, day);
    int64_t next = SolsticeOnOrBefore(korea, solstice + 370);
    Sui a = {0};
    MakeSui(korea, solstice, next, &a);
    Sui b = {0};
    if (day >= a.start[a.newYear])
    {
        MakeSui(korea, next, SolsticeOnOrBefore(korea, next + 370), &b);
        YearFrom(&a, &b, out);
    }
    else
    {
        MakeSui(korea, SolsticeOnOrBefore(korea, solstice - 1), solstice, &b);
        YearFrom(&b, &a, out);
    }
}

int64_t mlocChineseModelMonthStart(bool korea, int64_t lunation)
{
    return DayOfMoon(korea, lunation);
}

void mlocChineseModelYear(bool korea, int64_t year, LunisolarYear* out)
{
    mlocChineseModelYearOf(korea, (int32_t)mlocArithmeticToDay(mloc_calendarGregorian, year, 7, 1),
                           out);
}
