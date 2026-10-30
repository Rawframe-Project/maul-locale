// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The astronomy of the Chinese and Korean calendars (calendar_astro.h).
// Every value is an integer: days and degrees scaled by 2^32 (some sums
// by 2^40), Julian centuries by 2^48, sines by 2^62; products are taken
// to 128 bits and rounded to nearest. The terms are Calendrical
// Calculations' (its new-moon, solar-longitude, aberration, nutation and
// ephemeris-correction functions), written as the book's decimals.

#include "calendar_astro.h"

#include "calendar_arith.h"

enum
{
    // The lunation numbered 0 in the new-moon terms (January 6 of 2000).
    lunationOfJ2000 = 24724,
    // The years 50 centuries either side of 2000, past which the model
    // holds the fits that would otherwise run away.
    heldFirstYear = -3000,
    heldLastYear = 7000,
};

// The days of a mean lunation in billionths, and of a mean tropical year
// in millionths.
static const int64_t s_meanMonthNano = INT64_C(29530588861);
static const int64_t s_meanYearMicro = INT64_C(365242189);

#define POW10_0  INT64_C(1)
#define POW10_1  INT64_C(10)
#define POW10_2  INT64_C(100)
#define POW10_3  INT64_C(1000)
#define POW10_4  INT64_C(10000)
#define POW10_5  INT64_C(100000)
#define POW10_6  INT64_C(1000000)
#define POW10_7  INT64_C(10000000)
#define POW10_8  INT64_C(100000000)
#define POW10_9  INT64_C(1000000000)
#define POW10_10 INT64_C(10000000000)
#define POW10_11 INT64_C(100000000000)
#define POW10_12 INT64_C(1000000000000)
// m / d scaled by 2^q and rounded, for m at least 0 and (m mod d) * 2^q
// below 2^63.
#define FIXED_U(m, d, q)                                                                           \
    (((m) / (d)) * (INT64_C(1) << (q)) + (((m) % (d)) * (INT64_C(1) << (q)) + (d) / 2) / (d))
// The decimal m * 10^-p scaled by 2^q, for m at least 0 (a negative
// constant is written -FIXED).
#define FIXED(m, p, q) FIXED_U(INT64_C(m), POW10_##p, q)
#define DEG(m, p)      FIXED(m, p, 32)
#define DAYS40(m, p)   FIXED(m, p, 40)
#define Q40(m, p)      FIXED(m, p, 40)
#define SEC(m, p)      FIXED(m, p, 32)
// A decimal number of days as seconds, scaled by 2^32.
#define SEC_OF_DAYS(m, p) FIXED(INT64_C(m) * 86400, p, 32)

static const int64_t s_unit = INT64_C(1) << 32;
static const int64_t s_turn = INT64_C(360) << 32;
static const int64_t s_one62 = INT64_C(1) << 62;
// 2^62 / 360: a turn's degrees to a fraction of a turn scaled by 2^62.
static const int64_t s_turnInverse = INT64_C(12810238940076078);
// 2 pi scaled by 2^60.
static const int64_t s_twoPi = INT64_C(7244019458077122842);
// Calendrical Calculations' 0.000005729577951308232 (degrees in 10^-6
// radians), scaled by 2^62.
static const int64_t s_solarScale = INT64_C(26423014529538);
// 2^64 / 1236.85: lunations to Julian centuries, scaled by 2^48 (with
// a shift of 16).
static const int64_t s_centuriesPerLunation = INT64_C(14914293627933502);
// The centuries from 2000 past which the new moon's eccentricity factor
// is held, scaled by 2^48.
static const int64_t s_heldCenturies = INT64_C(50) << 48;
// 2^62 / 36525: days to Julian centuries, scaled by 2^62.
static const int64_t s_centuriesPerDay = INT64_C(126261081955575);

