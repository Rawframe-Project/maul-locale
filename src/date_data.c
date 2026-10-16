// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units (date_data.h).

#include "date_data.h"

#include "bytes.h"
#include "data_format.h"
#include "kinds.h"
#include "locale_fallback.h"
#include "number_data.h"

#include <string.h>

// A field item's length at text, or 0 when it is malformed.
static uint32_t FieldLength(const uint8_t* text, uint32_t left)
{
    if (left < 3 || text[1] == '\0' || strchr(datePatternLetters, text[1]) == nullptr ||
        text[2] < 1 || text[2] > dateMaxWidth)
    {
        return 0;
    }
    if (text[0] == datePatternField)
    {
        return 3;
    }
    uint32_t id = left > 3 ? text[3] : 0;
    if (id < 1 || id > dateMaxSystemId || 4 + id > left)
    {
        return 0;
    }
    for (uint32_t i = 0; i < id; i++)
    {
        if (text[4 + i] < 'a' || text[4 + i] > 'z')
        {
            return 0;
        }
    }
    return 4 + id;
}

bool mlocValidDatePattern(const uint8_t* text, uint32_t length)
{
    uint32_t at = 0;
    while (at < length)
    {
        if (text[at] == datePatternField || text[at] == datePatternOverride)
        {
            uint32_t n = FieldLength(text + at, length - at);
            if (n == 0)
            {
                return false;
            }
            at += n;
            continue;
        }
        uint32_t end = at;
        while (end < length && text[end] >= 0x20)
        {
            end++;
        }
        if (end == at || !mlocValidDataText(text + at, end - at, 0))
        {
            return false;
        }
        at = end;
    }
    return length > 0;
}

enum
{
    // The sections after the offsets: rows, sets, time sets, interval
    // sets, date parts, rows, time parts, prefixes and pairs.
    dateSectionCount = 9,
};

// The section offsets of a patterns unit (its sections, then the text),
// and whether its counts end at its size.
static bool Layout(DataSpan unit, uint32_t out[dateSectionCount + 1])
{
    static const uint32_t s_sizes[dateSectionCount] = {
        dateRowSize,          dateSetSize,         dateTimeSetSize,      dateIntervalSetSize,
        dateIntervalDateSize, dateIntervalRowSize, dateIntervalTimeSize, dateIntervalPrefixSize,
        dateIntervalPairSize};
    if (unit.size < datePatternsHeaderSize)
    {
        return false;
    }
    uint64_t at = (datePatternsHeaderSize + 4 * ((uint64_t)mlocLoad32(unit.bytes) + 1) + 7) & ~7ULL;
    for (uint32_t i = 0; i < dateSectionCount; i++)
    {
        out[i] = (uint32_t)at;
        at = (at + (uint64_t)s_sizes[i] * mlocLoad32(unit.bytes + 4 + 4 * (size_t)i) + 7) & ~7ULL;
    }
    out[dateSectionCount] = (uint32_t)at;
    return at + mlocLoad32(unit.bytes + 4 + 4 * dateSectionCount) == unit.size;
}

DatePatterns mlocReadDatePatterns(DataSpan unit)
{
    DatePatterns p = {0};
    uint32_t counts[dateSectionCount + 1];
    for (uint32_t i = 0; i <= dateSectionCount; i++)
    {
        counts[i] = mlocLoad32(unit.bytes + 4 * (size_t)i);
    }
    p.patternCount = counts[0];
    p.rowCount = counts[1];
    p.setCount = counts[2];
    p.timeSetCount = counts[3];
    p.intervalSetCount = counts[4];
    p.datePartCount = counts[5];
    p.intervalRowCount = counts[6];
    p.timePartCount = counts[7];
    p.prefixCount = counts[8];
    p.pairCount = counts[9];
    // A valid unit's layout holds; the offsets stay 0 for any other.
    uint32_t at[dateSectionCount + 1] = {0};
    (void)Layout(unit, at);
    p.offsets = unit.bytes + datePatternsHeaderSize;
    p.rows = unit.bytes + at[0];
    p.sets = unit.bytes + at[1];
    p.timeSets = unit.bytes + at[2];
    p.intervalSets = unit.bytes + at[3];
    p.dateParts = unit.bytes + at[4];
    p.intervalRows = unit.bytes + at[5];
    p.timeParts = unit.bytes + at[6];
    p.prefixes = unit.bytes + at[7];
    p.pairs = unit.bytes + at[8];
    p.text = unit.bytes + at[dateSectionCount];
    return p;
}

