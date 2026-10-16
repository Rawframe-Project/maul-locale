// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Ranges of dates and times (date_range.h), as ICU4C's DateIntervalFormat
// writes them (formatImpl, fallbackFormat): the parts of a value belong
// to it where the other value's writing shows the same kinds of field,
// from the first such field to the last, as ICU's spans
// (FormattedValueStringBuilderImpl::addOverlapSpans) mark them.

#include "date_range.h"

#include "bytes.h"

#include "maul-locale/datetime.h"

#include <string.h>

static bool ValidHalf(const PatternTable* table, uint32_t index,
                      bool (*writable)(const uint8_t* pattern, uint32_t length))
{
    DataSpan pattern;
    return index == dateIntervalNone ||
           (mlocTablePattern(table, index, &pattern) && writable(pattern.bytes, pattern.size));
}

bool mlocValidRangePair(const PatternTable* table, const uint8_t* pair,
                        bool (*writable)(const uint8_t* pattern, uint32_t length))
{
    for (uint32_t field = 0; field < dateIntervalFields; field++)
    {
        uint32_t first = mlocLoad16(pair + 4 * (size_t)field);
        uint32_t second = mlocLoad16(pair + 4 * (size_t)field + 2);
        if (!ValidHalf(table, first, writable) || !ValidHalf(table, second, writable))
        {
            return false;
        }
    }
    return true;
}

uint32_t mlocDateDifference(const mlocDateFields* a, const mlocDateFields* b)
{
    if (a->eraLength != b->eraLength || memcmp(a->era, b->era, a->eraLength) != 0)
    {
        return rangeEra;
    }
    if (a->eraYear != b->eraYear)
    {
        return rangeYear;
    }
    if (a->monthCodeLength != b->monthCodeLength ||
        memcmp(a->monthCode, b->monthCode, a->monthCodeLength) != 0)
    {
        return rangeMonth;
    }
    return a->day != b->day ? rangeDay : rangeNone;
}

uint32_t mlocTimeDifference(const mlocTime* a, const mlocTime* b, uint32_t fractionDigits)
{
    if (a->hour / 12 != b->hour / 12)
    {
        return rangeAmPm;
    }
    if (a->hour != b->hour)
    {
        return rangeHour;
    }
    if (a->minute != b->minute)
    {
        return rangeMinute;
    }
    uint32_t unit = 1000000000;
    for (uint32_t i = 0; i < fractionDigits; i++)
    {
        unit /= 10;
    }
    bool fraction = fractionDigits > 0 && a->nanosecond / unit != b->nanosecond / unit;
    return a->second != b->second || fraction ? rangeSecond : rangeNone;
}

// The part kind a field letter writes; the literal kind for none.
static uint32_t LetterPart(uint8_t letter)
{
    switch (letter)
    {
    case 'G':
        return mloc_datePartEra;
    case 'y':
    case 'Y':
    case 'u':
        return mloc_datePartYear;
    case 'r':
        return mloc_datePartRelatedYear;
    case 'U':
        return mloc_datePartYearName;
    case 'M':
    case 'L':
        return mloc_datePartMonth;
    case 'E':
    case 'c':
    case 'e':
        return mloc_datePartWeekday;
    case 'd':
        return mloc_datePartDay;
    case 'h':
    case 'H':
    case 'K':
    case 'k':
        return mloc_datePartHour;
    case 'm':
        return mloc_datePartMinute;
    case 's':
        return mloc_datePartSecond;
    case 'a':
    case 'b':
    case 'B':
        return mloc_datePartDayPeriod;
    case 'z':
    case 'Z':
    case 'O':
    case 'v':
    case 'V':
    case 'X':
    case 'x':
        return mloc_datePartTimeZone;
    default:
        return mloc_datePartLiteral;
    }
}

// Whether a pattern's byte starts a field, with or without its
// numbering system.
static bool IsField(uint8_t byte)
{
    return byte == datePatternField || byte == datePatternOverride;
}

// The size of the item at a pattern's byte: a field's, a field with its
// numbering system's, or one byte of literal text.
static uint32_t ItemSize(const uint8_t* item)
{
    return item[0] == datePatternField ? 3 : item[0] == datePatternOverride ? 4u + item[3] : 1;
}

// The kinds (bits by mlocDatePart) of a pattern's fields.
static uint32_t PatternKinds(DataSpan pattern)
{
    uint32_t kinds = 0;
    for (uint32_t at = 0; at < pattern.size; at += ItemSize(pattern.bytes + at))
    {
        uint32_t kind = IsField(pattern.bytes[at]) ? LetterPart(pattern.bytes[at + 1]) : 0;
        kinds |= kind != mloc_datePartLiteral ? 1u << kind : 0;
    }
    return kinds;
}

