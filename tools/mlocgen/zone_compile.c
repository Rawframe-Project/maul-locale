// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Zones compiled as zic's outzone and writezone compile them
// (zone_compile.h): each zone line walks its rules year by year, a line's
// start takes the offset of the last rule before it, and transitions
// whose local time does not move forward are merged.

#include "zone_compile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    secondsPerDay = 86400,
    // Thursday, the weekday of January 1 of 1970, Sunday being 0.
    epochWeekday = 4,
    // The most rules a set may have; the largest has 110.
    maxRuleSet = 1024,
};

// A transition while compiling: the source's offset and the line it
// comes from.
typedef struct Raw
{
    int64_t at;
    int32_t standard;
    int32_t save;
    int line;
    int order;
} Raw;

typedef struct Builder
{
    Raw* items;
    int count;
    int capacity;
    char* errorOut;
    size_t errorCapacity;
    const char* zone;
} Builder;

static int Fail(Builder* b, const char* what)
{
    snprintf(b->errorOut, b->errorCapacity, "zone %s: %s", b->zone, what);
    return 0;
}

static void Add(Builder* b, int64_t at, int32_t standard, int32_t save, int line)
{
    if (b->count == b->capacity)
    {
        b->capacity = b->capacity == 0 ? 64 : b->capacity * 2;
        Raw* grown = realloc(b->items, (size_t)b->capacity * sizeof(Raw));
        if (grown == nullptr)
        {
            fputs("mlocgen: out of memory compiling zones\n", stderr);
            exit(1);
        }
        b->items = grown;
    }
    b->items[b->count] = (Raw){at, standard, save, line, b->count};
    b->count++;
}

static bool IsLeap(int64_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static int64_t FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b != 0 && (a < 0) != (b < 0));
}

// Days from January 1 of 1970 to a proleptic Gregorian date, month 0
// to 11.
static int64_t DaysFromCivil(int64_t year, int month, int day)
{
    int64_t y = year - (month < 2);
    int64_t era = FloorDiv(y, 400);
    int64_t yearOfEra = y - era * 400;
    int64_t shifted = (month + 10) % 12;
    int64_t dayOfYear = (153 * shifted + 2) / 5 + day - 1;
    int64_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return era * 146097 + dayOfEra - 719468;
}

// The day a date names in a year, as zic's rpytime finds it: a weekday
// rule may step into the next or previous month. Feb 29 in a common year
// is the 28th for an on-or-before rule and refused otherwise.
static bool RuleDay(const TzdbDate* date, int64_t year, int64_t* dayOut)
{
    int dayOfMonth = date->dayOfMonth;
    if (date->month == 1 && dayOfMonth == 29 && !IsLeap(year))
    {
        if (date->day != tzdbWeekdayOnOrBefore)
        {
            return false;
        }
        dayOfMonth = 28;
    }
    int64_t day = DaysFromCivil(year, date->month, dayOfMonth);
    if (date->day != tzdbDayOfMonth)
    {
        int64_t weekday = ((day + epochWeekday) % 7 + 7) % 7;
        int64_t step = (date->weekday - weekday + 7) % 7;
        day += date->day == tzdbWeekdayOnOrAfter ? step : (step == 0 ? 0 : step - 7);
    }
    *dayOut = day;
    return true;
}

// A date's time in universal time, given the offsets in effect.
static bool DateTime(const TzdbDate* date, int64_t year, int32_t standard, int32_t save,
                     int64_t* out)
{
    int64_t day = 0;
    if (!RuleDay(date, year, &day))
    {
        return false;
    }
    int64_t offset = date->base == tzdbUniversal  ? 0
                     : date->base == tzdbStandard ? standard
                                                  : (int64_t)standard + save;
    *out = day * secondsPerDay + date->time - offset;
    return true;
}

// The walk's state across one zone line's years.
typedef struct Walk
{
    const TzdbLine* line;
    int lineIndex;
    bool useStart;
    bool useUntil;
    int64_t start;
    int32_t startOffset;
    int32_t save;
} Walk;

// The rule of those still to do this year that comes first, or -1.
static int Earliest(Builder* b, const Tzdb* t, Walk* w, const bool* todo, int64_t* timeOut,
                    int32_t year, bool* failed)
{
    int found = -1;
    for (int j = 0; j < w->line->ruleCount; j++)
    {
        const TzdbRule* rule = &t->rules[w->line->ruleFirst + j];
        int64_t time = 0;
        if (!todo[j])
        {
            continue;
        }
        if (!DateTime(&rule->at, year, w->line->standard, w->save, &time))
        {
            *failed = !Fail(b, "a rule on Feb 29 of a common year");
            return -1;
        }
        if (found >= 0 && time == *timeOut)
        {
            *failed = !Fail(b, "two rules at one instant");
            return -1;
        }
        if (found < 0 || time < *timeOut)
        {
            found = j;
            *timeOut = time;
        }
    }
    return found;
}