// The high 64 bits of a 64-by-64-bit product, and its low ones.
static uint64_t MultiplyHigh(uint64_t a, uint64_t b, uint64_t* lowOut)
{
#if defined(__SIZEOF_INT128__)
    __extension__ unsigned __int128 product = (unsigned __int128)a * b;
    *lowOut = (uint64_t)product;
    return (uint64_t)(product >> 64);
#else
    uint64_t aLow = (uint32_t)a;
    uint64_t aHigh = a >> 32;
    uint64_t bLow = (uint32_t)b;
    uint64_t bHigh = b >> 32;
    uint64_t lowLow = aLow * bLow;
    uint64_t lowHigh = aLow * bHigh;
    uint64_t highLow = aHigh * bLow;
    uint64_t highHigh = aHigh * bHigh;
    uint64_t middle = (lowLow >> 32) + (uint32_t)lowHigh + (uint32_t)highLow;
    *lowOut = (middle << 32) | (uint32_t)lowLow;
    return highHigh + (lowHigh >> 32) + (highLow >> 32) + (middle >> 32);
#endif
}

// a * b / 2^shift, rounded to nearest with ties away from zero, for a
// shift of 1 to 64 and a result that fits.
static int64_t Mul(int64_t a, int64_t b, uint32_t shift)
{
    bool negative = (a < 0) != (b < 0);
    uint64_t x = a < 0 ? 0 - (uint64_t)a : (uint64_t)a;
    uint64_t y = b < 0 ? 0 - (uint64_t)b : (uint64_t)b;
    uint64_t low = 0;
    uint64_t high = MultiplyHigh(x, y, &low);
    uint64_t half = UINT64_C(1) << (shift - 1);
    low += half;
    high += low < half;
    uint64_t magnitude = shift == 64 ? high : (high << (64 - shift)) | (low >> shift);
    return negative ? -(int64_t)magnitude : (int64_t)magnitude;
}

static int64_t FloorDiv(int64_t a, int64_t b)
{
    int64_t q = a / b;
    return q - (a % b != 0 && (a < 0) != (b < 0));
}

static int64_t FloorMod(int64_t a, int64_t b)
{
    return a - FloorDiv(a, b) * b;
}

// a / d rounded to nearest, ties away from zero, for d above 0.
static int64_t RoundDiv(int64_t a, int64_t d)
{
    return (a >= 0 ? a + d / 2 : a - d / 2) / d;
}

// m / d scaled by 2^32 and rounded, for d from 1 to 2^31.
static int64_t Fixed(int64_t m, int64_t d)
{
    int64_t q = FloorDiv(m, d);
    return q * s_unit + RoundDiv((m - q * d) * s_unit, d);
}

enum
{
    // The most angles one call of Sines takes.
    maxSines = 64,
};

// The steps of the series of Sines: Horner's, from the 19th and 20th
// powers down, of the cosines in v[0, cosines) and of the sines in
// v[first, count), each step over all of them so that their
// multiplications overlap. The divisors are written out as constants.
static void SineSeries(const int64_t* r2, int64_t* v, uint32_t cosines, uint32_t first,
                       uint32_t count)
{
#define MLOC_COSINE_STEP(i)                                                                        \
    for (uint32_t k = 0; k < cosines; k++)                                                         \
    {                                                                                              \
        v[k] = s_one62 - Mul(r2[k], v[k], 62) / ((2 * (i) - 1) * (2 * (i)));                       \
    }
#define MLOC_SINE_STEP(i)                                                                          \
    for (uint32_t k = first; k < count; k++)                                                       \
    {                                                                                              \
        v[k] = s_one62 - Mul(r2[k], v[k], 62) / ((2 * (i)) * (2 * (i) + 1));                       \
    }
    MLOC_COSINE_STEP(10)
    MLOC_COSINE_STEP(9)
    MLOC_SINE_STEP(9)
    MLOC_COSINE_STEP(8)
    MLOC_SINE_STEP(8)
    MLOC_COSINE_STEP(7)
    MLOC_SINE_STEP(7)
    MLOC_COSINE_STEP(6)
    MLOC_SINE_STEP(6)
    MLOC_COSINE_STEP(5)
    MLOC_SINE_STEP(5)
    MLOC_COSINE_STEP(4)
    MLOC_SINE_STEP(4)
    MLOC_COSINE_STEP(3)
    MLOC_SINE_STEP(3)
    MLOC_COSINE_STEP(2)
    MLOC_SINE_STEP(2)
    MLOC_COSINE_STEP(1)
    MLOC_SINE_STEP(1)
#undef MLOC_COSINE_STEP
#undef MLOC_SINE_STEP
}