static bool ValidIndexes(const uint8_t* bytes, uint32_t count, uint32_t limit)
{
    for (uint32_t i = 0; i < count; i++)
    {
        if (mlocLoad16(bytes + 2 * (size_t)i) >= limit)
        {
            return false;
        }
    }
    return true;
}

// Whether time set flags name only known bits, each month code 0 to 2.
static bool ValidTimeFlags(uint32_t flags)
{
    for (uint32_t length = 0; length < dateLengthCount; length++)
    {
        if (((flags >> (dateTimeMonthShift + 2 * length)) & 3) == 3)
        {
            return false;
        }
    }
    return flags <= dateTimeFlagsMax;
}

// Whether each record of a section holds, from a byte offset, values
// u16 indexes below their limits: the i-th below limits[i % groups].
static bool ValidGroups(const uint8_t* records, uint32_t recordCount, uint32_t size,
                        uint32_t offset, uint32_t values, const uint32_t* limits, uint32_t groups)
{
    for (uint32_t r = 0; r < recordCount; r++)
    {
        const uint8_t* record = records + (size_t)r * size + offset;
        for (uint32_t i = 0; i < values; i++)
        {
            if (mlocLoad16(record + 2 * (size_t)i) >= limits[i % groups])
            {
                return false;
            }
        }
    }
    return true;
}

// Whether each record holds u16 pattern indexes or none, then flags with
// no bit from a shift up.
static bool ValidPatternsOrNone(const uint8_t* records, uint32_t recordCount, uint32_t size,
                                uint32_t values, uint32_t patterns, uint32_t shift)
{
    for (uint32_t r = 0; r < recordCount; r++)
    {
        const uint8_t* record = records + (size_t)r * size;
        for (uint32_t i = 0; i < values; i++)
        {
            uint32_t index = mlocLoad16(record + 2 * (size_t)i);
            if (index != dateIntervalNone && index >= patterns)
            {
                return false;
            }
        }
        if (mlocLoad16(record + 2 * (size_t)values) >> shift != 0)
        {
            return false;
        }
    }
    return true;
}

// Whether the interval sections index what they name: sets their parts
// and patterns, date parts their rows and the formats' pairs, prefixes
// and patterns, rows their pairs and prefixes, time parts their pairs and
// the formats' pairs and patterns, prefixes and pairs their patterns or
// none.
static bool ValidIntervals(const DatePatterns* p)
{
    enum
    {
        times = dateIntervalTimeStyle - dateIntervalTime,
    };
    const uint32_t sets[5] = {p->datePartCount, p->timePartCount, p->patternCount, p->patternCount,
                              p->patternCount};
    const uint32_t styles[3] = {p->pairCount, p->prefixCount, p->patternCount};
    const uint32_t rows[2] = {p->pairCount, p->prefixCount};
    const uint32_t timeStyles[2] = {p->pairCount, p->patternCount};
    return ValidGroups(p->intervalSets, p->intervalSetCount, dateIntervalSetSize, 0, 5, sets, 5) &&
           ValidGroups(p->dateParts, p->datePartCount, dateIntervalDateSize, 0, dateEntryCount,
                       &p->intervalRowCount, 1) &&
           ValidGroups(p->dateParts, p->datePartCount, dateIntervalDateSize, 2 * dateEntryCount,
                       3 * dateStandardCount, styles, 3) &&
           ValidGroups(p->intervalRows, p->intervalRowCount, dateIntervalRowSize, 0,
                       2 * dateLengthCount, rows, 2) &&
           ValidGroups(p->timeParts, p->timePartCount, dateIntervalTimeSize, 0, times,
                       &p->pairCount, 1) &&
           ValidGroups(p->timeParts, p->timePartCount, dateIntervalTimeSize, 2 * times,
                       2 * dateStandardCount, timeStyles, 2) &&
           ValidPatternsOrNone(p->prefixes, p->prefixCount, dateIntervalPrefixSize,
                               dateIntervalPrefixes, p->patternCount, 2 * dateIntervalPrefixes) &&
           ValidPatternsOrNone(p->pairs, p->pairCount, dateIntervalPairSize, 2 * dateIntervalFields,
                               p->patternCount, dateIntervalFields);
}