// One year of a line's rules, in time order (outzone's inner loop).
static int WalkYear(Builder* b, const Tzdb* t, Walk* w, int32_t year)
{
    bool todo[maxRuleSet] = {0};
    if (w->line->ruleCount > maxRuleSet)
    {
        return Fail(b, "a rule set too large");
    }
    for (int j = 0; j < w->line->ruleCount; j++)
    {
        const TzdbRule* rule = &t->rules[w->line->ruleFirst + j];
        todo[j] = year >= rule->from && (rule->toMax || year <= rule->to);
    }
    for (;;)
    {
        int64_t until = 0;
        if (w->useUntil &&
            !DateTime(&w->line->until, w->line->untilYear, w->line->standard, w->save, &until))
        {
            return Fail(b, "an UNTIL on Feb 29 of a common year");
        }
        int64_t time = 0;
        bool failed = false;
        int k = Earliest(b, t, w, todo, &time, year, &failed);
        if (failed)
        {
            return 0;
        }
        if (k < 0)
        {
            return 1;
        }
        todo[k] = false;
        if (w->useUntil && time >= until)
        {
            return 1;
        }
        w->save = t->rules[w->line->ruleFirst + k].save;
        w->useStart = w->useStart && time != w->start;
        if (w->useStart && time < w->start)
        {
            w->startOffset = w->line->standard + w->save;
            continue;
        }
        Add(b, time, w->line->standard, w->save, w->lineIndex);
    }
}

// The first year a line's rules name.
static int32_t FirstRuleYear(const Tzdb* t, const TzdbLine* line)
{
    int32_t first = INT32_MAX;
    for (int j = 0; j < line->ruleCount; j++)
    {
        int32_t from = t->rules[line->ruleFirst + j].from;
        first = from < first ? from : first;
    }
    return first;
}

// The offset before a zone's first transition when its first line has
// rules: zic's default type, the first standard time the walk met, else
// the first offset, else the line's standard time.
static CompiledOffset InitialOffset(const Builder* b, int32_t standard)
{
    for (int i = 0; i < b->count; i++)
    {
        if (b->items[i].save == 0)
        {
            return (CompiledOffset){b->items[i].standard, 0};
        }
    }
    return b->count > 0 ? (CompiledOffset){b->items[0].standard, b->items[0].save}
                        : (CompiledOffset){standard, 0};
}

// Every line of a zone up to the end of lastYear for the last line's
// rules, as outzone walks them.
static int WalkLines(Builder* b, const Tzdb* t, const TzdbZone* z, int32_t lastYear,
                     CompiledOffset* initial)
{
    int64_t start = 0;
    for (int i = 0; i < z->lineCount; i++)
    {
        const TzdbLine* line = &t->lines[z->lineFirst + i];
        Walk w = {.line = line,
                  .lineIndex = i,
                  .useStart = i > 0,
                  .useUntil = i < z->lineCount - 1,
                  .start = start,
                  .startOffset = line->standard};
        if (line->ruleCount == 0)
        {
            w.save = line->save;
            if (i == 0)
            {
                *initial = (CompiledOffset){line->standard, line->save};
            }
            else
            {
                Add(b, start, line->standard, line->save, i);
            }
            w.useStart = false;
        }
        else
        {
            int32_t last = w.useUntil ? line->untilYear : lastYear;
            for (int32_t year = FirstRuleYear(t, line); year <= last; year++)
            {
                if (!WalkYear(b, t, &w, year))
                {
                    return 0;
                }
            }
        }
        if (w.useStart)
        {
            Add(b, start, line->standard, w.startOffset - line->standard, i);
        }
        if (i == 0 && line->ruleCount > 0)
        {
            *initial = InitialOffset(b, line->standard);
        }
        if (w.useUntil && !DateTime(&line->until, line->untilYear, line->standard, w.save, &start))
        {
            return Fail(b, "an UNTIL on Feb 29 of a common year");
        }
    }
    return 1;
}

static int CompareRaw(const void* a, const void* b)
{
    const Raw* x = a;
    const Raw* y = b;
    if (x->at != y->at)
    {
        return x->at < y->at ? -1 : 1;
    }
    return (x->order > y->order) - (x->order < y->order);
}

static bool SameOffset(const Raw* x, const Raw* y)
{
    return x->standard == y->standard && x->save == y->save;
}

static int64_t Utoff(const Raw* r)
{
    return (int64_t)r->standard + r->save;
}