// The sines of up to maxSines angles, scaled by 2^62: each angle is
// reduced to an eighth of a turn and the Taylor series of its sine or
// cosine there taken to the 19th and 20th powers, past the scale's
// resolution.
static void Sines(const int64_t* degrees, int64_t* values, uint32_t count)
{
    // The series to take, cosines from the front and sines from the back.
    int64_t r[maxSines];
    int64_t r2[maxSines];
    int64_t v[maxSines];
    uint32_t angle[maxSines];
    bool negative[maxSines];
    uint32_t cosines = 0;
    uint32_t first = count;
    for (uint32_t j = 0; j < count; j++)
    {
        uint64_t turn = (uint64_t)Mul(FloorMod(degrees[j], s_turn), s_turnInverse, 32);
        uint32_t octant = (uint32_t)(turn >> 59) & 7;
        int64_t within = (int64_t)(turn & ((UINT64_C(1) << 59) - 1));
        if (octant % 2 == 1)
        {
            within = (INT64_C(1) << 59) - within;
        }
        bool cosine = octant == 1 || octant == 2 || octant == 5 || octant == 6;
        uint32_t at = cosine ? cosines++ : --first;
        r[at] = Mul(within, s_twoPi, 60);
        r2[at] = Mul(r[at], r[at], 62);
        v[at] = s_one62;
        angle[at] = j;
        negative[at] = octant >= 4;
    }
    SineSeries(r2, v, cosines, first, count);
    for (uint32_t k = first; k < count; k++)
    {
        v[k] = Mul(r[k], v[k], 62);
    }
    for (uint32_t k = 0; k < count; k++)
    {
        values[angle[k]] = negative[k] ? -v[k] : v[k];
    }
}

// A polynomial in num / den, coefficients first to last power, all
// scaled alike.
static int64_t PolyRatio(const int64_t* a, int count, int64_t num, int64_t den)
{
    int64_t acc = a[count - 1];
    for (int i = count - 2; i >= 0; i--)
    {
        acc = a[i] + RoundDiv(acc * num, den);
    }
    return acc;
}

// A polynomial in a fraction scaled by 2^62.
static int64_t PolyFraction(const int64_t* a, int count, int64_t x)
{
    int64_t acc = a[count - 1];
    for (int i = count - 2; i >= 0; i--)
    {
        acc = a[i] + Mul(acc, x, 62);
    }
    return acc;
}

