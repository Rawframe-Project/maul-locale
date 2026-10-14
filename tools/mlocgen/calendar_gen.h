// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Japan's eras (record mloc-0040) from supplemental/supplementalData.xml's
// calendarData, as the source of src/generated/calendar_eras.c.

#ifndef MLOCGEN_CALENDAR_GEN_H
#define MLOCGEN_CALENDAR_GEN_H

#include "xml.h"

#include <stddef.h>

// The source of src/generated/calendar_eras.c in *sourceOut, which the
// caller frees. Returns 0 and a message on failure: no eras, an era out
// of order, a malformed start date or code, or a code Japan's Gregorian
// eras already have.
int GenerateCalendarEras(const char* commonDirectory, char** sourceOut, size_t* sourceSizeOut,
                         char* errorOut, size_t errorCapacity);

// The source of src/generated/calendar_years.c: the Chinese and Dangi
// years from 1600 to 2400 by the model, with the years of
// root/tools/calendar-overrides.txt in their place. Returns 0 and a
// message on failure: a malformed override, one for a year twice, or a
// year that does not meet its neighbours.
int GenerateCalendarYears(const char* root, char** sourceOut, size_t* sourceSizeOut, char* errorOut,
                          size_t errorCapacity);

// The same years from the overrides' text, for the tests.
int CalendarYearsSource(const char* overrides, char** sourceOut, size_t* sourceSizeOut,
                        char* errorOut, size_t errorCapacity);

// The same from supplementalData.xml's parsed root.
int CalendarErasSource(const XmlNode* supplemental, char** sourceOut, size_t* sourceSizeOut,
                       char* errorOut, size_t errorCapacity);

#endif