// Drops the transitions that change nothing.
static void DropUnchanged(Builder* b, CompiledOffset initial)
{
    Raw first = {.standard = initial.standard, .save = initial.save};
    int out = 0;
    for (int i = 0; i < b->count; i++)
    {
        const Raw* before = out == 0 ? &first : &b->items[out - 1];
        if (!SameOffset(&b->items[i], before))
        {
            b->items[out++] = b->items[i];
        }
    }
    b->count = out;
}

// writezone's merge, in time order: a transition whose local time is not
// after the previous one's takes that one's place (or undoes it when it
// returns to the offset before), and one that changes nothing is skipped.
// The first is kept as writezone keeps it, then dropped here if it
// changes nothing.
static void Merge(Builder* b, CompiledOffset initial)
{
    qsort(b->items, (size_t)b->count, sizeof(Raw), CompareRaw);
    Raw first = {.standard = initial.standard, .save = initial.save};
    int kept = 0;
    for (int from = 0; from < b->count; from++)
    {
        Raw* next = &b->items[from];
        if (kept > 0)
        {
            Raw* previous = &b->items[kept - 1];
            const Raw* before = kept == 1 ? &first : &b->items[kept - 2];
            if (next->at + Utoff(previous) <= previous->at + Utoff(before))
            {
                if (SameOffset(next, before))
                {
                    kept--;
                }
                else
                {
                    previous->standard = next->standard;
                    previous->save = next->save;
                    previous->line = next->line;
                }
                continue;
            }
        }
        if (kept == 0 || !SameOffset(next, &b->items[kept - 1]))
        {
            b->items[kept++] = *next;
        }
    }
    b->count = kept;
    DropUnchanged(b, initial);
}

// The most negative saving a line gives, or 0.
static int32_t LineShift(const Builder* b, CompiledOffset initial, int line)
{
    int32_t shift = line == 0 && initial.save < 0 ? initial.save : 0;
    for (int i = 0; i < b->count; i++)
    {
        if (b->items[i].line == line && b->items[i].save < shift)
        {
            shift = b->items[i].save;
        }
    }
    return shift;
}

// Lowers the standard offset of each line with a negative saving and
// raises its savings by the same amount.
static void Shift(Builder* b, CompiledOffset* initial, int lineCount, int32_t* lastShiftOut)
{
    for (int line = 0; line < lineCount; line++)
    {
        int32_t shift = LineShift(b, *initial, line);
        for (int i = 0; i < b->count; i++)
        {
            if (b->items[i].line == line)
            {
                b->items[i].standard += shift;
                b->items[i].save -= shift;
            }
        }
        if (line == 0)
        {
            initial->standard += shift;
            initial->save -= shift;
        }
        *lastShiftOut = shift;
    }
}

// The last line's yearly rules (TO "max"): none, or two in different
// months, in month order.
static int ClosingRules(Builder* b, const Tzdb* t, const TzdbLine* line, int found[2], int* count)
{
    *count = 0;
    for (int j = 0; j < line->ruleCount; j++)
    {
        if (t->rules[line->ruleFirst + j].toMax)
        {
            if (*count == 2)
            {
                return Fail(b, "more than two yearly rules");
            }
            found[(*count)++] = line->ruleFirst + j;
        }
    }
    if (*count == 1)
    {
        return Fail(b, "one yearly rule");
    }
    if (*count == 2 && t->rules[found[0]].at.month == t->rules[found[1]].at.month)
    {
        return Fail(b, "two yearly rules in one month");
    }
    if (*count == 2 && t->rules[found[0]].at.month > t->rules[found[1]].at.month)
    {
        int swap = found[0];
        found[0] = found[1];
        found[1] = swap;
    }
    return 1;
}

// The last year the last line's rules name otherwise, and no earlier
// than the line's start.
static int32_t LastNamedYear(const Tzdb* t, const TzdbZone* z)
{
    const TzdbLine* line = &t->lines[z->lineFirst + z->lineCount - 1];
    int32_t last = z->lineCount > 1 ? t->lines[z->lineFirst + z->lineCount - 2].untilYear : 0;
    for (int j = 0; j < line->ruleCount; j++)
    {
        const TzdbRule* rule = &t->rules[line->ruleFirst + j];
        last = rule->from > last ? rule->from : last;
        last = !rule->toMax && rule->to > last ? rule->to : last;
    }
    return last;
}

// A closing change from its rule: its time made universal with the
// line's standard offset and, for a wall time, the other rule's saving.
static CompiledChange Change(const Tzdb* t, const TzdbLine* line, int rule, int other,
                             int32_t shift)
{
    const TzdbRule* r = &t->rules[rule];
    int32_t offset = r->at.base == tzdbUniversal  ? 0
                     : r->at.base == tzdbStandard ? line->standard
                                                  : line->standard + t->rules[other].save;
    return (CompiledChange){.month = r->at.month,
                            .dayOfMonth = r->at.dayOfMonth,
                            .weekday = r->at.weekday,
                            .day = r->at.day,
                            .time = r->at.time - offset,
                            .offset = {line->standard + shift, r->save - shift}};
}

