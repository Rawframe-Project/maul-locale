// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date names unit (date_names_gen.h). Every name is CLDR's value of
// its path under the calendar, root's aliases followed from the locale
// (stand-alone names to format ones, narrow eras to abbreviated, other
// calendars to the Gregorian); a name no path gives is empty.

#include "date_names_gen.h"

#include "date_tables.h"
#include "kinds.h"
#include "table_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxList = dateCyclicListSize,
};

static const char* const s_contexts[3] = {"format", "stand-alone", "numeric"};
static const char* const s_widths[4] = {"abbreviated", "wide", "narrow", "short"};
static const char* const s_days[7] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

static void* Grown(void* p, int* capacity, int count, size_t size)
{
    if (count < *capacity)
    {
        return p;
    }
    *capacity = *capacity == 0 ? 1024 : *capacity * 2;
    void* grown = realloc(p, size * (size_t)*capacity);
    if (grown == nullptr)
    {
        GenDie("out of memory", "date names");
    }
    return grown;
}

static uint16_t AddString(DateNames* n, const char* text)
{
    void* found = MapGet(&n->strings, text);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (n->stringCount >= 0xFFFF)
    {
        GenDie("too many date names", text);
    }
    n->texts = Grown(n->texts, &n->stringCapacity, n->stringCount, sizeof(char*));
    size_t length = strlen(text);
    n->texts[n->stringCount] = GenAllocate(length + 1);
    memcpy(n->texts[n->stringCount], text, length);
    MapPut(&n->strings, text, (void*)(uintptr_t)(n->stringCount + 1));
    return (uint16_t)n->stringCount++;
}

// A record's key in a map: its u16 values in hex, and its length.
static void RecordKey(const uint16_t* values, uint32_t count, char* out)
{
    snprintf(out, 6, "%04x:", count);
    for (uint32_t i = 0; i < count; i++)
    {
        snprintf(out + 5 + 4 * i, 5, "%04x", values[i]);
    }
}

