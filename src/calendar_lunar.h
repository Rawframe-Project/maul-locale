// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The Chinese and Dangi years the dates use (record mloc-0040): the
// generated tables from 1600 to 2400, the model outside them.

#ifndef MLOC_CALENDAR_LUNAR_H
#define MLOC_CALENDAR_LUNAR_H

#include "calendar_chinese.h"

// The year holding a day; korea selects Dangi.
void mlocLunarYearOf(bool korea, int32_t day, LunisolarYear* out);

// The year numbered year, for a year within the dates' range and one more
// each side.
void mlocLunarYear(bool korea, int64_t year, LunisolarYear* out);

#endif
