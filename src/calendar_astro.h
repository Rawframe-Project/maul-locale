// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The astronomy of the Chinese and Korean calendars: new moons and the
// sun's longitude after Reingold and Dershowitz's Calendrical Calculations
// (Meeus's new moon terms, Bretagnon and Simon's solar terms, NASA's
// ephemeris correction), in integer fixed point so every machine gives the
// same bits.
//
// A moment is days from the start of day 0 of maul-locale/calendar.h
// (day 1 begins at 1), in universal time, scaled by 2^32. An angle is
// degrees scaled by 2^32. The functions are defined for moments within
// a few days of the dates' range; the model's terms are fitted to
// recent millennia and are only consistent, not true, far from them.
// Past 50 centuries from 2000 (the years -3000 and 7000) two fits are
// held at their values there: the ephemeris correction, and the new
// moon's eccentricity factor.

#ifndef MLOC_CALENDAR_ASTRO_H
#define MLOC_CALENDAR_ASTRO_H

#include <stdint.h>

enum
{
    astroFractionBits = 32,
};

// The correction from universal to dynamical time at a moment, in days
// scaled by 2^32.
int64_t mlocAstroDeltaT(int64_t moment);

// The moment of the nth new moon after the one of January 11 of year 1
// (n 0), before it for n negative.
int64_t mlocAstroNewMoon(int64_t n);

// The number of the first new moon at or after a moment.
int64_t mlocAstroNewMoonIndex(int64_t moment);

// The sun's apparent longitude at a moment, in [0, 360) degrees.
int64_t mlocAstroSolarLongitude(int64_t moment);

// A moment at or before the given one, close to the last at which the
// sun's longitude was the given angle (Calendrical Calculations'
// estimate-prior-solar-longitude).
int64_t mlocAstroPriorSolarLongitude(int64_t degrees, int64_t moment);

#endif
