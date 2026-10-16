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

// The section offsets of a patterns unit (rows, sets, time sets, text),
// and whether its counts end at its size.
static bool Layout(DataSpan unit, uint32_t out[4])
{
    if (unit.size < datePatternsHeaderSize)
    {
        return false;
    }
    uint64_t patterns = mlocLoad32(unit.bytes);
    uint64_t rowsAt = (datePatternsHeaderSize + 4 * (patterns + 1) + 7) & ~7ULL;
    uint64_t setsAt = (rowsAt + (uint64_t)dateRowSize * mlocLoad32(unit.bytes + 4) + 7) & ~7ULL;
    uint64_t timesAt = (setsAt + (uint64_t)dateSetSize * mlocLoad32(unit.bytes + 8) + 7) & ~7ULL;
    uint64_t textAt =
        (timesAt + (uint64_t)dateTimeSetSize * mlocLoad32(unit.bytes + 12) + 7) & ~7ULL;
    out[0] = (uint32_t)rowsAt;
    out[1] = (uint32_t)setsAt;
    out[2] = (uint32_t)timesAt;
    out[3] = (uint32_t)textAt;
    return textAt + mlocLoad32(unit.bytes + 16) == unit.size;
}

DatePatterns mlocReadDatePatterns(DataSpan unit)
{
    DatePatterns p = {0};
    p.patternCount = mlocLoad32(unit.bytes);
    p.rowCount = mlocLoad32(unit.bytes + 4);
    p.setCount = mlocLoad32(unit.bytes + 8);
    p.timeSetCount = mlocLoad32(unit.bytes + 12);
    // A valid unit's layout holds; the offsets stay 0 for any other.
    uint32_t at[4] = {0};
    (void)Layout(unit, at);
    p.offsets = unit.bytes + datePatternsHeaderSize;
    p.rows = unit.bytes + at[0];
    p.sets = unit.bytes + at[1];
    p.timeSets = unit.bytes + at[2];
    p.text = unit.bytes + at[3];
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

bool mlocValidDatePatterns(const DataView* view, DataSpan payload)
{
    (void)view;
    uint32_t at[4] = {0};
    if (!Layout(payload, at))
    {
        return false;
    }
    DatePatterns p = mlocReadDatePatterns(payload);
    uint32_t text = mlocLoad32(payload.bytes + 16);
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
            mlocLoad16(set + 2 * dateTimeFlags) > dateTimeOwnTwelve)
        {
            return false;
        }
    }
    return true;
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
    if (!FindDates(data, locale, kindDateNames, 3 * dateCalendarCount, &unit, &index))
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
