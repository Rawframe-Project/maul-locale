// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Japan's eras since Meiji, from CLDR's supplementalData.xml, which
// src/generated/calendar_eras.c defines.

#ifndef MLOC_CALENDAR_ERAS_H
#define MLOC_CALENDAR_ERAS_H

#include <stdint.h>

// An era: its code, the Gregorian year it began in (its year 1) and its
// first day (a day of maul-locale/calendar.h).
typedef struct JapaneseEra
{
    char code[8];
    int32_t year;
    int32_t day;
} JapaneseEra;

// In the order they began.
extern const JapaneseEra mlocJapaneseEras[];
extern const uint32_t mlocJapaneseEraCount;

#endif
