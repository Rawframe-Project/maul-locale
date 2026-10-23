// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Date skeletons matched to patterns (UTS #35, "Matching Skeletons" and
// "Missing Skeleton Fields"), as ICU's DateTimePatternGenerator does it: a
// locale's standard formats and availableFormats in a map, the best entry
// by a distance between fields, its field lengths adjusted to the request,
// missing fields appended or a date and a time glued by the request's
// month width. The generator uses it to compile each semantic skeleton's
// pattern; patterns are UTF-8.

#ifndef MLOCGEN_SKELETON_H
#define MLOCGEN_SKELETON_H

#include <stddef.h>
#include <stdint.h>

enum
{
    // UTS #35's fields in ICU's order: era, year, quarter, month, week of
    // year, week of month, weekday, day of year, weekday in month, day,
    // day period, hour, minute, second, fractional second, zone.
    skeletonFieldCount = 16,
    skeletonDayPeriod = 10,
    skeletonHour = 11,
    skeletonMinute = 12,
    skeletonSecond = 13,
    skeletonFraction = 14,
    skeletonZone = 15,
};

// The fields of a skeleton or pattern: each field's letter and length.
typedef struct SkeletonFields
{
    char chars[skeletonFieldCount];
    uint8_t lengths[skeletonFieldCount];
} SkeletonFields;

typedef struct Skeleton
{
    // The fields as given, and as their base letters at their least
    // lengths.
    SkeletonFields original;
    SkeletonFields base;
    // Each field's distance type: 0 for none.
    int16_t types[skeletonFieldCount];
} Skeleton;

typedef struct PatternEntry
{
    Skeleton skeleton;
    char* pattern;
    // Whether the entry came with its skeleton (availableFormats).
    bool specified;
    // The next entry of the same first base letter, or -1.
    int next;
} PatternEntry;

typedef struct PatternGenerator
{
    PatternEntry* entries;
    int count;
    int capacity;
    // The first entry of each base letter, A to Z then a to z, or -1.
    int heads[52];
    // The availableFormats skeletons already added, a locale before its
    // parents.
    char** keys;
    int keyCount;
    // The locale's hour letter (h, H, K or k) and its preferred hour
    // format's letters for C (an hour letter, and a, b or B or none).
    char hourChar;
    char preferredHour;
    char preferredPeriod;
    // appendItems by field, with the field names they name.
    char* appendItems[skeletonFieldCount];
    char* fieldNames[skeletonFieldCount];
    // The glue patterns of the full, long, medium and short lengths.
    char* glue[4];
    char* decimal;
} PatternGenerator;

// An empty generator with the canonical single fields added.
void PatternGeneratorInit(PatternGenerator* g);
void PatternGeneratorFree(PatternGenerator* g);

// Adds a pattern: a standard format (skeleton null, its own skeleton
// derived; it never replaces an entry), or an availableFormats entry
// with its skeleton, which replaces an entry derived from a standard
// format but not one an earlier availableFormats entry added. Entries
// are added a locale first, then its parents.
void PatternGeneratorAdd(PatternGenerator* g, const char* pattern, const char* skeleton);

// The best pattern for a skeleton, from malloc; jJC are the locale's
// hour letters.
char* PatternGeneratorBest(PatternGenerator* g, const char* skeleton);

// The fields of a skeleton or pattern.
void SkeletonParse(const char* text, Skeleton* out);

#endif
