// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date names unit (src/date_tables.h): each locale's months, weekdays,
// quarters, eras, cyclic years, leap month patterns and day periods in a
// calendar, resolved by CLDR's inheritance, kept once each as names, lists
// and name sets, and its day period rules.

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
    Map rules;
    uint8_t* ruleData;
    int ruleCount;
    int ruleCapacity;
} DateNames;

// The name set of a locale's calendar (MlocCalendar's value and CLDR's
// type), added when new; the generator stops on an era CLDR's
// calendarData does not list as src/date_tables.h does.
uint16_t DateNamesSet(DateNames* names, Cldr* cldr, const DateSupplement* supplement,
                      const char* locale, int calendar, const char* type);

// The index of a locale's day period rules, the first found cutting its
// id's last subtag (root's at the end), added when new.
uint16_t DateNamesRules(DateNames* names, const DateSupplement* supplement, const char* locale);

// Adds the names unit to a file and frees the builder.
void DateNamesFinish(DateNames* names, DataFile* file);

#endif
