// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Semantic skeletons mapped to classical ones (UTS #35, "Mapping to
// Standard Skeletons"), which skeleton.c then matches to a pattern: the
// year, month and day letters come from the length's datetimeSkeleton, a
// lone month and the weekday from the mapping table, the hour from the
// hour cycle, the minutes and seconds from the time precision, the zone
// from its style.

#ifndef MLOCGEN_SEMANTIC_H
#define MLOCGEN_SEMANTIC_H

#include <stddef.h>

typedef enum SemanticField
{
    semanticYear = 1,
    semanticMonth = 2,
    semanticDay = 4,
    semanticWeekday = 8,
    semanticTime = 16,
    semanticZone = 32,
} SemanticField;

typedef enum SemanticLength
{
    semanticLong,
    semanticMedium,
    semanticShort,
} SemanticLength;

typedef enum SemanticYearStyle
{
    // The datetimeSkeleton's year, two digits where it has them.
    semanticYearAuto,
    semanticYearFull,
    semanticYearWithEra,
} SemanticYearStyle;

typedef enum SemanticHourCycle
{
    semanticHourAuto,
    semanticHourH11,
    semanticHourH12,
    semanticHourH23,
    semanticHourH24,
    semanticHourClock12,
    semanticHourClock24,
} SemanticHourCycle;

typedef enum SemanticZoneStyle
{
    semanticZoneSpecific,
    semanticZoneGeneric,
    semanticZoneLocation,
    semanticZoneOffset,
} SemanticZoneStyle;

typedef struct SemanticSkeleton
{
    // SemanticField bits.
    unsigned fields;
    SemanticLength length;
    SemanticYearStyle yearStyle;
    SemanticHourCycle hourCycle;
    // 0 the hour, 1 minutes, 2 seconds, 3 to 11 seconds and 1 to 9
    // fractional digits.
    int precision;
    SemanticZoneStyle zoneStyle;
} SemanticSkeleton;

// Writes the classical skeleton of a semantic one into out (capacity
// bytes, at least 64), the year, month and day letters taken from the
// length's datetimeSkeleton ("yyMd", "GGGGGyMd"). Returns 0 for a field
// set UTS #35 does not define.
int SemanticClassical(const SemanticSkeleton* s, const char* datetimeSkeleton, char* out,
                      size_t capacity);

#endif