void ZoneClosingTimes(const CompiledZone* zone, int32_t year, int64_t timesOut[2])
{
    for (int i = 0; i < 2; i++)
    {
        const CompiledChange* c = &zone->changes[i];
        TzdbDate date = {.month = c->month,
                         .dayOfMonth = c->dayOfMonth,
                         .weekday = c->weekday,
                         .day = c->day,
                         .base = tzdbUniversal};
        int64_t day = 0;
        // A closing change on Feb 29 is refused when compiled, so the day
        // is always found.
        (void)RuleDay(&date, year, &day);
        timesOut[i] = day * secondsPerDay + c->time;
    }
}

// The two years after the table, compiled, must be the closing rule's.
static int CheckClosing(Builder* b, const CompiledZone* zone)
{
    if (b->count < 4)
    {
        return Fail(b, "the years after the table have too few transitions");
    }
    for (int k = 0; k < 4; k++)
    {
        int64_t times[2];
        ZoneClosingTimes(zone, zone->firstYear + k / 2, times);
        const Raw* r = &b->items[b->count - 4 + k];
        const CompiledOffset* o = &zone->changes[k % 2].offset;
        if (r->at != times[k % 2] || r->standard != o->standard || r->save != o->save)
        {
            return Fail(b, "the closing rule differs from the years it closes");
        }
    }
    b->count -= 4;
    // Each change must change the offset, the first of a year included,
    // which follows the table's last.
    const CompiledChange* c = zone->changes;
    CompiledOffset last = b->count == 0 ? zone->initial
                                        : (CompiledOffset){b->items[b->count - 1].standard,
                                                           b->items[b->count - 1].save};
    if ((c[0].offset.standard == c[1].offset.standard && c[0].offset.save == c[1].offset.save) ||
        last.standard != c[1].offset.standard || last.save != c[1].offset.save)
    {
        return Fail(b, "the closing rule does not follow from the table's last offset");
    }
    return 1;
}

static int Compile(Builder* b, const Tzdb* t, const TzdbZone* z, CompiledZone* out)
{
    const TzdbLine* last = &t->lines[z->lineFirst + z->lineCount - 1];
    int rules[2] = {0};
    int closingCount = 0;
    if (!ClosingRules(b, t, last, rules, &closingCount))
    {
        return 0;
    }
    int32_t lastYear = LastNamedYear(t, z);
    out->closing = closingCount == 2;
    out->firstYear = lastYear + 1;
    if (!WalkLines(b, t, z, lastYear + (out->closing ? 2 : 0), &out->initial))
    {
        return 0;
    }
    Merge(b, out->initial);
    int32_t shift = 0;
    Shift(b, &out->initial, z->lineCount, &shift);
    // A shift can make two lines' offsets meet.
    DropUnchanged(b, out->initial);
    if (out->closing)
    {
        out->changes[0] = Change(t, last, rules[0], rules[1], shift);
        out->changes[1] = Change(t, last, rules[1], rules[0], shift);
        // February to November, so that a year's changes fall within it
        // in universal time (a day rule moves a week at most, a time a
        // week at most), and never on Feb 29.
        for (int k = 0; k < 2; k++)
        {
            const CompiledChange* c = &out->changes[k];
            if (c->month < 1 || c->month > 10 || (c->month == 1 && c->dayOfMonth == 29))
            {
                return Fail(b, "a closing change outside February to November, or on Feb 29");
            }
        }
        return CheckClosing(b, out);
    }
    return 1;
}

int ZoneCompile(const Tzdb* tzdb, int zone, CompiledZone* out, char* errorOut, size_t errorCapacity)
{
    *out = (CompiledZone){0};
    const TzdbZone* z = &tzdb->zones[zone];
    Builder b = {.errorOut = errorOut, .errorCapacity = errorCapacity, .zone = z->name};
    if (!Compile(&b, tzdb, z, out))
    {
        free(b.items);
        return 0;
    }
    out->transitions = malloc((size_t)(b.count == 0 ? 1 : b.count) * sizeof(CompiledTransition));
    if (out->transitions == nullptr)
    {
        free(b.items);
        return Fail(&b, "out of memory");
    }
    for (int i = 0; i < b.count; i++)
    {
        out->transitions[i] =
            (CompiledTransition){b.items[i].at, {b.items[i].standard, b.items[i].save}};
    }
    out->transitionCount = b.count;
    free(b.items);
    return 1;
}

void ZoneFree(CompiledZone* zone)
{
    free(zone->transitions);
    *zone = (CompiledZone){0};
}
