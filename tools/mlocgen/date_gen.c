// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units (date_gen.h). For each locale file and calendar
// the pattern generator is loaded as ICU loads it (date_source.h), each
// semantic date skeleton mapped and matched at each length and year
// style, the standard date formats read, and the time set made
// (time_gen.h); patterns, rows, sets and time sets are kept once each.
// A locale whose sets match those of the nearest locale with another key
// along its chain files nothing; one under its parent's key is checked to
// match.

#include "date_gen.h"

#include "date_names_gen.h"
#include "date_source.h"
#include "date_tables.h"
#include "interval_gen.h"
#include "kinds.h"
#include "semantic.h"
#include "skeleton.h"
#include "table_gen.h"
#include "time_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
    maxKey = 64,
    maxCompiled = 512,
};

// The calendars by MlocCalendar; the ISO calendar (1) takes the
// Gregorian data.
static const char* const s_calendars[dateCalendarCount] = {
    "gregorian",    "gregorian",           "buddhist", "roc",          "japanese", "coptic",
    "ethiopic",     "ethiopic-amete-alem", "indian",   "persian",      "hebrew",   "islamic-civil",
    "islamic-tbla", "islamic-umalqura",    "islamic",  "islamic-rgsa", "chinese",  "dangi"};

// The field sets of DateSetEntry, as semantic.h's bits, and whether each
// shows a year (three year styles).
static const unsigned s_fields[] = {
    semanticDay,
    semanticWeekday,
    semanticDay | semanticWeekday,
    semanticMonth | semanticDay,
    semanticMonth | semanticDay | semanticWeekday,
    semanticMonth,
    semanticYear | semanticMonth | semanticDay,
    semanticYear | semanticMonth | semanticDay | semanticWeekday,
    semanticYear,
    semanticYear | semanticMonth,
};

// Records of u16 values kept once each, by index.
typedef struct RecordTable
{
    Map map;
    uint16_t* data;
    int count;
    int capacity;
} RecordTable;

enum
{
    intervalSets = 0,
    intervalDateParts = 1,
    intervalRows = 2,
    intervalTimeParts = 3,
    intervalPrefixes = 4,
    intervalPairs = 5,
    intervalTableCount = 6,
};

// Each interval table's record size in bytes.
static const int s_intervalSizes[intervalTableCount] = {
    dateIntervalSetSize,  dateIntervalDateSize,   dateIntervalRowSize,
    dateIntervalTimeSize, dateIntervalPrefixSize, dateIntervalPairSize};

typedef struct DateBuilder
{
    Map patterns;
    char** texts;
    size_t* lengths;
    int patternCount;
    Map rows;
    uint16_t* rowData;
    int rowCount;
    Map sets;
    uint16_t* setData;
    int setCount;
    Map timeSets;
    uint16_t* timeSetData;
    int timeSetCount;
    // The interval sections: sets, date parts, rows, time parts, prefixes
    // and pairs, in their order in the unit.
    RecordTable intervals[intervalTableCount];
    int capacity;
} DateBuilder;

typedef struct DateLocale
{
    uint16_t sets[dateCalendarCount];
    uint16_t names[dateCalendarCount];
    uint16_t times[dateCalendarCount];
    uint16_t intervals[dateCalendarCount];
    uint16_t rules;
} DateLocale;

// Whether a numbering override writes a field: an override of the whole
// pattern writes every numeric year, month and day field, one bound to a
// symbol ("d=hanidays") that symbol's alone, as ICU4X applies them.
static bool OverrideApplies(const char* numbers, char letter, int width)
{
    if (numbers == nullptr)
    {
        return false;
    }
    bool numeric =
        strchr("yYurdDFg", letter) != nullptr || (strchr("ML", letter) != nullptr && width <= 2);
    if (!numeric)
    {
        return false;
    }
    if (numbers[0] != '\0' && numbers[1] == '=')
    {
        return letter == numbers[0];
    }
    return true;
}