// The span of a pattern's fields of the kinds, from the first to the end
// of the last.
static PatternSpan SpanOf(DataSpan pattern, uint32_t kinds, uint32_t source)
{
    PatternSpan span = {0, 0, source};
    bool found = false;
    for (uint32_t at = 0; at < pattern.size; at += ItemSize(pattern.bytes + at))
    {
        if (IsField(pattern.bytes[at]) && (kinds >> LetterPart(pattern.bytes[at + 1]) & 1) != 0)
        {
            span.first = found ? span.first : at;
            span.end = at + ItemSize(pattern.bytes + at);
            found = true;
        }
    }
    return span;
}

// The level ICU gives a field letter (SimpleDateFormat's
// getLevelFromChar): era 0, year 10, month 20, day 30, am or pm 40, hour
// 50, minute 60, second 70, fraction 80; -1 for a letter without one.
static int32_t LetterLevel(uint8_t letter)
{
    static const char s_chars[] = "ADEFGHKLMOQSUVWXYZacdeghklmqrsuvwxyz";
    static const int8_t s_levels[] = {40, 20, 30, 30, 0,  50, 50, 20, 20, 0,  20, 80,
                                      10, 0,  30, 0,  10, 0,  40, 30, 30, 30, 0,  50,
                                      50, 0,  60, 20, 10, 70, 10, 0,  20, 0,  10, 0};
    static_assert(sizeof(s_chars) == sizeof(s_levels) + 1, "a level per letter");
    const char* at = letter == 0 ? nullptr : strchr(s_chars, letter);
    return at == nullptr ? -1 : s_levels[at - s_chars];
}

// Whether a pattern shows no field as small as a difference
// (SimpleDateFormat::isFieldUnitIgnored).
static bool Ignores(DataSpan pattern, uint32_t difference)
{
    for (uint32_t at = 0; at < pattern.size; at += ItemSize(pattern.bytes + at))
    {
        if (IsField(pattern.bytes[at]) &&
            LetterLevel(pattern.bytes[at + 1]) >= 10 * (int32_t)difference)
        {
            return false;
        }
    }
    return true;
}

static void WriteOne(const RangeChoice* c, NumberWriter* w)
{
    PatternSpan none = {0, 0, 0};
    c->write(c->context, 0, c->own.bytes, c->own.size, &none, w);
}

// The halves: the first by the earlier value (the later under
// laterFirst), the second by the other.
static void WritePair(const RangeChoice* c, DataSpan first, DataSpan second, bool laterFirst,
                      NumberWriter* w)
{
    uint32_t value = laterFirst ? 1 : 0;
    uint32_t sources[2] = {mloc_dateRangeStart, mloc_dateRangeEnd};
    PatternSpan spans[2] = {{0, 0, 0}, {0, 0, 0}};
    if (w->parts != nullptr)
    {
        uint32_t shared = PatternKinds(first) & PatternKinds(second);
        spans[0] = SpanOf(first, shared, sources[value]);
        spans[1] = SpanOf(second, shared, sources[1 - value]);
    }
    c->write(c->context, value, first.bytes, first.size, &spans[0], w);
    c->write(c->context, 1 - value, second.bytes, second.size, &spans[1], w);
}

typedef struct FallbackWriting
{
    const RangeChoice* c;
    DataSpan pattern;
    PatternSpan spans[2];
} FallbackWriting;

// The fallback's {0}, the start, and {1}, the end.
static void WriteFallbackSlot(void* context, uint32_t slot, NumberWriter* w)
{
    const FallbackWriting* g = context;
    g->c->write(g->c->context, slot, g->pattern.bytes, g->pattern.size, &g->spans[slot], w);
}

static void WriteFallback(const RangeChoice* c, DataSpan pattern, NumberWriter* w)
{
    FallbackWriting g = {c, pattern, {{0, 0, 0}, {0, 0, 0}}};
    if (w->parts != nullptr)
    {
        uint32_t kinds = PatternKinds(pattern);
        g.spans[0] = SpanOf(pattern, kinds, mloc_dateRangeStart);
        g.spans[1] = SpanOf(pattern, kinds, mloc_dateRangeEnd);
    }
    mlocWriteGlue(c->fallback.bytes, c->fallback.size, WriteFallbackSlot, &g, w);
}