// The ephemeris correction in seconds scaled by 2^32, by the Gregorian
// year's range.
static int64_t DeltaSeconds(int64_t year)
{
    static const int64_t s_2006[] = {SEC(6292, 2), SEC(32217, 5), SEC(5589, 6)};
    static const int64_t s_1987[] = {SEC(6386, 2),  SEC(3345, 4),   -SEC(60374, 6),
                                     SEC(17275, 7), SEC(651814, 9), SEC(2373599, 11)};
    static const int64_t s_1900[] = {-SEC_OF_DAYS(2, 5),     SEC_OF_DAYS(297, 6),
                                     SEC_OF_DAYS(25184, 6),  -SEC_OF_DAYS(181133, 6),
                                     SEC_OF_DAYS(553040, 6), -SEC_OF_DAYS(861938, 6),
                                     SEC_OF_DAYS(677066, 6), -SEC_OF_DAYS(212591, 6)};
    static const int64_t s_1800[] = {
        -SEC_OF_DAYS(9, 6),       SEC_OF_DAYS(3844, 6),     SEC_OF_DAYS(83563, 6),
        SEC_OF_DAYS(865736, 6),   SEC_OF_DAYS(4867575, 6),  SEC_OF_DAYS(15845535, 6),
        SEC_OF_DAYS(31332267, 6), SEC_OF_DAYS(38291999, 6), SEC_OF_DAYS(28316289, 6),
        SEC_OF_DAYS(11636204, 6), SEC_OF_DAYS(2043794, 6)};
    static const int64_t s_1700[] = {SEC(8118780842, 9), -SEC(5092142, 9), SEC(3336121, 9),
                                     -SEC(266484, 10)};
    static const int64_t s_1600[] = {SEC(120, 0), -SEC(9808, 4), -SEC(1532, 5), SEC(140272128, 12)};
    static const int64_t s_500[] = {SEC(15742, 1),    -SEC(55601, 2),   SEC(7123472, 5),
                                    SEC(319781, 6),   -SEC(8503463, 7), -SEC(5050998, 9),
                                    SEC(83572073, 10)};
    static const int64_t s_0[] = {SEC(105836, 1),   -SEC(101441, 2),  SEC(3378311, 5),
                                  -SEC(5952053, 6), -SEC(1798452, 7), SEC(22174192, 9),
                                  SEC(90316521, 10)};
    int64_t y1820 = year - 1820;
    if (year >= 2051 && year <= 2150)
    {
        return Fixed(-200000 + 32 * y1820 * y1820 + 5628 * (2150 - year), 10000);
    }
    if (year >= 2006 && year <= 2050)
    {
        return PolyRatio(s_2006, 3, year - 2000, 1);
    }
    if (year >= 1987 && year <= 2005)
    {
        return PolyRatio(s_1987, 6, year - 2000, 1);
    }
    if (year >= 1800 && year <= 1986)
    {
        // Centuries from January 1 of 1900 to July 1 of the year.
        int64_t middle = mlocArithmeticToDay(mloc_calendarGregorian, year, 7, 1);
        int64_t c = (middle - 693596) * s_centuriesPerDay;
        return year >= 1900 ? PolyFraction(s_1900, 8, c) : PolyFraction(s_1800, 11, c);
    }
    if (year >= 1700 && year <= 1799)
    {
        return PolyRatio(s_1700, 4, year - 1700, 1);
    }
    if (year >= 1600 && year <= 1699)
    {
        return PolyRatio(s_1600, 4, year - 1600, 1);
    }
    if (year >= 500 && year <= 1599)
    {
        return PolyRatio(s_500, 7, year - 1000, 100);
    }
    if (year >= -499 && year <= 499)
    {
        return PolyRatio(s_0, 7, year, 100);
    }
    return Fixed(-200000 + 32 * y1820 * y1820, 10000);
}

int64_t mlocAstroDeltaT(int64_t moment)
{
    int32_t day = (int32_t)FloorDiv(moment, s_unit);
    int64_t year = mlocArithmeticFromDay(mloc_calendarGregorian, day).year;
    // Held past 50 centuries from 2000, as the new moon's eccentricity
    // factor is: the parabola would move universal time by years (2,133
    // days at the range's start) and walk the new year around the
    // Gregorian year.
    year = year < heldFirstYear ? heldFirstYear : year > heldLastYear ? heldLastYear : year;
    return RoundDiv(DeltaSeconds(year), 86400);
}

// Julian centuries of dynamical time from noon of January 1 of 2000,
// scaled by 2^48.
static int64_t Centuries(int64_t moment)
{
    int64_t days = moment + mlocAstroDeltaT(moment) - DEG(7301205, 1);
    int64_t q = FloorDiv(days, 36525);
    int64_t r = days - q * 36525;
    return q * 65536 + RoundDiv(r * 65536, 36525);
}

