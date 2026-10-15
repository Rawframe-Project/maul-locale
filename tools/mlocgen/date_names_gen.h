// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date names unit (src/date_tables.h): each locale's months,
// weekdays, quarters, eras, cyclic years and leap month patterns in a
// calendar, resolved by CLDR's inheritance, kept once each as names,
// lists and name sets (record mloc-0042).

#ifndef MLOCGEN_DATE_NAMES_GEN_H
#define MLOCGEN_DATE_NAMES_GEN_H

#include "cldr.h"
#include "datafile.h"
#include "date_source.h"
#include "map.h"

#include <stdint.h>

typedef struct DateNames
{
    Map strings;
    char** texts;
    int stringCount;
    int stringCapacity;
    Map lists;
    uint16_t* items;
    uint32_t itemCount;
    uint32_t itemCapacity;
    uint32_t* listStarts;
    int listCount;
    int listCapacity;
    Map sets;
    uint16_t* setData;
    int setCount;
    int setCapacity;
} DateNames;

// The name set of a locale's calendar (MlocCalendar's value and CLDR's
// type), added when new; the generator stops on an era CLDR's
// calendarData does not list as src/date_tables.h does.
uint16_t DateNamesSet(DateNames* names, Cldr* cldr, const DateSupplement* supplement,
                      const char* locale, int calendar, const char* type);

// Adds the names unit to a file and frees the builder.
void DateNamesFinish(DateNames* names, DataFile* file);

#endif