static const char* SystemOf(const char* numbers)
{
    return numbers[0] != '\0' && numbers[1] == '=' ? numbers + 2 : numbers;
}

static bool AppendField(const char* numbers, char letter, int width, char* out, size_t* n,
                        size_t capacity)
{
    if (strchr(datePatternLetters, letter) == nullptr || width > dateMaxWidth)
    {
        return false;
    }
    bool override = OverrideApplies(numbers, letter, width);
    const char* system = override ? SystemOf(numbers) : "";
    size_t systemLength = strlen(system);
    size_t need = 3 + (override ? 1 + systemLength : 0);
    if (*n + need >= capacity || systemLength > dateMaxSystemId)
    {
        return false;
    }
    out[(*n)++] = (char)(override ? datePatternOverride : datePatternField);
    out[(*n)++] = letter;
    out[(*n)++] = (char)width;
    if (override)
    {
        out[(*n)++] = (char)systemLength;
        memcpy(out + *n, system, systemLength);
        *n += systemLength;
    }
    return true;
}

size_t DateCompilePattern(const char* pattern, const char* numbers, char* out, size_t capacity)
{
    size_t n = 0;
    bool quoted = false;
    for (const char* p = pattern; *p != '\0';)
    {
        char c = *p;
        if (c == '\'')
        {
            if (p[1] == '\'')
            {
                c = '\'';
                p += 2;
            }
            else
            {
                quoted = !quoted;
                p++;
                continue;
            }
        }
        else if (!quoted && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        {
            int width = 0;
            while (p[width] == c)
            {
                width++;
            }
            if (!AppendField(numbers, c, width, out, &n, capacity))
            {
                return 0;
            }
            p += width;
            continue;
        }
        else
        {
            p++;
        }
        if ((unsigned char)c < 0x20 || n + 1 >= capacity)
        {
            return 0;
        }
        out[n++] = c;
    }
    out[n] = '\0';
    return quoted ? 0 : n;
}

static void Grow(DateBuilder* b)
{
    if (b->patternCount < b->capacity && b->rowCount < b->capacity && b->setCount < b->capacity &&
        b->timeSetCount < b->capacity)
    {
        return;
    }
    int capacity = b->capacity == 0 ? 1024 : b->capacity * 2;
    char** texts = realloc(b->texts, sizeof(char*) * (size_t)capacity);
    size_t* lengths = realloc(b->lengths, sizeof(size_t) * (size_t)capacity);
    uint16_t* rows = realloc(b->rowData, sizeof(uint16_t) * dateLengthCount * (size_t)capacity);
    uint16_t* sets = realloc(b->setData, (size_t)dateSetSize * (size_t)capacity);
    uint16_t* times = realloc(b->timeSetData, (size_t)dateTimeSetSize * (size_t)capacity);
    if (texts == nullptr || lengths == nullptr || rows == nullptr || sets == nullptr ||
        times == nullptr)
    {
        GenDie("out of memory", "date patterns");
    }
    b->texts = texts;
    b->lengths = lengths;
    b->rowData = rows;
    b->setData = sets;
    b->timeSetData = times;
    b->capacity = capacity;
}

// The index a key names in a map, added as next when new; the generator
// stops past what a u16 index holds.
static uint16_t Intern(Map* map, const char* key, int* count, bool* added)
{
    void* value = MapGet(map, key);
    *added = value == nullptr;
    if (value != nullptr)
    {
        return (uint16_t)((uintptr_t)value - 1);
    }
    if (*count >= 0xFFFF)
    {
        GenDie("too many date patterns, rows or sets", key);
    }
    MapPut(map, key, (void*)(uintptr_t)(*count + 1));
    return (uint16_t)(*count)++;
}

static uint16_t AddPattern(DateBuilder* b, const char* pattern, const char* numbers)
{
    char compiled[maxCompiled];
    size_t length = DateCompilePattern(pattern, numbers, compiled, sizeof(compiled));
    if (length == 0)
    {
        GenDie("a date pattern that cannot be compiled", pattern);
    }
    Grow(b);
    bool added = false;
    int index = b->patternCount;
    uint16_t found = Intern(&b->patterns, compiled, &b->patternCount, &added);
    if (added)
    {
        b->texts[index] = GenAllocate(length + 1);
        memcpy(b->texts[index], compiled, length);
        b->lengths[index] = length;
    }
    return found;
}

// A record's key in a map: its u16 values in hex.
static void RecordKey(const uint16_t* values, int count, char* out)
{
    for (int i = 0; i < count; i++)
    {
        snprintf(out + 4 * i, 5, "%04x", values[i]);
    }
}

static uint16_t AddRow(DateBuilder* b, const uint16_t row[dateLengthCount])
{
    char key[4 * dateLengthCount + 1];
    RecordKey(row, dateLengthCount, key);
    Grow(b);
    bool added = false;
    int index = b->rowCount;
    uint16_t found = Intern(&b->rows, key, &b->rowCount, &added);
    if (added)
    {
        memcpy(b->rowData + (size_t)index * dateLengthCount, row, dateRowSize);
    }
    return found;
}

static uint16_t AddSet(DateBuilder* b, const uint16_t* set)
{
    char key[2 * dateSetSize + 1];
    RecordKey(set, dateSetSize / 2, key);
    Grow(b);
    bool added = false;
    int index = b->setCount;
    uint16_t found = Intern(&b->sets, key, &b->setCount, &added);
    if (added)
    {
        memcpy(b->setData + (size_t)index * (dateSetSize / 2), set, dateSetSize);
    }
    return found;
}

static uint16_t AddTimeSet(DateBuilder* b, const uint16_t* set)
{
    char key[2 * dateTimeSetSize + 1];
    RecordKey(set, dateTimeEntryCount, key);
    Grow(b);
    bool added = false;
    int index = b->timeSetCount;
    uint16_t found = Intern(&b->timeSets, key, &b->timeSetCount, &added);
    if (added)
    {
        memcpy(b->timeSetData + (size_t)index * dateTimeEntryCount, set, dateTimeSetSize);
    }
    return found;
}

// The time set of a loaded generator's locale and calendar.
static uint16_t TimeSet(Cldr* cldr, const DateSupplement* supplement, PatternGenerator* g,
                        DateBuilder* b, const char* locale, const char* calendar)
{
    char* patterns[dateTimePatternCount];
    uint16_t flags = 0;
    PatternGenerator items;
    PatternGeneratorInit(&items);
    DateSourceLoadItems(cldr, supplement, locale, calendar, &items);
    TimeSetPatterns(cldr, g, &items, locale, calendar, patterns, &flags);
    PatternGeneratorFree(&items);
    uint16_t set[dateTimeEntryCount];
    for (int i = 0; i < dateTimePatternCount; i++)
    {
        set[i] = AddPattern(b, patterns[i], nullptr);
        free(patterns[i]);
    }
    set[dateTimeFlags] = flags;
    return AddTimeSet(b, set);
}

// Keeps a record once in an interval table; its index.
static uint16_t AddRecord(DateBuilder* b, int table, const uint16_t* record)
{
    RecordTable* t = &b->intervals[table];
    int values = s_intervalSizes[table] / 2;
    if (t->count == t->capacity)
    {
        t->capacity = t->capacity == 0 ? 256 : 2 * t->capacity;
        t->data = realloc(t->data, 2 * (size_t)values * (size_t)t->capacity);
        if (t->data == nullptr)
        {
            GenDie("out of memory", "date intervals");
        }
    }
    char key[4 * (dateIntervalTimeSize / 2) + 1];
    RecordKey(record, values, key);
    bool added = false;
    int index = t->count;
    uint16_t found = Intern(&t->map, key, &t->count, &added);
    if (added)
    {
        memcpy(t->data + (size_t)index * (size_t)values, record, 2 * (size_t)values);
    }
    return found;
}

static uint16_t PatternOrNone(DateBuilder* b, const char* pattern, const char* numbers)
{
    return pattern != nullptr && pattern[0] != '\0' ? AddPattern(b, pattern, numbers)
                                                    : dateIntervalNone;
}

// An interval entry's pair: each greatest difference's halves compiled.
static uint16_t AddPair(DateBuilder* b, const IntervalPatterns* p, const char* numbers)
{
    uint16_t pair[dateIntervalPairSize / 2] = {0};
    for (int f = 0; f < dateIntervalFields; f++)
    {
        const IntervalPair* half = &p->pairs[f];
        pair[2 * f] = PatternOrNone(b, half->first, numbers);
        pair[2 * f + 1] = PatternOrNone(b, half->second, numbers);
        pair[2 * dateIntervalFields] |= (uint16_t)(half->laterFirst ? 1u << f : 0u);
    }
    return AddRecord(b, intervalPairs, pair);
}

static uint16_t AddPrefix(DateBuilder* b, const IntervalPrefix* p, const char* numbers)
{
    uint16_t prefix[dateIntervalPrefixSize / 2] = {0};
    for (int level = 0; level < dateIntervalPrefixes; level++)
    {
        prefix[level] = PatternOrNone(b, p->patterns[level], numbers);
        prefix[dateIntervalPrefixes] |= (uint16_t)(p->styles[level] << (2 * level));
    }
    return AddRecord(b, intervalPrefixes, prefix);
}

// The date part: each date entry's row of pairs and prefixes by length,
// then each standard date format's pair, prefix and own pattern.
static uint16_t AddDatePart(DateBuilder* b, const IntervalSet* r,
                            const char* const numbers[dateIntervalTime])
{
    uint16_t part[dateIntervalDateSize / 2];
    for (int entry = 0; entry < dateEntryCount; entry++)
    {
        uint16_t row[dateIntervalRowSize / 2];
        for (int length = 0; length < dateLengthCount; length++)
        {
            int e = dateIntervalDate + entry * dateLengthCount + length;
            row[2 * length] = AddPair(b, &r->entries[e], numbers[e]);
            row[2 * length + 1] = AddPrefix(b, &r->prefixes[e], numbers[e]);
        }
        part[entry] = AddRecord(b, intervalRows, row);
    }
    for (int i = 0; i < dateStandardCount; i++)
    {
        int e = dateIntervalDateStyle + i;
        part[dateEntryCount + 3 * i] = AddPair(b, &r->entries[e], numbers[e]);
        part[dateEntryCount + 3 * i + 1] = AddPrefix(b, &r->prefixes[e], numbers[e]);
        part[dateEntryCount + 3 * i + 2] = AddPattern(b, r->entries[e].pattern, numbers[e]);
    }
    return AddRecord(b, intervalDateParts, part);
}

// The time part: each time entry's pair, then each standard time
// format's pair and own pattern.
static uint16_t AddTimePart(DateBuilder* b, const IntervalSet* r)
{
    enum
    {
        times = dateIntervalTimeStyle - dateIntervalTime,
    };
    uint16_t part[dateIntervalTimeSize / 2];
    for (int t = 0; t < times; t++)
    {
        part[t] = AddPair(b, &r->entries[dateIntervalTime + t], nullptr);
    }
    for (int i = 0; i < dateStandardCount; i++)
    {
        const IntervalPatterns* p = &r->entries[dateIntervalTimeStyle + i];
        part[times + 2 * i] = AddPair(b, p, nullptr);
        part[times + 2 * i + 1] = AddPattern(b, p->pattern, nullptr);
    }
    return AddRecord(b, intervalTimeParts, part);
}

// A calendar's interval set: its date and time parts, the fallback and
// the short dates a time's date differences write.
static uint16_t AddIntervalSet(DateBuilder* b, const IntervalSet* r,
                               const char* const numbers[dateIntervalTime])
{
    uint16_t set[dateIntervalSetSize / 2] = {
        AddDatePart(b, r, numbers),
        AddTimePart(b, r),
        AddPattern(b, r->fallback, nullptr),
        AddPattern(b, r->shortDate, nullptr),
        AddPattern(b, r->shortEraDate, nullptr),
    };
    return AddRecord(b, intervalSets, set);
}

// A calendar's datetimeSkeleton and numbering override at each length,
// read once.
typedef struct LengthData
{
    const char* skeleton[dateLengthCount];
    const char* numbers[dateLengthCount];
} LengthData;

// The semantic pattern of a field set at a length and year style; its
// classical skeleton into classical.
static uint16_t SemanticPattern(PatternGenerator* g, DateBuilder* b, const LengthData* lengths,
                                const char* locale, unsigned fields, int length, int style,
                                char classical[64])
{
    SemanticSkeleton s = {
        .fields = fields, .length = (SemanticLength)length, .yearStyle = (SemanticYearStyle)style};
    if (!SemanticClassical(&s, lengths->skeleton[length], classical, 64))
    {
        GenDie("a date field set UTS #35 does not define", locale);
    }
    char* pattern = PatternGeneratorBest(g, classical);
    // A length's override applies to its semantic patterns (ICU4X).
    uint16_t index = AddPattern(b, pattern, lengths->numbers[length]);
    free(pattern);
    return index;
}

// A calendar's interval set: the date entries' classical skeletons as
// matched for the date set, each length's numbering override with them.
static uint16_t CalendarIntervals(const DateInput* in, DateBuilder* b, PatternGenerator* g,
                                  const LengthData* lengths, const char* locale,
                                  const char* calendar, char classical[dateIntervalDateStyle][64])
{
    const char* dates[dateIntervalDateStyle];
    const char* numbers[dateIntervalTime] = {0};
    for (int e = 0; e < dateIntervalDateStyle; e++)
    {
        dates[e] = classical[e];
        numbers[e] = lengths->numbers[e % dateLengthCount];
    }
    for (int i = 0; i < dateStandardCount; i++)
    {
        numbers[dateIntervalDateStyle + i] = DateNumbersOf(in->cldr, locale, calendar, i);
    }
    IntervalSet* resolved = GenAllocate(sizeof(IntervalSet));
    IntervalSetResolve(in->cldr, g, locale, calendar, dates, resolved);
    uint16_t index = AddIntervalSet(b, resolved, numbers);
    IntervalSetFree(resolved);
    free(resolved);
    return index;
}

// A calendar's date set, and its time and interval sets into times and
// intervals.
static uint16_t CalendarSet(const DateInput* in, const DateSupplement* supplement, DateBuilder* b,
                            const char* locale, const char* calendar, uint16_t* times,
                            uint16_t* intervals)
{
    PatternGenerator g;
    PatternGeneratorInit(&g);
    DateSourceLoad(in->cldr, supplement, locale, calendar, &g);
    LengthData lengths;
    for (int length = 0; length < dateLengthCount; length++)
    {
        lengths.skeleton[length] = DateSkeletonOf(in->cldr, locale, calendar, length);
        lengths.numbers[length] = DateNumbersOf(in->cldr, locale, calendar, length + 1);
    }
    uint16_t set[dateSetSize / 2];
    char classical[dateIntervalDateStyle][64];
    int entry = 0;
    for (size_t f = 0; f < sizeof(s_fields) / sizeof(s_fields[0]); f++)
    {
        int styles = (s_fields[f] & semanticYear) != 0 ? 3 : 1;
        for (int style = 0; style < styles; style++)
        {
            uint16_t row[dateLengthCount];
            for (int length = 0; length < dateLengthCount; length++)
            {
                row[length] = SemanticPattern(&g, b, &lengths, locale, s_fields[f], length, style,
                                              classical[entry * dateLengthCount + length]);
            }
            set[entry++] = AddRow(b, row);
        }
    }
    for (int i = 0; i < dateStandardCount; i++)
    {
        const char* pattern = DateStandardPattern(in->cldr, locale, calendar, i);
        if (pattern == nullptr)
        {
            GenDie("a standard date format missing", locale);
        }
        set[entry++] = AddPattern(b, pattern, DateNumbersOf(in->cldr, locale, calendar, i));
    }
    *times = TimeSet(in->cldr, supplement, &g, b, locale, calendar);
    *intervals = CalendarIntervals(in, b, &g, &lengths, locale, calendar, classical);
    PatternGeneratorFree(&g);
    return AddSet(b, set);
}

// The nearest locale along a locale's chain whose key differs from its
// own, or null for root.
static const char* NextKeyed(const DateInput* in, const char* locale, const char* key)
{
    for (const char* at = CldrParent(in->cldr, locale, componentMain); at != nullptr;
         at = CldrParent(in->cldr, at, componentMain))
    {
        char other[maxKey];
        KeyOf(in->keys, at, other, sizeof(other));
        if (strcmp(other, key) != 0)
        {
            return at;
        }
    }
    return nullptr;
}

static int IndexOf(const DateInput* in, const char* id)
{
    for (int i = 0; i < in->count; i++)
    {
        if (strcmp(in->ids[i], id) == 0)
        {
            return i;
        }
    }
    GenDie("a parent locale without a file", id);
}

static size_t Align8(size_t n)
{
    return (n + 7) & ~(size_t)7;
}

static void AddPatternsUnit(const DateBuilder* b, DataFile* file)
{
    size_t text = 0;
    for (int i = 0; i < b->patternCount; i++)
    {
        text += b->lengths[i];
    }
    size_t offsets = datePatternsHeaderSize;
    size_t rows = Align8(offsets + 4 * ((size_t)b->patternCount + 1));
    size_t sets = Align8(rows + (size_t)dateRowSize * (size_t)b->rowCount);
    size_t times = Align8(sets + (size_t)dateSetSize * (size_t)b->setCount);
    size_t intervals[intervalTableCount + 1];
    intervals[0] = Align8(times + (size_t)dateTimeSetSize * (size_t)b->timeSetCount);
    for (int t = 0; t < intervalTableCount; t++)
    {
        intervals[t + 1] =
            Align8(intervals[t] + (size_t)s_intervalSizes[t] * (size_t)b->intervals[t].count);
    }
    size_t texts = intervals[intervalTableCount];
    uint8_t* unit = GenAllocate(texts + text);
    GenPut(unit, (uint64_t)b->patternCount, 4);
    GenPut(unit + 4, (uint64_t)b->rowCount, 4);
    GenPut(unit + 8, (uint64_t)b->setCount, 4);
    GenPut(unit + 12, (uint64_t)b->timeSetCount, 4);
    for (int t = 0; t < intervalTableCount; t++)
    {
        GenPut(unit + 16 + 4 * (size_t)t, (uint64_t)b->intervals[t].count, 4);
    }
    GenPut(unit + 16 + 4 * intervalTableCount, text, 4);
    size_t at = 0;
    for (int i = 0; i <= b->patternCount; i++)
    {
        GenPut(unit + offsets + 4 * (size_t)i, at, 4);
        if (i < b->patternCount)
        {
            memcpy(unit + texts + at, b->texts[i], b->lengths[i]);
            at += b->lengths[i];
        }
    }
    for (int i = 0; i < b->rowCount * dateLengthCount; i++)
    {
        GenPut(unit + rows + 2 * (size_t)i, b->rowData[i], 2);
    }
    for (int i = 0; i < b->setCount * (dateSetSize / 2); i++)
    {
        GenPut(unit + sets + 2 * (size_t)i, b->setData[i], 2);
    }
    for (int i = 0; i < b->timeSetCount * dateTimeEntryCount; i++)
    {
        GenPut(unit + times + 2 * (size_t)i, b->timeSetData[i], 2);
    }
    for (int t = 0; t < intervalTableCount; t++)
    {
        for (int i = 0; i < b->intervals[t].count * (s_intervalSizes[t] / 2); i++)
        {
            GenPut(unit + intervals[t] + 2 * (size_t)i, b->intervals[t].data[i], 2);
        }
    }
    DataFileAdd(file, kindDatePatterns, nullptr, unit, texts + text);
    free(unit);
}

static void FileLocales(const DateInput* in, const DateLocale* all, DataFile* file)
{
    for (int i = 0; i < in->count; i++)
    {
        char key[maxKey];
        KeyOf(in->keys, in->ids[i], key, sizeof(key));
        const char* parent = CldrParent(in->cldr, in->ids[i], componentMain);
        char parentKey[maxKey] = "";
        if (parent != nullptr)
        {
            KeyOf(in->keys, parent, parentKey, sizeof(parentKey));
        }
        if (parent != nullptr && strcmp(parentKey, key) == 0)
        {
            if (memcmp(&all[IndexOf(in, parent)], &all[i], sizeof(DateLocale)) != 0)
            {
                GenDie("a locale's dates differ from those of its parent's key", in->ids[i]);
            }
            continue;
        }
        const char* next = NextKeyed(in, in->ids[i], key);
        if (next == nullptr || memcmp(&all[IndexOf(in, next)], &all[i], sizeof(DateLocale)) != 0)
        {
            uint8_t unit[dateLocaleSize];
            for (int c = 0; c < dateCalendarCount; c++)
            {
                GenPut(unit + 2 * c, all[i].sets[c], 2);
                GenPut(unit + 2 * (dateCalendarCount + c), all[i].names[c], 2);
                GenPut(unit + 2 * (2 * dateCalendarCount + c), all[i].times[c], 2);
                GenPut(unit + 2 * (3 * dateCalendarCount + c), all[i].intervals[c], 2);
            }
            GenPut(unit + 8 * dateCalendarCount, all[i].rules, 2);
            DataFileAdd(file, kindDates, key, unit, sizeof(unit));
        }
    }
}

int GenerateDateUnits(const DateInput* in, DataFile* file, char* errorOut, size_t errorCapacity)
{
    DateSupplement supplement;
    if (!DateSupplementLoad(&supplement, in->commonDirectory, errorOut, errorCapacity))
    {
        return 0;
    }
    static DateBuilder s_b;
    static DateNames s_names;
    DateBuilder* b = &s_b;
    memset(b, 0, sizeof(*b));
    memset(&s_names, 0, sizeof(s_names));
    DateLocale* all = GenAllocate(sizeof(DateLocale) * (size_t)in->count);
    memset(all, 0, sizeof(DateLocale) * (size_t)in->count);
    for (int i = 0; i < in->count; i++)
    {
        for (int c = 0; c < dateCalendarCount; c++)
        {
            // The ISO calendar (1) takes the Gregorian data.
            int from = c == 1 ? 0 : c;
            if (from == c)
            {
                all[i].sets[c] = CalendarSet(in, &supplement, b, in->ids[i], s_calendars[c],
                                             &all[i].times[c], &all[i].intervals[c]);
                all[i].names[c] =
                    DateNamesSet(&s_names, in->cldr, &supplement, in->ids[i], c, s_calendars[c]);
            }
            all[i].sets[c] = all[i].sets[from];
            all[i].names[c] = all[i].names[from];
            all[i].times[c] = all[i].times[from];
            all[i].intervals[c] = all[i].intervals[from];
        }
        all[i].rules = DateNamesRules(&s_names, &supplement, in->ids[i]);
    }
    FileLocales(in, all, file);
    AddPatternsUnit(b, file);
    DateNamesFinish(&s_names, file);
    for (int i = 0; i < b->patternCount; i++)
    {
        free(b->texts[i]);
    }
    free(b->texts);
    free(b->lengths);
    free(b->rowData);
    free(b->setData);
    free(b->timeSetData);
    for (int t = 0; t < intervalTableCount; t++)
    {
        free(b->intervals[t].data);
        MapFree(&b->intervals[t].map);
    }
    MapFree(&b->patterns);
    MapFree(&b->rows);
    MapFree(&b->sets);
    MapFree(&b->timeSets);
    free(all);
    DateSupplementFree(&supplement);
    return 1;
}