bool mlocValidDatePatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    uint32_t at[dateSectionCount + 1] = {0};
    if (!Layout(payload, at))
    {
        return false;
    }
    DatePatterns p = mlocReadDatePatterns(payload);
    uint32_t text = mlocLoad32(payload.bytes + 4 + 4 * dateSectionCount);
    if (mlocLoad32(p.offsets) != 0 || mlocLoad32(p.offsets + 4 * (size_t)p.patternCount) != text)
    {
        return false;
    }
    for (uint32_t i = 0; i < p.patternCount; i++)
    {
        uint32_t start = mlocLoad32(p.offsets + 4 * (size_t)i);
        uint32_t end = mlocLoad32(p.offsets + 4 * (size_t)i + 4);
        if (end < start || end > text || !mlocValidDatePattern(p.text + start, end - start))
        {
            return false;
        }
    }
    if (!ValidIndexes(p.rows, p.rowCount * dateLengthCount, p.patternCount))
    {
        return false;
    }
    for (uint32_t s = 0; s < p.setCount; s++)
    {
        const uint8_t* set = p.sets + (size_t)s * dateSetSize;
        if (!ValidIndexes(set, dateEntryCount, p.rowCount) ||
            !ValidIndexes(set + 2 * dateEntryCount, dateStandardCount, p.patternCount))
        {
            return false;
        }
    }
    for (uint32_t s = 0; s < p.timeSetCount; s++)
    {
        const uint8_t* set = p.timeSets + (size_t)s * dateTimeSetSize;
        if (!ValidIndexes(set, dateTimePatternCount, p.patternCount) ||
            !ValidTimeFlags(mlocLoad16(set + 2 * dateTimeFlags)))
        {
            return false;
        }
    }
    return ValidIntervals(&p);
}

// The section offsets of a names unit (lists, items, sets, rules, text),
// and whether its counts end at its size.
static bool NamesLayout(DataSpan unit, uint32_t out[5])
{
    if (unit.size < dateNamesHeaderSize)
    {
        return false;
    }
    uint64_t strings = mlocLoad32(unit.bytes);
    uint64_t lists = mlocLoad32(unit.bytes + 4);
    uint64_t sets = mlocLoad32(unit.bytes + 8);
    uint64_t listsAt = (dateNamesHeaderSize + 4 * (strings + 1) + 7) & ~7ULL;
    uint64_t itemsAt = listsAt + 4 * (lists + 1);
    if (itemsAt > unit.size)
    {
        return false;
    }
    uint64_t items = mlocLoad32(unit.bytes + itemsAt - 4);
    uint64_t setsAt = (itemsAt + 2 * items + 7) & ~7ULL;
    uint64_t rulesAt = (setsAt + (uint64_t)dateNameSetSize * sets + 7) & ~7ULL;
    uint64_t textAt = (rulesAt + (uint64_t)dateRuleSize * mlocLoad32(unit.bytes + 12) + 7) & ~7ULL;
    out[0] = (uint32_t)listsAt;
    out[1] = (uint32_t)itemsAt;
    out[2] = (uint32_t)setsAt;
    out[3] = (uint32_t)rulesAt;
    out[4] = (uint32_t)textAt;
    return textAt + mlocLoad32(unit.bytes + 16) == unit.size;
}

DateNameTables mlocReadDateNames(DataSpan unit)
{
    DateNameTables t = {0};
    uint32_t at[5] = {0};
    (void)NamesLayout(unit, at);
    t.stringCount = mlocLoad32(unit.bytes);
    t.listCount = mlocLoad32(unit.bytes + 4);
    t.setCount = mlocLoad32(unit.bytes + 8);
    t.ruleCount = mlocLoad32(unit.bytes + 12);
    t.strings = unit.bytes + dateNamesHeaderSize;
    t.lists = unit.bytes + at[0];
    t.items = unit.bytes + at[1];
    t.sets = unit.bytes + at[2];
    t.rules = unit.bytes + at[3];
    t.text = unit.bytes + at[4];
    return t;
}