typedef struct SolarTerm
{
    int32_t x;
    int64_t y;
    int64_t z;
} SolarTerm;

typedef struct LunarTerm
{
    int64_t v;
    int8_t e;
    int8_t x;
    int8_t y;
    int8_t z;
} LunarTerm;

typedef struct PlanetaryTerm
{
    int64_t l;
    int64_t i;
    // Degrees a lunation, in millionths.
    int64_t j;
} PlanetaryTerm;

static const SolarTerm s_solar[] = {
    {403406, DEG(27054861, 5), DEG(9287892, 7)},
    {195207, DEG(34019128, 5), DEG(359991376958, 7)},
    {119433, DEG(6391854, 5), DEG(359994089666, 7)},
    {112392, DEG(3312622, 4), DEG(359987287385, 7)},
    {3891, DEG(317843, 3), DEG(7199820261, 5)},
    {2819, DEG(86631, 3), DEG(719984403, 4)},
    {1721, DEG(240052, 3), DEG(3600035726, 5)},
    {660, DEG(31026, 2), DEG(719974812, 4)},
    {350, DEG(24723, 2), DEG(329644678, 4)},
    {334, DEG(26087, 2), -DEG(19441, 3)},
    {314, DEG(29782, 2), DEG(4452671117, 4)},
    {268, DEG(34314, 2), DEG(45036884, 3)},
    {242, DEG(16679, 2), DEG(31008, 4)},
    {234, DEG(8153, 2), DEG(225184434, 4)},
    {158, DEG(35, 1), -DEG(199739, 4)},
    {132, DEG(13275, 2), DEG(659289345, 4)},
    {129, DEG(18295, 2), DEG(90380293, 4)},
    {114, DEG(16203, 2), DEG(30347684, 4)},
    {99, DEG(298, 1), DEG(33718148, 3)},
    {93, DEG(2664, 1), DEG(3034448, 3)},
    {86, DEG(2492, 1), -DEG(2280773, 3)},
    {78, DEG(1576, 1), DEG(29929992, 3)},
    {72, DEG(2578, 1), DEG(31556493, 3)},
    {68, DEG(1851, 1), DEG(149588, 3)},
    {64, DEG(699, 1), DEG(903775, 2)},
    {46, DEG(8, 0), DEG(107997405, 3)},
    {38, DEG(1971, 1), -DEG(4444176, 3)},
    {37, DEG(2504, 1), DEG(151771, 3)},
    {32, DEG(653, 1), DEG(67555316, 3)},
    {29, DEG(1627, 1), DEG(3155608, 2)},
    {28, DEG(3415, 1), -DEG(456154, 2)},
    {27, DEG(2916, 1), DEG(107996706, 3)},
    {27, DEG(985, 1), DEG(1221655, 3)},
    {25, DEG(1467, 1), DEG(62894167, 3)},
    {24, DEG(110, 0), DEG(31437369, 3)},
    {21, DEG(52, 1), DEG(14578298, 3)},
    {21, DEG(3426, 1), -DEG(31931757, 3)},
    {20, DEG(2309, 1), DEG(34777243, 3)},
    {18, DEG(2561, 1), DEG(1221999, 3)},
    {17, DEG(453, 1), DEG(62894511, 3)},
    {14, DEG(2429, 1), -DEG(4442039, 3)},
    {13, DEG(1152, 1), DEG(107997909, 3)},
    {13, DEG(1518, 1), DEG(119066, 3)},
    {13, DEG(2853, 1), DEG(16859071, 3)},
    {12, DEG(533, 1), -DEG(4578, 3)},
    {10, DEG(1266, 1), DEG(26895292, 3)},
    {10, DEG(2057, 1), -DEG(39127, 3)},
    {10, DEG(859, 1), DEG(12297536, 3)},
    {10, DEG(1461, 1), DEG(90073778, 3)},
};
static const LunarTerm s_lunar[] = {
    {-DAYS40(4072, 4), 0, 0, 1, 0}, {DAYS40(17241, 5), 1, 1, 0, 0}, {DAYS40(1608, 5), 0, 0, 2, 0},
    {DAYS40(1039, 5), 0, 0, 0, 2},  {DAYS40(739, 5), 1, -1, 1, 0},  {-DAYS40(514, 5), 1, 1, 1, 0},
    {DAYS40(208, 5), 2, 2, 0, 0},   {-DAYS40(111, 5), 0, 0, 1, -2}, {-DAYS40(57, 5), 0, 0, 1, 2},
    {DAYS40(56, 5), 1, 1, 2, 0},    {-DAYS40(42, 5), 0, 0, 3, 0},   {DAYS40(42, 5), 1, 1, 0, 2},
    {DAYS40(38, 5), 1, 1, 0, -2},   {-DAYS40(24, 5), 1, -1, 2, 0},  {-DAYS40(7, 5), 0, 2, 1, 0},
    {DAYS40(4, 5), 0, 0, 2, -2},    {DAYS40(4, 5), 0, 3, 0, 0},     {DAYS40(3, 5), 0, 1, 1, -2},
    {DAYS40(3, 5), 0, 0, 2, 2},     {-DAYS40(3, 5), 0, 1, 1, 2},    {DAYS40(3, 5), 0, -1, 1, 2},
    {-DAYS40(2, 5), 0, -1, 1, -2},  {-DAYS40(2, 5), 0, 1, 3, 0},    {DAYS40(2, 5), 0, 0, 4, 0},
};
static const PlanetaryTerm s_planetary[] = {
    {DAYS40(165, 6), DEG(25188, 2), 16321},    {DAYS40(164, 6), DEG(25183, 2), 26651886},
    {DAYS40(126, 6), DEG(34942, 2), 36412478}, {DAYS40(11, 5), DEG(8466, 2), 18206239},
    {DAYS40(62, 6), DEG(14174, 2), 53303771},  {DAYS40(6, 5), DEG(20714, 2), 2453732},
    {DAYS40(56, 6), DEG(15484, 2), 7306860},   {DAYS40(47, 6), DEG(3452, 2), 27261239},
    {DAYS40(42, 6), DEG(20719, 2), 121824},    {DAYS40(4, 5), DEG(29134, 2), 1844379},
    {DAYS40(37, 6), DEG(16172, 2), 24198154},  {DAYS40(35, 6), DEG(23956, 2), 25513099},
    {DAYS40(23, 6), DEG(33155, 2), 3592518},
};

