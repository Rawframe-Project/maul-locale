// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Korean calendars (calendar_chinese.h). Days are those
// of maul-locale/calendar.h; moments and angles are calendar_astro.h's.

#include "calendar_chinese.h"

#include "calendar_arith.h"
#include "calendar_astro.h"
#include "invariant.h"

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
    // The months made so far, start[0] to start[made], and how far the
    // leap month has been sought: term is the major solar term in effect
    // when month sought begins.
    int made;
    int sought;
    int term;
} Sui;

// Makes the months up to start[last] and seeks the leap month among
// them: in a span of twelve lunations, the first month with no major
// solar term (the one in effect at its start is in effect at the next
// month's).
static void MakeMonths(bool korea, int last, Sui* s)
{
    for (; s->made < last; s->made++)
    {
        // The next month begins with the next new moon: a month's own
        // falls on its first day, and the next one a month later.
        s->moon[s->made + 1] = s->moon[s->made] + 1;
        s->start[s->made + 1] = DayOfMoon(korea, s->moon[s->made + 1]);
    }
    for (; s->count == 12 && s->sought < s->made && s->leap < 0; s->sought++)
    {
        int next = MajorTerm(korea, s->start[s->sought + 1]);
        s->leap = next == s->term ? s->sought : -1;
        s->term = next;
    }
}

// Begins a span: its count of months, its first three and which month
// opens its year, all a year that ends in it needs. FinishSui makes the
// rest.
static void BeginSui(bool korea, int64_t solstice, int64_t nextSolstice, Sui* s)
{
    // Months begin on days in the order of their new moons, so the span
    // holds those from the first one after the solstice to the eleventh
    // month's, the last one before the next.
    int64_t eleventh = NewMoonOnOrAfter(korea, nextSolstice + 1) - 1;
    s->moon[0] = NewMoonOnOrAfter(korea, solstice + 1);
    s->start[0] = DayOfMoon(korea, s->moon[0]);
    // A span between solstices holds twelve or thirteen new moons.
    MLOC_ASSERT(eleventh - s->moon[0] >= 2 && eleventh - s->moon[0] <= maxSuiMonths);
    s->count = (int)(eleventh - s->moon[0]);
    s->made = 0;
    s->sought = 0;
    s->leap = -1;
    s->term = s->count == 12 ? MajorTerm(korea, s->start[0]) : 0;
    MakeMonths(korea, 2, s);
    // A leap month first or second opens the year a month later; one
    // further on does not change which month opens it.
    s->newYear = s->leap == 0 || s->leap == 1 ? 2 : 1;
    s->newYear = s->newYear < s->count ? s->newYear : s->count;
}

// Makes the rest of a span's months and finds its leap month.
static void FinishSui(bool korea, Sui* s)
{
    MakeMonths(korea, s->count, s);
    // The rule always finds one over the dates' range; were it not to,
    // the month before the eleventh keeps the count of months.
    s->leap = s->count == 12 && s->leap < 0 ? s->count - 1 : s->leap;
}

// The year from the first month of one span to the first of the next.
static void YearFrom(const Sui* a, const Sui* b, LunisolarYear* out)
{
    // From one new year to the next are twelve or thirteen months, which
    // the loops below take whole.
    MLOC_ASSERT(a->count + 1 - a->newYear + b->newYear == 12 ||
                a->count + 1 - a->newYear + b->newYear == 13);
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
    // The year is made of one span whole and the first months of the
    // other: the next one's, or this one's for a day before its new year.
    Sui a = {0};
    BeginSui(korea, solstice, next, &a);
    Sui b = {0};
    if (day >= a.start[a.newYear])
    {
        FinishSui(korea, &a);
        BeginSui(korea, next, SolsticeOnOrBefore(korea, next + 370), &b);
        YearFrom(&a, &b, out);
    }
    else
    {
        BeginSui(korea, SolsticeOnOrBefore(korea, solstice - 1), solstice, &b);
        FinishSui(korea, &b);
        YearFrom(&b, &a, out);
    }
    MLOC_ASSERT(out->start[0] <= day && day < out->start[out->count]);
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
