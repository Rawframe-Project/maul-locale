// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A calendar's time patterns in a locale (src/date_tables.h's time set):
// the time field set matched at the hour, minute and second on the
// 12-hour and 24-hour clocks, the standard time formats, the date-time
// glue and the clocks' minutes and seconds for a standard format, as
// CLDR resolves them under the calendar (record mloc-0042).

#ifndef MLOCGEN_TIME_GEN_H
#define MLOCGEN_TIME_GEN_H

#include "cldr.h"
#include "date_tables.h"
#include "skeleton.h"

// Writes the patterns of each DateTimeEntry into out, allocated, from a
// generator loaded for the locale and calendar (date_source.h), and
// whether the locale's own hour is on the 12-hour clock. The generator
// stops on a missing standard time format or glue, or glue with letters
// outside quotes or without exactly one {0} and one {1}.
// items is a generator loaded without the standard formats
// (DateSourceLoadItems), for the standard formats' clocks.
void TimeSetPatterns(Cldr* cldr, PatternGenerator* g, PatternGenerator* items, const char* locale,
                     const char* calendar, char* out[dateTimePatternCount], bool* ownTwelve);

#endif