static uint16_t AddList(DateNames* n, const uint16_t* items, uint32_t count)
{
    char key[5 + 4 * maxList + 1];
    RecordKey(items, count, key);
    void* found = MapGet(&n->lists, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (n->listCount >= 0xFFFF)
    {
        GenDie("too many date name lists", key);
    }
    n->listStarts = Grown(n->listStarts, &n->listCapacity, n->listCount, sizeof(uint32_t));
    while (n->itemCount + count > n->itemCapacity)
    {
        int capacity = (int)n->itemCapacity;
        n->items = Grown(n->items, &capacity, capacity, sizeof(uint16_t));
        n->itemCapacity = (uint32_t)capacity;
    }
    n->listStarts[n->listCount] = n->itemCount;
    memcpy(n->items + n->itemCount, items, sizeof(uint16_t) * count);
    n->itemCount += count;
    MapPut(&n->lists, key, (void*)(uintptr_t)(n->listCount + 1));
    return (uint16_t)n->listCount++;
}

// The name of a path under the calendar, or the empty string.
static uint16_t Name(DateNames* n, Cldr* cldr, const char* locale, const char* type,
                     const char* rest)
{
    const char* value = DateCalendarValue(cldr, locale, type, rest);
    return AddString(n, value != nullptr ? value : "");
}

static uint16_t Months(DateNames* n, Cldr* cldr, const char* locale, const char* type)
{
    uint16_t items[dateMonthListSize];
    uint32_t count = 0;
    for (int c = 0; c < 2; c++)
    {
        for (int w = 0; w < 3; w++)
        {
            for (int m = 1; m <= dateMonthCount; m++)
            {
                char rest[256];
                snprintf(rest, sizeof(rest),
                         m < dateMonthCount
                             ? "months/monthContext[@type=\"%s\"]/monthWidth[@type=\"%s\"]/"
                               "month[@type=\"%d\"]"
                             : "months/monthContext[@type=\"%s\"]/monthWidth[@type=\"%s\"]/"
                               "month[@type=\"7\"][@yeartype=\"leap\"]",
                         s_contexts[c], s_widths[w], m);
                items[count++] = Name(n, cldr, locale, type, rest);
            }
        }
    }
    return AddList(n, items, count);
}

static uint16_t Weekdays(DateNames* n, Cldr* cldr, const char* locale, const char* type)
{
    uint16_t items[dateWeekdayListSize];
    uint32_t count = 0;
    for (int c = 0; c < 2; c++)
    {
        for (int w = 0; w < 4; w++)
        {
            for (int d = 0; d < 7; d++)
            {
                char rest[256];
                snprintf(rest, sizeof(rest),
                         "days/dayContext[@type=\"%s\"]/dayWidth[@type=\"%s\"]/day[@type=\"%s\"]",
                         s_contexts[c], s_widths[w], s_days[d]);
                items[count++] = Name(n, cldr, locale, type, rest);
            }
        }
    }
    return AddList(n, items, count);
}

static uint16_t Quarters(DateNames* n, Cldr* cldr, const char* locale, const char* type)
{
    uint16_t items[dateQuarterListSize];
    uint32_t count = 0;
    for (int c = 0; c < 2; c++)
    {
        for (int w = 0; w < 3; w++)
        {
            for (int q = 1; q <= 4; q++)
            {
                char rest[256];
                snprintf(rest, sizeof(rest),
                         "quarters/quarterContext[@type=\"%s\"]/quarterWidth[@type=\"%s\"]/"
                         "quarter[@type=\"%d\"]",
                         s_contexts[c], s_widths[w], q);
                items[count++] = Name(n, cldr, locale, type, rest);
            }
        }
    }
    return AddList(n, items, count);
}

// The calendar's era types in the order of dateEraCodes.
static int EraTypes(const DateSupplement* s, int calendar, const char* type,
                    const char* out[dateMaxEras])
{
    static const char* const s_codes[dateCalendarCount] = dateEraCodes;
    int count = 0;
    char codes[64];
    snprintf(codes, sizeof(codes), "%s", s_codes[calendar]);
    for (char* code = strtok(codes, " "); code != nullptr; code = strtok(nullptr, " "))
    {
        char key[128];
        snprintf(key, sizeof(key), "%s %s", type, code);
        out[count] = MapGet(&s->eras, key);
        if (out[count] == nullptr || count + 1 >= dateMaxEras)
        {
            GenDie("an era calendarData does not list", key);
        }
        count++;
    }
    return count;
}

static uint16_t Eras(DateNames* n, Cldr* cldr, const DateSupplement* s, const char* locale,
                     int calendar, const char* type)
{
    static const char* const s_elements[3] = {"eraAbbr", "eraNames", "eraNarrow"};
    const char* types[dateMaxEras];
    int eras = EraTypes(s, calendar, type, types);
    uint16_t items[3 * dateMaxEras];
    uint32_t count = 0;
    for (int w = 0; w < 3; w++)
    {
        for (int e = 0; e < eras; e++)
        {
            char rest[256];
            snprintf(rest, sizeof(rest), "eras/%s/era[@type=\"%s\"]", s_elements[w], types[e]);
            items[count++] = Name(n, cldr, locale, type, rest);
        }
    }
    return AddList(n, items, count);
}

// Cyclic years and leap month patterns, for the calendars that have
// them (an empty list for the others).
static uint16_t Cyclic(DateNames* n, Cldr* cldr, const char* locale, const char* type, bool has)
{
    uint16_t items[dateCyclicListSize];
    uint32_t count = 0;
    for (int w = 0; has && w < 3; w++)
    {
        for (int y = 1; y <= 60; y++)
        {
            char rest[256];
            snprintf(rest, sizeof(rest),
                     "cyclicNameSets/cyclicNameSet[@type=\"years\"]/cyclicNameContext[@type="
                     "\"format\"]/cyclicNameWidth[@type=\"%s\"]/cyclicName[@type=\"%d\"]",
                     s_widths[w], y);
            items[count++] = Name(n, cldr, locale, type, rest);
        }
    }
    return AddList(n, items, count);
}

static uint16_t DayPeriods(DateNames* n, Cldr* cldr, const char* locale, const char* type)
{
    static const char* const s_periods[dateDayPeriodCount] = {
        "am",         "pm",         "midnight", "noon",     "morning1", "morning2",
        "afternoon1", "afternoon2", "evening1", "evening2", "night1",   "night2"};
    uint16_t items[dateDayPeriodListSize];
    uint32_t count = 0;
    for (int c = 0; c < 2; c++)
    {
        for (int w = 0; w < 3; w++)
        {
            for (int p = 0; p < dateDayPeriodCount; p++)
            {
                char rest[256];
                snprintf(rest, sizeof(rest),
                         "dayPeriods/dayPeriodContext[@type=\"%s\"]/dayPeriodWidth[@type=\"%s\"]/"
                         "dayPeriod[@type=\"%s\"]",
                         s_contexts[c], s_widths[w], s_periods[p]);
                items[count++] = Name(n, cldr, locale, type, rest);
            }
        }
    }
    return AddList(n, items, count);
}

static uint16_t Leap(DateNames* n, Cldr* cldr, const char* locale, const char* type, bool has)
{
    uint16_t items[dateLeapListSize];
    uint32_t count = 0;
    for (int c = 0; has && c < 3; c++)
    {
        for (int w = 0; w < 3; w++)
        {
            char rest[256];
            snprintf(rest, sizeof(rest),
                     "monthPatterns/monthPatternContext[@type=\"%s\"]/monthPatternWidth[@type="
                     "\"%s\"]/monthPattern[@type=\"leap\"]",
                     s_contexts[c], c == 2 ? "all" : s_widths[w]);
            items[count] = Name(n, cldr, locale, type, rest);
            if (strstr(n->texts[items[count]], "{0}") == nullptr)
            {
                GenDie("a leap month pattern without {0}", locale);
            }
            count++;
        }
    }
    return AddList(n, items, count);
}

uint16_t DateNamesSet(DateNames* n, Cldr* cldr, const DateSupplement* supplement,
                      const char* locale, int calendar, const char* type)
{
    // The Chinese and Dangi calendars name their years in a cycle and
    // their leap months by a pattern.
    bool lunisolar = calendar == 16 || calendar == 17;
    uint16_t set[dateNameKindCount] = {
        Months(n, cldr, locale, type),
        Weekdays(n, cldr, locale, type),
        Quarters(n, cldr, locale, type),
        Eras(n, cldr, supplement, locale, calendar, type),
        Cyclic(n, cldr, locale, type, lunisolar),
        Leap(n, cldr, locale, type, lunisolar),
        DayPeriods(n, cldr, locale, type),
    };
    char key[5 + 4 * dateNameKindCount + 1];
    RecordKey(set, dateNameKindCount, key);
    void* found = MapGet(&n->sets, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    if (n->setCount >= 0xFFFF)
    {
        GenDie("too many date name sets", locale);
    }
    n->setData = Grown(n->setData, &n->setCapacity, n->setCount, dateNameSetSize);
    memcpy(n->setData + (size_t)n->setCount * dateNameKindCount, set, dateNameSetSize);
    MapPut(&n->sets, key, (void*)(uintptr_t)(n->setCount + 1));
    return (uint16_t)n->setCount++;
}

uint16_t DateNamesRules(DateNames* n, const DateSupplement* supplement, const char* locale)
{
    // By truncation, as ICU's DayPeriodRules looks them up: zh_Hant takes
    // zh's, where CLDR's parent locales would give root's.
    char id[64];
    // A listed id, which GenListXml keeps under genMaxId (64) bytes.
    snprintf(id, sizeof(id), "%s", locale);
    const uint8_t* rules = MapGet(&supplement->dayPeriods, id);
    for (char* cut = strrchr(id, '_'); rules == nullptr && cut != nullptr; cut = strrchr(id, '_'))
    {
        *cut = '\0';
        rules = MapGet(&supplement->dayPeriods, id);
    }
    if (rules == nullptr)
    {
        rules = MapGet(&supplement->dayPeriods, "root");
    }
    char key[2 * dateRuleSize + 1];
    for (int i = 0; i < dateRuleSize; i++)
    {
        snprintf(key + 2 * i, 3, "%02x", rules[i]);
    }
    void* found = MapGet(&n->rules, key);
    if (found != nullptr)
    {
        return (uint16_t)((uintptr_t)found - 1);
    }
    n->ruleData = Grown(n->ruleData, &n->ruleCapacity, n->ruleCount, dateRuleSize);
    memcpy(n->ruleData + (size_t)n->ruleCount * dateRuleSize, rules, dateRuleSize);
    MapPut(&n->rules, key, (void*)(uintptr_t)(n->ruleCount + 1));
    return (uint16_t)n->ruleCount++;
}

static size_t Align8(size_t n)
{
    return (n + 7) & ~(size_t)7;
}

void DateNamesFinish(DateNames* n, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < n->stringCount; i++)
    {
        text += strlen(n->texts[i]);
    }
    size_t strings = dateNamesHeaderSize;
    size_t lists = Align8(strings + 4 * ((size_t)n->stringCount + 1));
    size_t items = lists + 4 * ((size_t)n->listCount + 1);
    size_t sets = Align8(items + 2 * (size_t)n->itemCount);
    size_t rules = Align8(sets + (size_t)dateNameSetSize * (size_t)n->setCount);
    size_t texts = Align8(rules + (size_t)dateRuleSize * (size_t)n->ruleCount);
    uint8_t* unit = GenAllocate(texts + text);
    GenPut(unit, (uint64_t)n->stringCount, 4);
    GenPut(unit + 4, (uint64_t)n->listCount, 4);
    GenPut(unit + 8, (uint64_t)n->setCount, 4);
    GenPut(unit + 12, (uint64_t)n->ruleCount, 4);
    GenPut(unit + 16, text, 4);
    memcpy(unit + rules, n->ruleData, (size_t)dateRuleSize * (size_t)n->ruleCount);
    size_t at = 0;
    for (int i = 0; i <= n->stringCount; i++)
    {
        GenPut(unit + strings + 4 * (size_t)i, at, 4);
        if (i < n->stringCount)
        {
            size_t length = strlen(n->texts[i]);
            memcpy(unit + texts + at, n->texts[i], length);
            at += length;
            free(n->texts[i]);
        }
    }
    for (int i = 0; i <= n->listCount; i++)
    {
        GenPut(unit + lists + 4 * (size_t)i, i < n->listCount ? n->listStarts[i] : n->itemCount, 4);
    }
    for (uint32_t i = 0; i < n->itemCount; i++)
    {
        GenPut(unit + items + 2 * (size_t)i, n->items[i], 2);
    }
    for (int i = 0; i < n->setCount * dateNameKindCount; i++)
    {
        GenPut(unit + sets + 2 * (size_t)i, n->setData[i], 2);
    }
    DataFileAdd(file, kindDateNames, nullptr, unit, texts + text);
    free(unit);
    free(n->texts);
    free(n->items);
    free(n->listStarts);
    free(n->setData);
    free(n->ruleData);
    MapFree(&n->rules);
    MapFree(&n->strings);
    MapFree(&n->lists);
    MapFree(&n->sets);
}
