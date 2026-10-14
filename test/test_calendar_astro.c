// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The fixed-point astronomy (record mloc-0040) against Calendrical
// Calculations' formulas evaluated in 45-digit decimal arithmetic by a
// program outside the repository: new moons within 2^-28
// of a day and solar longitudes within 2^-26 of a degree for five
// thousand years either side of 2000, and the model well formed over
// the whole range of dates.

#include "calendar_astro.h"
#include "test_harness.h"

#include <stdlib.h>

typedef struct AstroCase
{
    int64_t input;
    int64_t expected;
} AstroCase;

// Lunation, moment.
static const AstroCase s_newMoons[] = {
    {-37000, INT64_C(-4692773404456266)},
    {-36000, INT64_C(-4565937387738138)},
    {-30001, INT64_C(-3805068283653912)},
    {-18000, INT64_C(-2282943673851035)},
    {-9001, INT64_C(-1141574896550631)},
    {-4950, INT64_C(-627774642863451)},
    {-2000, INT64_C(-253618061381545)},
    {-500, INT64_C(-63367295612802)},
    {0, INT64_C(49215698741)},
    {1, INT64_C(177048808481)},
    {777, INT64_C(98595503202752)},
    {5000, INT64_C(634212753003258)},
    {12000, INT64_C(1522043430096543)},
    {19774, INT64_C(2508042786955568)},
    {21011, INT64_C(2664932579833718)},
    {23456, INT64_C(2975042764597212)},
    {24724, INT64_C(3135866259099851)},
    {24725, INT64_C(3135994182337907)},
    {26000, INT64_C(3297704348155322)},
    {27500, INT64_C(3487955361155540)},
    {29674, INT64_C(3763689747420494)},
    {33333, INT64_C(4227770660016798)},
    {40000, INT64_C(5073364451891851)},
    {52000, INT64_C(6595361635474296)},
    {61000, INT64_C(7736854120219271)},
    {86000, INT64_C(10907677071748378)},
};
// Moment (days), solar longitude.
static const AstroCase s_longitudes[] = {
    {INT64_C(-4724466173083648), INT64_C(-1427065438979)},
    {INT64_C(-1717987992141824), INT64_C(-600556325739)},
    {INT64_C(4294967296), INT64_C(-337502251918)},
    {INT64_C(784354443788288), INT64_C(-340691024494)},
    {INT64_C(1568702445125632), INT64_C(-346018979407)},
    {INT64_C(2508360758853632), INT64_C(-342564827801)},
    {INT64_C(2664905659593523), INT64_C(-671907005319)},
    {INT64_C(2978972136636416), INT64_C(-342937927618)},
    {INT64_C(3053723894939648), INT64_C(-887467494431)},
    {INT64_C(3135843669639168), INT64_C(1204176076542)},
    {INT64_C(3173551871885312), INT64_C(1264596677794)},
    {INT64_C(3328603412496384), INT64_C(1010370179968)},
    {INT64_C(3650724349083648), INT64_C(7281831816)},
    {INT64_C(3764908202131456), INT64_C(1218092180833)},
    {INT64_C(6442453091483648), INT64_C(984236433893)},
    {INT64_C(10737419313741824), INT64_C(854799010274)},
};
static const AstroCase s_farLongitudes[] = {
    {INT64_C(-426407952329474048), INT64_C(-864021929577)},
    {INT64_C(-214748366947483648), INT64_C(-1129922495734)},
    {INT64_C(214748366947483648), INT64_C(167014408960)},
    {INT64_C(432585511165493248), INT64_C(497638973219)},
};
// Moment, ephemeris correction (days).
static const AstroCase s_deltas[] = {
    {INT64_C(-426407952329474048), INT64_C(11911279033502)},
    {INT64_C(-858993459200000), INT64_C(890241573)},
    {INT64_C(2147483648), INT64_C(526113610)},
    {INT64_C(644245094400000), INT64_C(327592295)},
    {INT64_C(1717986918400000), INT64_C(54961833)},
    {INT64_C(2509929495658496), INT64_C(5965232)},
    {INT64_C(2663741723456307), INT64_C(440204)},
    {INT64_C(2821793513472000), INT64_C(718565)},
    {INT64_C(2976412336128000), INT64_C(-217337)},
    {INT64_C(3006477107200000), INT64_C(979372)},
    {INT64_C(3113851289600000), INT64_C(2746699)},
    {INT64_C(3135843669639168), INT64_C(3174498)},
    {INT64_C(3156800962560000), INT64_C(3382921)},
    {INT64_C(3199750635520000), INT64_C(4212906)},
    {INT64_C(3307124817920000), INT64_C(13438774)},
    {INT64_C(3435973836800000), INT64_C(20900743)},
    {INT64_C(432585511165493248), INT64_C(11937323511413)},
    // July 1 of each range's last and the next range's first year.
    {INT64_C(3371927284482048), INT64_C(16328829)},
    {INT64_C(3373494947545088), INT64_C(16433977)},
    {INT64_C(3215057898962944), INT64_C(4623105)},
    {INT64_C(3216625562025984), INT64_C(10263799)},
    {INT64_C(3144465816485888), INT64_C(3217281)},
    {INT64_C(3146033479548928), INT64_C(3233863)},
    {INT64_C(3114658743451648), INT64_C(2761545)},
    {INT64_C(3116226406514688), INT64_C(2749795)},
    {INT64_C(2978181862653952), INT64_C(-113179)},
    {INT64_C(2979749525716992), INT64_C(-77015)},
    {INT64_C(2821312477134848), INT64_C(718565)},
    {INT64_C(2822880140197888), INT64_C(677218)},
    {INT64_C(2664443091615744), INT64_C(440204)},
    {INT64_C(2666010754678784), INT64_C(403587)},
    {INT64_C(2507569411129344), INT64_C(5999896)},
    {INT64_C(2509141369159680), INT64_C(5965232)},
    {INT64_C(781997580484608), INT64_C(284336650)},
    {INT64_C(783565243547648), INT64_C(283847860)},
    {INT64_C(-783573833482240), INT64_C(854300010)},
    {INT64_C(-785141496545280), INT64_C(855199571)},
};

