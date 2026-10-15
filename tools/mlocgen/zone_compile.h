// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// A zone's rules compiled as zic compiles them (record mloc-0041): the
// transitions up to the last year the source names, then a closing rule
// of two yearly changes, or none when the last offset stays.

#ifndef MLOCGEN_ZONE_COMPILE_H
#define MLOCGEN_ZONE_COMPILE_H

#include "tzdb.h"

#include <stddef.h>
#include <stdint.h>

// An offset as a standard offset and a saving, in seconds. Compiled from
// the source's rearguard form (tzdb.h), the saving is never negative: a
// zone line that still gives a negative one (Morocco's, which tzdb
// rewrites by script) has its standard lowered and its savings raised by
// the most negative, which keeps each offset.
typedef struct CompiledOffset
{
    int32_t standard;
    int32_t save;
} CompiledOffset;

typedef struct CompiledTransition
{
    // Seconds since 1970 in universal time.
    int64_t at;
    CompiledOffset offset;
} CompiledTransition;

// One yearly change of a closing rule: the day of the year as tzdb names
// it, a time on it in universal time (seconds, which may pass the day),
// and the offset it starts.
typedef struct CompiledChange
{
    uint8_t month;
    uint8_t dayOfMonth;
    uint8_t weekday;
    TzdbDay day;
    int32_t time;
    CompiledOffset offset;
} CompiledChange;

typedef struct CompiledZone
{
    // The offset before the first transition.
    CompiledOffset initial;
    CompiledTransition* transitions;
    int transitionCount;
    // With a closing rule, its changes apply each year from firstYear on,
    // in this order within a year; without, the last offset stays.
    bool closing;
    int32_t firstYear;
    CompiledChange changes[2];
} CompiledZone;

// Compiles a zone of a read source. Returns 0 and a message on failure:
// two rules at one instant, a rule on Feb 29 of a common year, a closing
// rule set of other than two rules in two months, or a closing rule that
// does not give the two years after the table as the source does.
int ZoneCompile(const Tzdb* tzdb, int zone, CompiledZone* out, char* errorOut,
                size_t errorCapacity);

// The universal times of a closing rule's changes in a year, in order.
void ZoneClosingTimes(const CompiledZone* zone, int32_t year, int64_t timesOut[2]);

void ZoneFree(CompiledZone* zone);

#endif