DateNameList mlocDateNameList(const DateNameTables* t, uint32_t list)
{
    uint32_t start = mlocLoad32(t->lists + 4 * (size_t)list);
    uint32_t end = mlocLoad32(t->lists + 4 * (size_t)list + 4);
    return (DateNameList){t->items + 2 * (size_t)start, end - start};
}

DataSpan mlocDateName(const DateNameTables* t, DateNameList list, uint32_t item)
{
    if (item >= list.count)
    {
        return (DataSpan){t->text, 0};
    }
    uint32_t s = mlocLoad16(list.items + 2 * (size_t)item);
    uint32_t start = mlocLoad32(t->strings + 4 * (size_t)s);
    return (DataSpan){t->text + start, mlocLoad32(t->strings + 4 * (size_t)s + 4) - start};
}

// Whether offsets (u32, count + 1) run from 0 up to end without falling.
static bool Ascending(const uint8_t* offsets, uint32_t count, uint32_t end)
{
    uint32_t previous = 0;
    for (uint32_t i = 0; i <= count; i++)
    {
        uint32_t at = mlocLoad32(offsets + 4 * (size_t)i);
        if (at < previous || (i == 0 && at != 0))
        {
            return false;
        }
        previous = at;
    }
    return previous == end;
}

// Whether a list's size suits its kind. A leap month pattern's "{0}" is
// checked where it is used: checking it here would scan a list again for
// every set that names it.
static bool ValidList(uint32_t kind, DateNameList list)
{
    switch (kind)
    {
    case dateNamesMonths:
        return list.count == dateMonthListSize;
    case dateNamesWeekdays:
        return list.count == dateWeekdayListSize;
    case dateNamesQuarters:
        return list.count == dateQuarterListSize;
    case dateNamesEras:
        return list.count % 3 == 0 && list.count <= 3 * dateMaxEras;
    case dateNamesCyclic:
        return list.count == 0 || list.count == dateCyclicListSize;
    case dateNamesLeap:
        return list.count == 0 || list.count == dateLeapListSize;
    default:
        return list.count == dateDayPeriodListSize;
    }
}

// Whether day period rules give each hour am, pm or a flexible period
// (noon and midnight are named only at their instant) and known flags.
static bool ValidRules(const uint8_t* rules, uint32_t count)
{
    for (uint32_t r = 0; r < count; r++)
    {
        const uint8_t* rule = rules + (size_t)r * dateRuleSize;
        for (uint32_t hour = 0; hour < 24; hour++)
        {
            if (rule[hour] >= dateDayPeriodCount || rule[hour] == dateDayPeriodMidnight ||
                rule[hour] == dateDayPeriodNoon)
            {
                return false;
            }
        }
        if (rule[24] > (dateRuleNoon | dateRuleMidnight))
        {
            return false;
        }
    }
    return true;
}

bool mlocValidDateNames(const DataView* view, DataSpan payload)
{
    (void)view;
    uint32_t at[5];
    if (!NamesLayout(payload, at))
    {
        return false;
    }
    DateNameTables t = mlocReadDateNames(payload);
    uint32_t text = mlocLoad32(payload.bytes + 16);
    uint32_t items = mlocLoad32(t.lists + 4 * (size_t)t.listCount);
    if (!Ascending(t.strings, t.stringCount, text) || !Ascending(t.lists, t.listCount, items) ||
        !ValidIndexes(t.items, items, t.stringCount))
    {
        return false;
    }
    for (uint32_t i = 0; i < t.stringCount; i++)
    {
        uint32_t start = mlocLoad32(t.strings + 4 * (size_t)i);
        uint32_t end = mlocLoad32(t.strings + 4 * (size_t)i + 4);
        if (!mlocValidDataText(t.text + start, end - start, 0))
        {
            return false;
        }
    }
    for (uint32_t s = 0; s < t.setCount; s++)
    {
        for (uint32_t k = 0; k < dateNameKindCount; k++)
        {
            uint32_t list = mlocLoad16(t.sets + (size_t)s * dateNameSetSize + 2 * k);
            if (list >= t.listCount || !ValidList(k, mlocDateNameList(&t, list)))
            {
                return false;
            }
        }
    }
    return ValidRules(t.rules, t.ruleCount);
}

