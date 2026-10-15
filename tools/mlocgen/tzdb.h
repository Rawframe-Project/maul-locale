// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The time zone database's source (record mloc-0041), read as zic reads
// it: Rule, Zone and Link lines from the default build's files. Only the
// forms the pinned release uses are taken; anything else stops the
// generator with a message, so a new form is met before it is compiled
// wrongly.
//
// The source marks some lines as a vanguard section (negative savings,
// %z) with a commented-out rearguard alternative. Read in rearguard form,
// the vanguard lines are skipped and the alternative's data lines (a
// "#" straight before Rule, Zone, Link or a blank) are read instead.

#ifndef MLOCGEN_TZDB_H
#define MLOCGEN_TZDB_H

#include <stddef.h>
#include <stdint.h>

enum
{
    tzdbMaxName = 64,
};

// What a time of day is measured against.
typedef enum TzdbBase
{
    tzdbWall,
    tzdbStandard,
    tzdbUniversal,
} TzdbBase;

// How a day in a month is named.
typedef enum TzdbDay
{
    tzdbDayOfMonth,
    // The last given weekday on or before the day of the month (lastSun
    // is the last Sunday on or before the month's last day).
    tzdbWeekdayOnOrBefore,
    tzdbWeekdayOnOrAfter,
} TzdbDay;

// A day of a year and a time on it: a Rule's IN, ON and AT, or a Zone
// line's UNTIL after its year. Months are 0 to 11, weekdays 0 (Sunday)
// to 6, times seconds.
typedef struct TzdbDate
{
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t weekday;
    TzdbDay day;
    int32_t time;
    TzdbBase base;
} TzdbDate;

typedef struct TzdbRule
{
    char name[tzdbMaxName];
    int32_t from;
    int32_t to;
    // TO is "max": the rule applies every year from FROM on.
    bool toMax;
    TzdbDate at;
    int32_t save;
    // The rule's place in the source, which orders a set's rules.
    int sourceIndex;
} TzdbRule;

typedef struct TzdbLine
{
    int32_t standard;
    // The line's rules, a range of Tzdb.rules; none when ruleCount is 0,
    // and then save is the line's fixed saving.
    int ruleFirst;
    int ruleCount;
    int32_t save;
    bool hasUntil;
    int32_t untilYear;
    TzdbDate until;
} TzdbLine;

typedef struct TzdbZone
{
    char name[tzdbMaxName];
    int lineFirst;
    int lineCount;
} TzdbZone;

typedef struct TzdbLink
{
    char target[tzdbMaxName];
    char name[tzdbMaxName];
} TzdbLink;

typedef struct Tzdb
{
    // Sorted by name, so each rule set is one range.
    TzdbRule* rules;
    int ruleCount;
    TzdbLine* lines;
    int lineCount;
    // Sorted by name.
    TzdbZone* zones;
    int zoneCount;
    TzdbLink* links;
    int linkCount;
} Tzdb;

// Reads the default build's files (the seven regions, etcetera and
// backward) from a tzdb source directory. Returns 0 and a message on
// failure: a file missing, a form not taken, a zone line naming no rule
// set, or a zone or link name twice.
int TzdbRead(const char* directory, bool rearguard, Tzdb* out, char* errorOut,
             size_t errorCapacity);

// The same from one text, for the tests.
int TzdbParse(const char* text, bool rearguard, Tzdb* out, char* errorOut, size_t errorCapacity);

void TzdbFree(Tzdb* tzdb);

#endif