bool mlocRangeWritesTwo(const RangeChoice* c)
{
    if (c->difference == rangeNone)
    {
        return false;
    }
    uint32_t second = c->difference < dateIntervalFields
                          ? mlocLoad16(c->pair + 4 * (size_t)c->difference + 2)
                          : dateIntervalNone;
    return second != dateIntervalNone || !Ignores(c->own, c->difference);
}

bool mlocRangePrefix(const PatternTable* table, const uint8_t* prefix, uint32_t difference,
                     DataSpan* pattern, uint32_t* style)
{
    // The levels prefix d, M, y and G: the day's difference first.
    uint32_t level = rangeDay - difference;
    *style = mlocLoad16(prefix + 2 * dateIntervalPrefixes) >> (2 * level) & 3;
    return mlocTablePattern(table, mlocLoad16(prefix + 2 * (size_t)level), pattern);
}

void mlocWriteRange(const RangeChoice* c, NumberWriter* w)
{
    if (c->difference == rangeNone)
    {
        WriteOne(c, w);
        return;
    }
    uint32_t first = dateIntervalNone;
    uint32_t second = dateIntervalNone;
    bool laterFirst = false;
    if (c->difference < dateIntervalFields)
    {
        first = mlocLoad16(c->pair + 4 * (size_t)c->difference);
        second = mlocLoad16(c->pair + 4 * (size_t)c->difference + 2);
        uint32_t flags = mlocLoad16(c->pair + 4 * dateIntervalFields);
        laterFirst = (flags >> (dateIntervalLaterFirst + c->difference) & 1) != 0;
    }
    DataSpan halves[2];
    if (mlocTablePattern(c->table, first, &halves[0]) &&
        mlocTablePattern(c->table, second, &halves[1]))
    {
        WritePair(c, halves[0], halves[1], laterFirst, w);
    }
    else if (mlocTablePattern(c->table, second, &halves[1]))
    {
        WriteFallback(c, halves[1], w);
    }
    else if (Ignores(c->own, c->difference))
    {
        WriteOne(c, w);
    }
    else
    {
        WriteFallback(c, c->own, w);
    }
}

typedef struct JoinWriting
{
    const RangeJoin* j;
    uint32_t value;
    PatternSpan spans[2];
} JoinWriting;

// The glue's slots: {0} the time, {1} the date.
static void WriteJoinSlot(void* context, uint32_t slot, NumberWriter* w)
{
    const JoinWriting* g = context;
    const RangeJoin* j = g->j;
    if (slot == 0)
    {
        j->writeTime(j->context, g->value, j->time.bytes, j->time.size, &g->spans[0], w);
        return;
    }
    j->writeDate(j->context, g->value, j->date.bytes, j->date.size, &g->spans[1], w);
}

// A joined value: its parts' spans run from the first part's first field
// of the kinds to the second part's last.
static void WriteJoined(const RangeJoin* j, uint32_t value, bool parts, NumberWriter* w)
{
    JoinWriting g = {j, value, {{0, 0, 0}, {0, 0, 0}}};
    uint32_t source = value == 0 ? mloc_dateRangeStart : mloc_dateRangeEnd;
    if (parts)
    {
        uint32_t kinds = PatternKinds(j->date) | PatternKinds(j->time);
        PatternSpan time = SpanOf(j->time, kinds, source);
        PatternSpan date = SpanOf(j->date, kinds, source);
        bool dateFirst = false;
        for (uint32_t at = 0; at + 3 <= j->glue.size; at++)
        {
            if (j->glue.bytes[at] == '{' && j->glue.bytes[at + 2] == '}')
            {
                dateFirst = j->glue.bytes[at + 1] == '1';
                break;
            }
        }
        // The first part's span runs to its end, the second's from its
        // start.
        PatternSpan* first = dateFirst ? &date : &time;
        PatternSpan* second = dateFirst ? &time : &date;
        first->end = (dateFirst ? j->date : j->time).size;
        second->first = 0;
        g.spans[0] = time;
        g.spans[1] = date;
    }
    mlocWriteGlueBetween(j->glue.bytes, j->glue.size, WriteJoinSlot, &g, parts ? source : 0, w);
}

typedef struct JoinedFallback
{
    const RangeJoin* j;
    bool parts;
} JoinedFallback;

static void WriteJoinedSlot(void* context, uint32_t slot, NumberWriter* w)
{
    const JoinedFallback* g = context;
    WriteJoined(g->j, slot, g->parts, w);
}

void mlocWriteJoinedFallback(const RangeJoin* j, DataSpan fallback, NumberWriter* w)
{
    JoinedFallback g = {j, w->parts != nullptr};
    mlocWriteGlue(fallback.bytes, fallback.size, WriteJoinedSlot, &g, w);
}
