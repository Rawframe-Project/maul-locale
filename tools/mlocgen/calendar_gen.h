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

// The same from supplementalData.xml's parsed root.
int CalendarErasSource(const XmlNode* supplemental, char** sourceOut, size_t* sourceSizeOut,
                       char* errorOut, size_t errorCapacity);

#endif
