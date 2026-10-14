// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Umm al-Qura (record mloc-0040): KACST's month lengths for 1300 to 1600
// AH, the tabular civil calendar outside them, which meets the table's
// first and last days exactly. Days are those of maul-locale/calendar.h;
// years are counted on across eras.

#ifndef MLOC_CALENDAR_UMALQURA_H
#define MLOC_CALENDAR_UMALQURA_H

#include "calendar_arith.h"

#include <stdint.h>

CalendarDate mlocUmalquraFromDay(int32_t day);

// The day of a valid year, month and day.
int64_t mlocUmalquraToDay(int64_t year, uint32_t month, uint32_t day);

// A leap year has 355 days.
bool mlocUmalquraLeap(int64_t year);
uint32_t mlocUmalquraDaysInMonth(int64_t year, uint32_t month);

#endif