bool mlocValidDates(const DataView* view, DataSpan payload)
{
    (void)view;
    return payload.size == dateLocaleSize;
}

// A locale's dates unit along its main chain, a u16 of it (at index,
// in u16s) and the unit of no locale of a kind beside it.
static bool FindDates(const mlocDataSet* data, const mlocLocale* locale, uint32_t kind,
                      uint32_t index, DataSpan* unit, uint32_t* value)
{
    DataSpan dates;
    bool fellBack = false;
    DataView view;
    if (!mlocFindLocaleUnit(data, mloc_componentMain, kindDates, locale, &dates, &fellBack,
                            &view) ||
        !mlocFindDataUnit(&view, kind, dataNoLocale, unit))
    {
        return false;
    }
    *value = mlocLoad16(dates.bytes + 2 * (size_t)index);
    return true;
}

bool mlocFindDateSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                     DatePatterns* patterns, const uint8_t** set)
{
    DataSpan unit;
    uint32_t index = 0;
    if (calendar >= dateCalendarCount ||
        !FindDates(data, locale, kindDatePatterns, calendar, &unit, &index))
    {
        return false;
    }
    *patterns = mlocReadDatePatterns(unit);
    if (index >= patterns->setCount)
    {
        return false;
    }
    *set = patterns->sets + (size_t)index * dateSetSize;
    return true;
}

bool mlocFindTimeSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                     DatePatterns* patterns, const uint8_t** set)
{
    DataSpan unit;
    uint32_t index = 0;
    if (calendar >= dateCalendarCount ||
        !FindDates(data, locale, kindDatePatterns, 2 * dateCalendarCount + calendar, &unit, &index))
    {
        return false;
    }
    *patterns = mlocReadDatePatterns(unit);
    if (index >= patterns->timeSetCount)
    {
        return false;
    }
    *set = patterns->timeSets + (size_t)index * dateTimeSetSize;
    return true;
}

bool mlocFindIntervalSet(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                         DatePatterns* patterns, const uint8_t** set)
{
    DataSpan unit;
    uint32_t index = 0;
    if (calendar >= dateCalendarCount ||
        !FindDates(data, locale, kindDatePatterns, 3 * dateCalendarCount + calendar, &unit, &index))
    {
        return false;
    }
    *patterns = mlocReadDatePatterns(unit);
    if (index >= patterns->intervalSetCount)
    {
        return false;
    }
    *set = patterns->intervalSets + (size_t)index * dateIntervalSetSize;
    return true;
}

bool mlocFindDateNames(const mlocDataSet* data, const mlocLocale* locale, uint32_t calendar,
                       DateNameTables* names, const uint8_t** set)
{
    DataSpan unit;
    uint32_t index = 0;
    if (calendar >= dateCalendarCount ||
        !FindDates(data, locale, kindDateNames, dateCalendarCount + calendar, &unit, &index))
    {
        return false;
    }
    *names = mlocReadDateNames(unit);
    if (index >= names->setCount)
    {
        return false;
    }
    *set = names->sets + (size_t)index * dateNameSetSize;
    return true;
}

bool mlocFindDayPeriodRules(const mlocDataSet* data, const mlocLocale* locale,
                            const uint8_t** rules)
{
    DataSpan unit;
    uint32_t index = 0;
    if (!FindDates(data, locale, kindDateNames, 4 * dateCalendarCount, &unit, &index))
    {
        return false;
    }
    DateNameTables names = mlocReadDateNames(unit);
    if (index >= names.ruleCount)
    {
        return false;
    }
    *rules = names.rules + (size_t)index * dateRuleSize;
    return true;
}

bool mlocDatePattern(const DatePatterns* p, uint32_t index, DataSpan* out)
{
    if (index >= p->patternCount)
    {
        return false;
    }
    uint32_t start = mlocLoad32(p->offsets + 4 * (size_t)index);
    out->bytes = p->text + start;
    out->size = mlocLoad32(p->offsets + 4 * (size_t)index + 4) - start;
    return true;
}