// The model's own bits (record mloc-0001: bit-exact on every machine),
// from the 128-bit and the four-product paths alike: lunation and moment.
static const AstroCase s_goldenMoons[] = {
    {-3400000, INT64_C(-431040968784227846)},
    {-37118, INT64_C(-4707734911127656)},
    {0, INT64_C(49215698741)},
    {24724, INT64_C(3135866259099851)},
    {86566, INT64_C(10979464012300791)},
    {3390000, INT64_C(430115583582881420)},
};

// Moment, solar longitude, ephemeris correction, next lunation.
static const int64_t s_goldenMoments[][4] = {
    {INT64_C(-426407952329474048), INT64_C(682166296353), INT64_C(11911279033502), -3363408},
    {INT64_C(3135843669639168), INT64_C(1204176076542), INT64_C(3174498), 24724},
    {INT64_C(2978972136636416), INT64_C(1203250298943), INT64_C(-77015), 23487},
    {INT64_C(2663738716979200), INT64_C(1281118687227), INT64_C(440201), 21002},
    {INT64_C(432585511165493248), INT64_C(497638973449), INT64_C(11937323511413), 3409445},
};

static const int64_t s_turn = INT64_C(360) << 32;
static const int64_t s_day = INT64_C(1) << 32;

static int64_t AngleDifference(int64_t a, int64_t b)
{
    int64_t d = (a - b) % s_turn;
    if (d > s_turn / 2)
    {
        d -= s_turn;
    }
    if (d < -s_turn / 2)
    {
        d += s_turn;
    }
    return d < 0 ? -d : d;
}

static void TestNewMoons(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_newMoons) / sizeof(s_newMoons[0]); i++)
    {
        int64_t got = mlocAstroNewMoon(s_newMoons[i].input);
        if (llabs(got - s_newMoons[i].expected) > 16)
        {
            printf("new moon %lld off by %lld\n", (long long)s_newMoons[i].input,
                   (long long)(got - s_newMoons[i].expected));
            all = false;
        }
    }
    CHECK(all, "new moons as the decimal evaluation gives them");
}

