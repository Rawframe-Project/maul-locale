// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Hebrew calendar: the molad and the postponements of Calendrical
// Calculations, months in civil order from Tishrei, Adar I the leap month
// (code 'M05L'). Days are those of maul-locale/calendar.h; years are Anno
// Mundi.

#ifndef MLOC_CALENDAR_HEBREW_H
#define MLOC_CALENDAR_HEBREW_H

#include "calendar_arith.h"

#include <stdint.h>

CalendarDate mlocHebrewFromDay(int32_t day);

// The day of a valid year, month (its place in the year) and day.
int64_t mlocHebrewToDay(int64_t year, uint32_t month, uint32_t day);

bool mlocHebrewLeap(int64_t year);
uint32_t mlocHebrewMonths(int64_t year);
uint32_t mlocHebrewDaysInMonth(int64_t year, uint32_t month);

#endif