int64_t mlocAstroSolarLongitude(int64_t moment)
{
    enum
    {
        solarCount = sizeof(s_solar) / sizeof(s_solar[0]),
        // The terms, then the aberration's cosine and the nutation's two.
        angleCount = solarCount + 3,
    };
    static_assert((size_t)angleCount <= (size_t)maxSines,
                  "Sines takes the solar longitude's angles");
    int64_t c = Centuries(moment);
    int64_t c2 = Mul(c, c, 64);
    int64_t angles[angleCount];
    for (uint32_t i = 0; i < solarCount; i++)
    {
        angles[i] = s_solar[i].y + Mul(s_solar[i].z, c, 48);
    }
    angles[solarCount] = DEG(17763, 2) + Mul(DEG(3599901848, 5), c, 48) + DEG(90, 0);
    angles[solarCount + 1] =
        DEG(12490, 2) - Mul(DEG(1934134, 3), c, 48) + Mul(DEG(2063, 6), c2, 32);
    angles[solarCount + 2] =
        DEG(20111, 2) + Mul(DEG(720015377, 4), c, 48) + Mul(DEG(57, 5), c2, 32);
    int64_t sines[angleCount];
    Sines(angles, sines, angleCount);
    int64_t sum = 0;
    for (uint32_t i = 0; i < solarCount; i++)
    {
        sum += Mul(s_solar[i].x, sines[i], 30);
    }
    int64_t lambda =
        Mul(sum, s_solarScale, 62) + DEG(2827771834, 7) + Mul(DEG(3600076953744, 8), c, 48);
    int64_t aberration = Mul(DEG(974, 7), sines[solarCount], 62) - DEG(5575, 6);
    int64_t nutation = -Mul(DEG(4778, 6), sines[solarCount + 1], 62) -
                       Mul(DEG(3667, 7), sines[solarCount + 2], 62);
    return FloorMod(lambda + aberration + nutation, s_turn);
}

