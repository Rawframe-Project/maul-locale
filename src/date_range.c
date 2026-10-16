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
        uint32_t kind = pattern.bytes[at] < 0x20 ? LetterPart(pattern.bytes[at + 1]) : 0;
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
        if (pattern.bytes[at] < 0x20 && (kinds >> LetterPart(pattern.bytes[at + 1]) & 1) != 0)
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
        if (pattern.bytes[at] < 0x20 &&
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
