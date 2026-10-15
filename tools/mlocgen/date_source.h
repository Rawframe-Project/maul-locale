// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A locale's date patterns read from CLDR into a pattern generator
// (skeleton.h) as ICU's DateTimePatternGenerator reads them: the
// standard time and date formats, then availableFormats from the locale
// up its parents and through root's calendar aliases, the atTime glue,
// appendItems, the field names, the decimal separator and the hour
// letters of the locale's region (supplementalData's timeData).

#ifndef MLOCGEN_DATE_SOURCE_H
#define MLOCGEN_DATE_SOURCE_H

#include "cldr.h"
#include "map.h"
#include "skeleton.h"

#include <stddef.h>

// timeData and likelySubtags, read once.
typedef struct DateSupplement
{
    // A region or locale to its hours: "preferred allowed allowed...".
    Map hours;
    // A language, language_Script or und_Script to its likely tag.
    Map likely;
} DateSupplement;

int DateSupplementLoad(DateSupplement* s, const char* commonDirectory, char* errorOut,
                       size_t errorCapacity);
void DateSupplementFree(DateSupplement* s);

// Fills an initialized generator for a CLDR locale id ("en_US",
// "zh_Hant") and calendar type ("gregorian").
void DateSourceLoad(Cldr* cldr, const DateSupplement* s, const char* locale, const char* calendar,
                    PatternGenerator* g);

// Prints the pattern of each line of standard input, "locale<TAB>calendar
// <TAB>skeleton", for tools/check_skeletons.py.
int PrintSkeletons(const char* commonDirectory, char* errorOut, size_t errorCapacity);

// The datetimeSkeleton of a date length (0 long, 1 medium, 2 short).
const char* DateSkeletonOf(Cldr* cldr, const char* locale, const char* calendar, int length);

// Prints the classical skeleton and the pattern of each semantic skeleton
// on standard input, for tools/check_datetime.py.
int PrintSemantic(const char* commonDirectory, char* errorOut, size_t errorCapacity);

#endif