static void TestLongitudes(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_longitudes) / sizeof(s_longitudes[0]); i++)
    {
        int64_t d = AngleDifference(mlocAstroSolarLongitude(s_longitudes[i].input),
                                    s_longitudes[i].expected);
        all = all && d <= 64;
    }
    for (size_t i = 0; i < sizeof(s_farLongitudes) / sizeof(s_farLongitudes[0]); i++)
    {
        int64_t d = AngleDifference(mlocAstroSolarLongitude(s_farLongitudes[i].input),
                                    s_farLongitudes[i].expected);
        all = all && d <= 4096;
    }
    CHECK(all, "solar longitudes, near and at the range's ends");
}

static void TestDeltas(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_deltas) / sizeof(s_deltas[0]); i++)
    {
        all = all && llabs(mlocAstroDeltaT(s_deltas[i].input) - s_deltas[i].expected) <= 4;
    }
    CHECK(all, "the ephemeris correction in each of its ranges");
}

// Lunations across the whole range, the ends and the held factor's
// bounds included.
static bool WellFormed(int64_t n)
{
    int64_t m = mlocAstroNewMoon(n);
    int64_t next = mlocAstroNewMoon(n + 1);
    int64_t length = next - m;
    return length > 29 * s_day + s_day / 10 && length < 29 * s_day + 9 * s_day / 10 &&
           mlocAstroNewMoonIndex(m) == n && mlocAstroNewMoonIndex(m + 1) == n + 1 &&
           mlocAstroNewMoonIndex(m - 1) == n && mlocAstroNewMoonIndex(next) == n + 1;
}

static void TestWellFormed(void)
{
    static const int64_t s_edges[] = {-3400000, -3399999, -37119, -37117, 86565, 86567, 3390000};
    bool all = true;
    for (int64_t n = -3400000; all && n <= 3390000; n += 9973)
    {
        all = WellFormed(n);
    }
    for (size_t i = 0; all && i < sizeof(s_edges) / sizeof(s_edges[0]); i++)
    {
        all = WellFormed(s_edges[i]);
    }
    CHECK(all, "lunations of 29.1 to 29.9 days everywhere, and each found by its moment");
}

static void TestPriorLongitude(void)
{
    static const int64_t s_angles[] = {0, INT64_C(90) << 32, INT64_C(270) << 32};
    bool all = true;
    for (int64_t day = -99000000; all && day <= 100500000; day += 1999993)
    {
        for (size_t i = 0; all && i < 3; i++)
        {
            int64_t moment = day * s_day;
            int64_t r = mlocAstroPriorSolarLongitude(s_angles[i], moment);
            all = r <= moment && moment - r < 367 * s_day &&
                  AngleDifference(mlocAstroSolarLongitude(r), s_angles[i]) < (INT64_C(1) << 32) / 4;
        }
    }
    CHECK(all, "the prior solar longitude's estimate within a quarter of a degree");
}

static void TestGolden(void)
{
    bool all = true;
    for (size_t i = 0; i < sizeof(s_goldenMoons) / sizeof(s_goldenMoons[0]); i++)
    {
        all = all && mlocAstroNewMoon(s_goldenMoons[i].input) == s_goldenMoons[i].expected;
    }
    for (size_t i = 0; i < sizeof(s_goldenMoments) / sizeof(s_goldenMoments[0]); i++)
    {
        const int64_t* g = s_goldenMoments[i];
        all = all && mlocAstroSolarLongitude(g[0]) == g[1] && mlocAstroDeltaT(g[0]) == g[2] &&
              mlocAstroNewMoonIndex(g[0]) == g[3];
    }
    CHECK(all, "the model's bits unchanged");
}

int main(void)
{
    TestGolden();
    TestNewMoons();
    TestLongitudes();
    TestDeltas();
    TestWellFormed();
    TestPriorLongitude();
    return s_failures == 0 ? 0 : 1;
}