// (mantissa * 10^-places * k) mod 360 degrees, scaled by 2^32: exact for
// an integer k.
static int64_t DegreesTimes(int64_t mantissa, int64_t scale, int64_t k)
{
    return Fixed(FloorMod(mantissa * k, 360 * scale), scale);
}

// A polynomial's coefficients from c^2 up, the book's decimals scaled by
// 2^60; its partial sums stay below 1 for any c the dates reach.
typedef struct SquaredPoly
{
    int64_t a[3];
    int count;
} SquaredPoly;

// c^2 times the polynomial, for c scaled by 2^48, scaled by 2^32.
static int64_t SquaredValue(const SquaredPoly* p, int64_t c)
{
    int64_t acc = p->a[p->count - 1];
    for (int i = p->count - 2; i >= 0; i--)
    {
        acc = p->a[i] + Mul(acc, c, 48);
    }
    return Mul(Mul(acc, c, 60), c, 64);
}

int64_t mlocAstroNewMoon(int64_t n)
{
    // 0.00015437, -0.00000015 and 0.00000000073 days.
    static const SquaredPoly s_approx = {
        {INT64_C(177976492666159), INT64_C(-172938225691), INT64_C(841632698)}, 3};
    // -0.0000014 and -0.00000011 degrees.
    static const SquaredPoly s_solarAnomaly = {{INT64_C(-1614090106450), INT64_C(-126821365507)},
                                               2};
    // 0.0107582, 0.00001238 and -0.000000058 degrees.
    static const SquaredPoly s_lunarAnomaly = {
        {INT64_C(12403360130861381), INT64_C(14273168227033), INT64_C(-66869447267)}, 3};
    // -0.0016118, -0.00000227 and 0.000000011 degrees.
    static const SquaredPoly s_moonArgument = {
        {INT64_C(-1858278881125316), INT64_C(-2617131815458), INT64_C(12682136551)}, 3};
    // 0.0020672 and 0.00000215 degrees.
    static const SquaredPoly s_omega = {{INT64_C(2383319334323274), INT64_C(2478781234905)}, 2};
    int64_t k = n - lunationOfJ2000;
    int64_t c = Mul(k, s_centuriesPerLunation, 16);
    int64_t c2 = Mul(c, c, 64);
    int64_t approx = DEG(7301205, 1) + DEG(509766, 5) + Fixed(s_meanMonthNano * k, 1000000000) +
                     SquaredValue(&s_approx, c);
    // The eccentricity factor is held past 50 centuries from 2000, where
    // its square would grow the terms it scales to days and the months
    // to 20 or 40 days.
    int64_t held = c < -s_heldCenturies  ? -s_heldCenturies
                   : c > s_heldCenturies ? s_heldCenturies
                                         : c;
    int64_t e = Q40(1, 0) - Mul(Q40(2516, 6), held, 48) - Mul(Q40(74, 7), Mul(held, held, 64), 32);
    int64_t e2 = Mul(e, e, 40);
    int64_t m =
        DEG(25534, 4) + DegreesTimes(2910535670, 100000000, k) + SquaredValue(&s_solarAnomaly, c);
    int64_t mp = DEG(2015643, 4) + DegreesTimes(38581693528, 100000000, k) +
                 SquaredValue(&s_lunarAnomaly, c);
    int64_t f = DEG(1607108, 4) + DegreesTimes(39067050284, 100000000, k) +
                SquaredValue(&s_moonArgument, c);
    int64_t omega =
        DEG(1247746, 4) + DegreesTimes(-156375588, 100000000, k) + SquaredValue(&s_omega, c);
    enum
    {
        lunarCount = sizeof(s_lunar) / sizeof(s_lunar[0]),
        planetaryCount = sizeof(s_planetary) / sizeof(s_planetary[0]),
        // The lunar and planetary terms, then omega's and the 0.000325's.
        angleCount = lunarCount + planetaryCount + 2,
    };
    static_assert((size_t)angleCount <= (size_t)maxSines, "Sines takes the new moon's angles");
    int64_t angles[angleCount];
    for (uint32_t i = 0; i < lunarCount; i++)
    {
        const LunarTerm* t = &s_lunar[i];
        angles[i] =
            t->x * FloorMod(m, s_turn) + t->y * FloorMod(mp, s_turn) + t->z * FloorMod(f, s_turn);
    }
    for (uint32_t i = 0; i < planetaryCount; i++)
    {
        angles[lunarCount + i] = s_planetary[i].i + DegreesTimes(s_planetary[i].j, 1000000, k);
    }
    angles[lunarCount + planetaryCount] = omega;
    angles[lunarCount + planetaryCount + 1] =
        DEG(29977, 2) + Mul(DEG(1328475848, 7), c, 48) - Mul(DEG(9173, 6), c2, 32);
    int64_t sines[angleCount];
    Sines(angles, sines, angleCount);
    int64_t sum = -Mul(DAYS40(17, 5), sines[lunarCount + planetaryCount], 62);
    for (uint32_t i = 0; i < lunarCount; i++)
    {
        const LunarTerm* t = &s_lunar[i];
        int64_t term = Mul(t->v, sines[i], 62);
        sum += t->e == 0 ? term : Mul(term, t->e == 1 ? e : e2, 40);
    }
    sum += Mul(DAYS40(325, 6), sines[lunarCount + planetaryCount + 1], 62);
    for (uint32_t i = 0; i < planetaryCount; i++)
    {
        sum += Mul(s_planetary[i].l, sines[lunarCount + i], 62);
    }
    int64_t dynamical = approx + RoundDiv(sum, 256);
    return dynamical - mlocAstroDeltaT(dynamical);
}

int64_t mlocAstroNewMoonIndex(int64_t moment, int64_t* moonOut)
{
    int64_t month = Fixed(s_meanMonthNano, 1000000000);
    // The mean month, then steps of the true one (within a day of the
    // mean however far the terms drift), then the exact search. A new
    // moon computed is kept while it is n's, so none is computed twice.
    int64_t n = FloorDiv(moment - DEG(11458922816, 9), month);
    int64_t moon = 0;
    bool known = false;
    for (int i = 0; i < 8; i++)
    {
        moon = mlocAstroNewMoon(n);
        int64_t step = FloorDiv(moment - moon, month);
        if (step == 0)
        {
            known = true;
            break;
        }
        n += step;
    }
    for (int64_t before = mlocAstroNewMoon(n - 1); before >= moment;
         before = mlocAstroNewMoon(n - 1))
    {
        n--;
        moon = before;
        known = true;
    }
    for (moon = known ? moon : mlocAstroNewMoon(n); moon < moment; moon = mlocAstroNewMoon(n))
    {
        n++;
    }
    *moonOut = moon;
    return n;
}

int64_t mlocAstroPriorSolarLongitude(int64_t degrees, int64_t moment)
{
    int64_t rate = Fixed(s_meanYearMicro, 360000000);
    int64_t tau =
        moment - Mul(rate, FloorMod(mlocAstroSolarLongitude(moment) - degrees, s_turn), 32);
    int64_t delta =
        FloorMod(mlocAstroSolarLongitude(tau) - degrees + DEG(180, 0), s_turn) - DEG(180, 0);
    int64_t estimate = tau - Mul(rate, delta, 32);
    return moment < estimate ? moment : estimate;
}
