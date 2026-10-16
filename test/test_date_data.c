// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units (date_data.h, date_tables.h): compiled patterns
// well formed or not, a patterns unit built here and copies with one
// field spoiled, then the dates data file: patterns of a locale's
// calendars by entry and length, a region's own over its parent's, the
// ISO calendar's taken from the Gregorian, and a numbering override.

#include "bytes.h"
#include "data_view.h"
#include "date_data.h"
#include "date_tables.h"
#include "kinds.h"
#include "test_harness.h"
#include "test_pack.h"

#include "maul-locale/data.h"
#include "maul-locale/datetime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t s_unit[4096];

static void Put(uint8_t* p, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

// A pattern checked in a block of exactly its length, so a sanitizer sees
// any read past it.
static bool Valid(const char* pattern, size_t length)
{
    uint8_t* copy = malloc(length > 0 ? length : 1);
    if (copy == nullptr)
    {
        return false;
    }
    memcpy(copy, pattern, length);
    bool result = mlocValidDatePattern(copy, (uint32_t)length);
    free(copy);
    return result;
}

static void TestPatterns(void)
{
    CHECK(Valid("\x01y\x01", 3), "a field");
    CHECK(Valid("\x01M\x03 \x01"
                "d\x01, \x01y\x01",
                12),
          "fields and literals");
    CHECK(Valid("\xE5\xB9\xB4", 3), "literal UTF-8");
    CHECK(Valid("\x02y\x01\x08jpanyear\xE5\xB9\xB4", 15), "an override");
    CHECK(Valid("\x01S\x09", 3), "the widest field");
    CHECK(!Valid("", 0), "an empty pattern");
    CHECK(!Valid("\x01y\x00", 3), "width 0");
    CHECK(!Valid("\x01S\x0A", 3), "width 10");
    CHECK(!Valid("\x01j\x01", 3), "a letter no pattern holds");
    CHECK(!Valid("\x01\x00\x01", 3), "a NUL letter");
    CHECK(!Valid("\x01y", 2), "a field cut short");
    CHECK(!Valid("\x02y\x01\x00", 4), "an override's id of length 0");
    CHECK(!Valid("\x02y\x01\x09hanidecxx", 13), "an override's id of length 9");
    CHECK(!Valid("\x02y\x01\x07Hanidec", 11), "an override's id in upper case");
    CHECK(!Valid("\x02y\x01\x07hanide", 10), "an override cut short");
    CHECK(!Valid("\x02y\x01", 3), "an override without its id");
    CHECK(Valid("\x02y\x01\x01x", 5), "an override's id of one letter");
    CHECK(!Valid("a\x05", 2), "a control byte in a literal");
    CHECK(!Valid("\xC0\x80", 2), "malformed UTF-8");
}

// The sections of the patterns unit built here.
enum
{
    unitOffsets = datePatternsHeaderSize,
    unitRows = 32,
    unitSets = 40,
    unitTimes = 88,
    unitText = 128,
};

// Builds a unit of two patterns, one row, one set and one time set; its
// size.
static uint32_t BuildUnit(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    static const char s_text[] = "\x01y\x01\x01M\x01";
    Put(s_unit, 2, 4);
    Put(s_unit + 4, 1, 4);
    Put(s_unit + 8, 1, 4);
    Put(s_unit + 12, 1, 4);
    Put(s_unit + 16, 6, 4);
    Put(s_unit + unitOffsets, 0, 4);
    Put(s_unit + unitOffsets + 4, 3, 4);
    Put(s_unit + unitOffsets + 8, 6, 4);
    // One row of patterns 0, 1, 0.
    Put(s_unit + unitRows + 2, 1, 2);
    // Every entry row 0, the standard patterns 1.
    for (int i = 0; i < dateStandardCount; i++)
    {
        Put(s_unit + unitSets + 2 * dateEntryCount + 2 * i, 1, 2);
    }
    // The time set: pattern 1 for the last glue.
    Put(s_unit + unitTimes + 2 * (dateTimeEntryCount - 1), 1, 2);
    memcpy(s_unit + unitText, s_text, 6);
    return unitText + 6;
}

// A validator run on a copy of the unit's first bytes in a block of
// exactly that size, so a sanitizer sees any read past them.
static bool ValidCopy(bool (*valid)(const DataView*, DataSpan), uint32_t size)
{
    uint8_t* copy = malloc(size > 0 ? size : 1);
    if (copy == nullptr)
    {
        return false;
    }
    memcpy(copy, s_unit, size);
    bool result = valid(nullptr, (DataSpan){copy, size});
    free(copy);
    return result;
}

static bool ValidUnit(uint32_t size)
{
    return ValidCopy(mlocValidDatePatterns, size);
}

static void TestUnit(void)
{
    uint32_t size = BuildUnit();
    CHECK(ValidUnit(size), "a valid unit");
    DatePatterns p = mlocReadDatePatterns((DataSpan){s_unit, size});
    DataSpan pattern;
    CHECK(mlocDatePattern(&p, 1, &pattern) && pattern.size == 3 && pattern.bytes[1] == 'M',
          "a pattern by index");
    CHECK(!mlocDatePattern(&p, 2, &pattern), "an index past the patterns");
    CHECK(p.timeSetCount == 1 && p.timeSets == s_unit + unitTimes, "the time sets");
    CHECK(!ValidUnit(size - 1) && !ValidUnit(size + 1) && !ValidUnit(8) &&
              !ValidUnit(datePatternsHeaderSize - 1),
          "a size that is not the layout's");
    BuildUnit();
    Put(s_unit, 3, 4);
    CHECK(!ValidUnit(size), "a pattern count past the offsets");
    BuildUnit();
    Put(s_unit + 12, 2, 4);
    CHECK(!ValidUnit(size), "a time set count past the unit");
    BuildUnit();
    Put(s_unit + unitOffsets, 1, 4);
    CHECK(!ValidUnit(size), "a first offset that is not 0");
    BuildUnit();
    Put(s_unit + unitOffsets + 8, 5, 4);
    CHECK(!ValidUnit(size), "a last offset short of the text");
    BuildUnit();
    Put(s_unit + unitOffsets + 4, 7, 4);
    CHECK(!ValidUnit(size), "offsets out of order");
    BuildUnit();
    s_unit[unitText + 2] = 0;
    CHECK(!ValidUnit(size), "a malformed pattern");
    BuildUnit();
    Put(s_unit + unitRows + 4, 2, 2);
    CHECK(!ValidUnit(size), "a row's pattern past the patterns");
    BuildUnit();
    Put(s_unit + unitSets + 2 * (dateEntryCount - 1), 1, 2);
    CHECK(!ValidUnit(size), "a set's row past the rows");
    BuildUnit();
    Put(s_unit + unitSets + 2 * (dateEntryCount + dateStandardCount - 1), 2, 2);
    CHECK(!ValidUnit(size), "a set's standard pattern past the patterns");
    BuildUnit();
    Put(s_unit + unitTimes + 2 * (dateTimeEntryCount - 1), 2, 2);
    CHECK(!ValidUnit(size), "a time set's last pattern past the patterns");
    CHECK(mlocValidDates(nullptr, (DataSpan){s_unit, dateLocaleSize}) &&
              !mlocValidDates(nullptr, (DataSpan){s_unit, dateLocaleSize - 2}),
          "a locale's dates of its size");
}

// The sections of the names unit built here.
static uint32_t s_listsAt;
static uint32_t s_itemsAt;
static uint32_t s_setsAt;
static uint32_t s_rulesAt;
static uint32_t s_textAt;

enum
{
    namesLists = 7,
};

// Builds a names unit: names "", "a", "{0}x" and "b"; lists of months,
// weekdays and quarters of "a", an empty one, leap patterns of "{0}x",
// three of "{0}x" and day periods of "a"; one set of the first five and
// the day periods; one rule set, am to noon and pm after, noon named.
// Its size.
static uint32_t BuildNames(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    static const uint32_t s_sizes[namesLists] = {
        dateMonthListSize,    dateWeekdayListSize, dateQuarterListSize, 0, dateLeapListSize, 3,
        dateDayPeriodListSize};
    static const uint16_t s_names[namesLists] = {1, 1, 1, 0, 2, 2, 1};
    static const uint32_t s_offsets[5] = {0, 0, 1, 5, 6};
    Put(s_unit, 4, 4);
    Put(s_unit + 4, namesLists, 4);
    Put(s_unit + 8, 1, 4);
    Put(s_unit + 12, 1, 4);
    Put(s_unit + 16, 6, 4);
    for (uint32_t i = 0; i < 5; i++)
    {
        Put(s_unit + dateNamesHeaderSize + 4 * i, s_offsets[i], 4);
    }
    s_listsAt = (dateNamesHeaderSize + 4 * 5 + 7) / 8 * 8;
    s_itemsAt = s_listsAt + 4 * (namesLists + 1);
    uint32_t item = 0;
    for (uint32_t l = 0; l <= namesLists; l++)
    {
        Put(s_unit + s_listsAt + 4 * l, item, 4);
        for (uint32_t i = 0; l < namesLists && i < s_sizes[l]; i++)
        {
            Put(s_unit + s_itemsAt + 2 * (item + i), s_names[l], 2);
        }
        item += l < namesLists ? s_sizes[l] : 0;
    }
    s_setsAt = (s_itemsAt + 2 * item + 7) / 8 * 8;
    static const uint16_t s_set[dateNameKindCount] = {0, 1, 2, 3, 3, 4, 6};
    for (uint32_t k = 0; k < dateNameKindCount; k++)
    {
        Put(s_unit + s_setsAt + 2 * k, s_set[k], 2);
    }
    s_rulesAt = (s_setsAt + dateNameSetSize + 7) / 8 * 8;
    memset(s_unit + s_rulesAt + 12, dateDayPeriodPm, 12);
    s_unit[s_rulesAt + 24] = dateRuleNoon;
    s_textAt = (s_rulesAt + dateRuleSize + 7) / 8 * 8;
    memcpy(s_unit + s_textAt, "a{0}xb", 6);
    return s_textAt + 6;
}

static bool ValidNames(uint32_t size)
{
    return ValidCopy(mlocValidDateNames, size);
}

// Whether the unit built here is refused once a set's kind points at a
// list.
static bool RefusedWith(uint32_t kind, uint32_t list)
{
    uint32_t size = BuildNames();
    Put(s_unit + s_setsAt + 2 * kind, list, 2);
    return !ValidNames(size);
}

// Whether the unit built here is refused once its rules hold a byte.
static bool RefusedWithRule(uint32_t at, uint8_t value)
{
    uint32_t size = BuildNames();
    s_unit[s_rulesAt + at] = value;
    return !ValidNames(size);
}

static void TestNamesUnit(void)
{
    uint32_t size = BuildNames();
    CHECK(ValidNames(size), "a valid names unit");
    DateNameTables t = mlocReadDateNames((DataSpan){s_unit, size});
    DataSpan name = mlocDateName(&t, mlocDateNameList(&t, 4), 8);
    CHECK(name.size == 4 && memcmp(name.bytes, "{0}x", 4) == 0, "a name by list and item");
    CHECK(t.ruleCount == 1 && t.rules == s_unit + s_rulesAt, "the rules");
    CHECK(!ValidNames(size - 1) && !ValidNames(size + 1) && !ValidNames(12) &&
              !ValidNames(dateNamesHeaderSize),
          "a size that is not the layout's");
    BuildNames();
    Put(s_unit + 4, 1000, 4);
    CHECK(!ValidNames(size), "a list count past the unit");
    BuildNames();
    Put(s_unit + 12, 2, 4);
    CHECK(!ValidNames(size), "a rule count past the unit");
    BuildNames();
    Put(s_unit + dateNamesHeaderSize + 8, 6, 4);
    CHECK(!ValidNames(size), "string offsets out of order");
    BuildNames();
    Put(s_unit + dateNamesHeaderSize, 1, 4);
    CHECK(!ValidNames(size), "a first string offset that is not 0");
    BuildNames();
    Put(s_unit + s_itemsAt, 4, 2);
    CHECK(!ValidNames(size), "an item past the names");
    BuildNames();
    s_unit[s_textAt] = 0x05;
    CHECK(!ValidNames(size), "a control byte in a name");
    CHECK(!RefusedWith(dateNamesLeap, 3), "no leap patterns");
    CHECK(RefusedWith(dateNamesMonths, 1), "months of the weekdays' size");
    CHECK(RefusedWith(dateNamesWeekdays, 2), "weekdays of the quarters' size");
    CHECK(RefusedWith(dateNamesQuarters, 3), "no quarters");
    CHECK(RefusedWith(dateNamesEras, 0), "more eras than dateMaxEras");
    CHECK(!RefusedWith(dateNamesEras, 5), "three eras of one width");
    CHECK(RefusedWith(dateNamesCyclic, 2), "cyclic years of the quarters' size");
    CHECK(RefusedWith(dateNamesLeap, 5), "three leap patterns");
    CHECK(RefusedWith(dateNamesDayPeriods, 3), "no day periods");
    CHECK(RefusedWith(dateNamesDayPeriods, 0), "day periods of the months' size");
    CHECK(RefusedWith(dateNamesMonths, namesLists), "a list past the lists");
    CHECK(!RefusedWithRule(23, dateDayPeriodNight2), "the last period an hour may have");
    CHECK(RefusedWithRule(23, dateDayPeriodCount), "a period past the periods");
    CHECK(RefusedWithRule(0, dateDayPeriodMidnight) && RefusedWithRule(12, dateDayPeriodNoon),
          "noon or midnight for a whole hour");
    CHECK(!RefusedWithRule(24, dateRuleNoon | dateRuleMidnight) &&
              RefusedWithRule(24, (dateRuleNoon | dateRuleMidnight) + 1),
          "the rules' flags");
}

// A root pack of the patterns and names units built here and a dates
// unit.
static bool OpenDatesPack(const uint8_t dates[dateLocaleSize], mlocData sources[2])
{
    alignas(8) static uint8_t s_patterns[256];
    alignas(8) static uint8_t s_names[1024];
    uint32_t patterns = BuildUnit();
    memcpy(s_patterns, s_unit, patterns);
    uint32_t names = BuildNames();
    memcpy(s_names, s_unit, names);
    PackUnit units[] = {{kindDatePatterns, {s_patterns, patterns}, false},
                        {kindDates, {dates, dateLocaleSize}, true},
                        {kindDateNames, {s_names, names}, false}};
    mlocOpenBuiltinData(&sources[0]);
    return OpenRootPack(units, 3, &sources[1]);
}

// The dates unit names name set 0 and time set 0 for the Gregorian
// calendar, set 1 of each, past the units' one, for the Buddhist, and
// rules 0, then rules 1, past the names unit's one.
static void TestNamesPastUnit(void)
{
    static uint8_t s_dates[dateLocaleSize];
    Put(s_dates + 2 * (dateCalendarCount + 2), 1, 2);
    Put(s_dates + 2 * (2 * dateCalendarCount + 2), 1, 2);
    mlocData sources[2];
    CHECK(OpenDatesPack(s_dates, sources), "a pack of date units and names");
    mlocDataSet data = {sources, 2};
    mlocLocale fr;
    DateNameTables t;
    DatePatterns p;
    const uint8_t* set = nullptr;
    const uint8_t* rules = nullptr;
    CHECK(mlocParseLocale("fr", 2, &fr).status == mloc_success &&
              mlocFindDateNames(&data, &fr, 0, &t, &set) && set == t.sets,
          "root's Gregorian names");
    CHECK(!mlocFindDateNames(&data, &fr, 2, &t, &set), "a name set past the unit's");
    CHECK(mlocFindTimeSet(&data, &fr, 0, &p, &set) && set == p.timeSets, "root's Gregorian times");
    CHECK(!mlocFindTimeSet(&data, &fr, 2, &p, &set) &&
              !mlocFindTimeSet(&data, &fr, dateCalendarCount, &p, &set),
          "a time set past the unit's, a calendar past MlocCalendar's");
    CHECK(mlocFindDayPeriodRules(&data, &fr, &rules) && rules[24] == dateRuleNoon,
          "root's day period rules");
    Put(s_dates + 6 * dateCalendarCount, 1, 2);
    CHECK(OpenDatesPack(s_dates, sources) && !mlocFindDayPeriodRules(&data, &fr, &rules),
          "rules past the unit's");
}

// A formatter refuses a pattern with a field it does not write ("Q",
// which a compiled pattern may hold but CLDR's date patterns do not).
static void TestUnwritablePattern(void)
{
    alignas(8) static uint8_t s_patterns[256];
    uint32_t patterns = BuildUnit();
    s_unit[unitText + 1] = 'Q';
    memcpy(s_patterns, s_unit, patterns);
    uint32_t names = BuildNames();
    uint8_t dates[dateLocaleSize] = {0};
    PackUnit units[] = {{kindDatePatterns, {s_patterns, patterns}, false},
                        {kindDates, {dates, sizeof(dates)}, true},
                        {kindDateNames, {s_unit, names}, false}};
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    CHECK(OpenRootPack(units, 3, &sources[1]), "a pack with a quarter in a pattern");
    mlocDataSet data = {sources, 2};
    mlocLocale fr;
    mlocDateFormatter formatter;
    CHECK(mlocParseLocale("fr", 2, &fr).status == mloc_success &&
              mlocInitDateFormatter(&formatter, &data, &fr, nullptr) == mloc_errorData,
          "a pattern the formatter cannot write");
    mlocDateOptions month = {.fields = mloc_dateFieldSetMonth, .length = mloc_dateLengthMedium};
    CHECK(mlocInitDateFormatter(&formatter, &data, &fr, &month) == mloc_success,
          "another of the pack's patterns");
}

static mlocData s_sources[2];
static mlocDataSet s_data;

// The pattern of a locale's calendar at a set entry (or the standard
// formats after the entries) and length, compared with bytes.
static bool PatternIs(const char* id, uint32_t calendar, uint32_t entry, uint32_t length,
                      const char* want, size_t wantLength)
{
    mlocLocale locale;
    DatePatterns p;
    const uint8_t* set = nullptr;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        !mlocFindDateSet(&s_data, &locale, calendar, &p, &set))
    {
        return false;
    }
    uint32_t index = mlocLoad16(set + 2 * (size_t)entry);
    if (entry < dateEntryCount)
    {
        index = mlocLoad16(p.rows + (size_t)index * dateRowSize + 2 * (size_t)length);
    }
    DataSpan pattern;
    return mlocDatePattern(&p, index, &pattern) && pattern.size == wantLength &&
           memcmp(pattern.bytes, want, wantLength) == 0;
}

#define PATTERN(id, calendar, entry, length, text)                                                 \
    PatternIs(id, calendar, entry, length, text, sizeof(text) - 1)

// A locale's time pattern of a calendar at a time set entry, compared
// with bytes.
static bool TimePatternIs(const char* id, uint32_t calendar, uint32_t entry, const char* want,
                          size_t wantLength)
{
    mlocLocale locale;
    DatePatterns p;
    const uint8_t* set = nullptr;
    DataSpan pattern;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocFindTimeSet(&s_data, &locale, calendar, &p, &set) &&
           mlocDatePattern(&p, mlocLoad16(set + 2 * (size_t)entry), &pattern) &&
           pattern.size == wantLength && memcmp(pattern.bytes, want, wantLength) == 0;
}

#define TIME_PATTERN(id, calendar, entry, text)                                                    \
    TimePatternIs(id, calendar, entry, text, sizeof(text) - 1)

static void TestTimes(void)
{
    CHECK(TIME_PATTERN("en", 0, dateTimeClock12 + 1,
                       "\x01h\x01:\x01m\x02\xE2\x80\xAF\x01"
                       "a\x01"),
          "en's 12-hour minutes");
    CHECK(TIME_PATTERN("en", 0, dateTimeClock24 + 2, "\x01H\x02:\x01m\x02:\x01s\x02"),
          "en's 24-hour seconds");
    CHECK(TIME_PATTERN("en", 0, dateTimeClock12,
                       "\x01h\x01\xE2\x80\xAF\x01"
                       "a\x01"),
          "en's 12-hour hour");
    CHECK(TIME_PATTERN("ja", 0, dateTimeClock12 + 1,
                       "\x01"
                       "a\x01\x01K\x01:\x01m\x02"),
          "ja's 12-hour clock from 0");
    CHECK(TIME_PATTERN("zh_Hant", 0, dateTimeStandard + 3,
                       "\x01"
                       "B\x01\x01h\x01:\x01m\x02"),
          "zh_Hant's short time with a flexible day period");
    CHECK(TIME_PATTERN("am", 0, dateTimeClock12 + 1,
                       "\x01"
                       "a\x01 \x01h\x01:\x01m\x02") &&
              TIME_PATTERN("am", 16, dateTimeClock12 + 1,
                           "\x01h\x01:\x01m\x02 \x01"
                           "a\x01"),
          "am's Gregorian times and its Chinese ones, root's by inheritance");
    CHECK(TIME_PATTERN("en", 0, dateTimeGlue + 2, "{1}, {0}") &&
              TIME_PATTERN("en", 0, dateTimeAtGlue + 0, "{1} at {0}"),
          "en's glue, standard and atTime");
    CHECK(TIME_PATTERN("en", 1, dateTimeClock24 + 1, "\x01H\x02:\x01m\x02"),
          "the ISO calendar takes the Gregorian times");
}

static void TestFile(void)
{
    CHECK(PATTERN("en", 0, dateYearMonthDay, 1,
                  "\x01M\x03 \x01"
                  "d\x01, \x01y\x01"),
          "en's medium year, month and day");
    CHECK(PATTERN("en", 0, dateYearMonthDay, 2,
                  "\x01M\x01/\x01"
                  "d\x01/\x01y\x02"),
          "en's short one, two-digit year");
    CHECK(PATTERN("en", 0, dateYearMonthDay + dateStyleFull, 2,
                  "\x01M\x01/\x01"
                  "d\x01/\x01y\x01"),
          "its full year");
    CHECK(PATTERN("en", 0, dateYearMonthDay + dateStyleEra, 2,
                  "\x01M\x01/\x01"
                  "d\x01/\x01y\x01 \x01G\x01"),
          "its era");
    CHECK(PATTERN("en", 0, dateMonth, 0, "\x01L\x04"), "a lone month, long");
    CHECK(PATTERN("en", 0, dateWeekday, 2,
                  "\x01"
                  "c\x05"),
          "a lone weekday, short");
    CHECK(PATTERN("en", 0, dateEntryCount + 3, 0,
                  "\x01M\x01/\x01"
                  "d\x01/\x01y\x02"),
          "en's standard short date");
    CHECK(PATTERN("en", 1, dateYearMonthDay, 1,
                  "\x01M\x03 \x01"
                  "d\x01, \x01y\x01"),
          "the ISO calendar takes the Gregorian patterns");
    CHECK(PATTERN("en_GB", 0, dateYearMonthDay, 2,
                  "\x01"
                  "d\x02/\x01M\x02/\x01y\x01"),
          "en_GB's own short date");
    CHECK(PATTERN("ja", 4, dateYearMonthDay, 0,
                  "\x01G\x01\x02y\x01\x08jpanyear\xE5\xB9\xB4\x01M\x01\xE6\x9C\x88\x01"
                  "d\x01\xE6\x97\xA5"),
          "ja's Japanese calendar writes its year by jpanyear");
    mlocLocale en;
    DatePatterns p;
    const uint8_t* set = nullptr;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              !mlocFindDateSet(&s_data, &en, dateCalendarCount, &p, &set),
          "a calendar past MlocCalendar's");
}

// A root pack of the unit built here and a dates unit naming set 0 for
// the Gregorian calendar and set 1, past the unit's one set, for the
// Buddhist.
static void TestSetPastUnit(void)
{
    uint32_t size = BuildUnit();
    uint8_t dates[dateLocaleSize] = {0};
    Put(dates + 4, 1, 2);
    PackUnit units[] = {{kindDatePatterns, {s_unit, size}, false},
                        {kindDates, {dates, sizeof(dates)}, true}};
    mlocData sources[2];
    mlocOpenBuiltinData(&sources[0]);
    CHECK(OpenRootPack(units, 2, &sources[1]), "a pack of date units");
    mlocDataSet data = {sources, 2};
    mlocLocale fr;
    DatePatterns p;
    const uint8_t* set = nullptr;
    CHECK(mlocParseLocale("fr", 2, &fr).status == mloc_success &&
              mlocFindDateSet(&data, &fr, 0, &p, &set) && set == p.sets,
          "root's Gregorian set");
    CHECK(!mlocFindDateSet(&data, &fr, 2, &p, &set), "a set past the unit's");
}

// A locale's name of a calendar's list kind at an item, compared with
// text.
static bool NameIs(const char* id, uint32_t calendar, uint32_t kind, uint32_t item,
                   const char* want)
{
    mlocLocale locale;
    DateNameTables t;
    const uint8_t* set = nullptr;
    if (mlocParseLocale(id, strlen(id), &locale).status != mloc_success ||
        !mlocFindDateNames(&s_data, &locale, calendar, &t, &set))
    {
        return false;
    }
    DateNameList list = mlocDateNameList(&t, mlocLoad16(set + 2 * (size_t)kind));
    DataSpan name = mlocDateName(&t, list, item);
    return name.size == strlen(want) && memcmp(name.bytes, want, name.size) == 0;
}

// Months by context, width (abbreviated, wide, narrow) and month.
static uint32_t Month(uint32_t context, uint32_t width, uint32_t month)
{
    return (context * 3 + width) * dateMonthCount + month - 1;
}

static void TestNames(void)
{
    CHECK(NameIs("en", 0, dateNamesMonths, Month(0, 1, 1), "January"), "en's wide January");
    CHECK(NameIs("en", 0, dateNamesMonths, Month(1, 2, 9), "S"), "a narrow stand-alone month");
    CHECK(NameIs("en", 0, dateNamesMonths, Month(0, 1, 13), ""), "no thirteenth month");
    CHECK(NameIs("ru", 0, dateNamesMonths, Month(0, 1, 1), "января") &&
              NameIs("ru", 0, dateNamesMonths, Month(1, 1, 1), "январь"),
          "ru's format and stand-alone January");
    CHECK(NameIs("en", 10, dateNamesMonths, Month(0, 1, 7), "Adar") &&
              NameIs("en", 10, dateNamesMonths, Month(0, 1, 14), "Adar II"),
          "the Hebrew leap year's Adar II");
    CHECK(NameIs("en", 0, dateNamesWeekdays, (0 * 4 + 3) * 7 + 1, "Mo"), "a short weekday");
    CHECK(NameIs("en", 0, dateNamesQuarters, (0 * 3 + 1) * 4 + 1, "2nd quarter"), "a wide quarter");
    CHECK(NameIs("en", 0, dateNamesEras, 0 * 2 + 1, "AD") &&
              NameIs("en", 0, dateNamesEras, 1 * 2 + 0, "Before Christ"),
          "en's Gregorian eras");
    CHECK(NameIs("ja", 4, dateNamesEras, 1 * 5 + 4, "令和"), "ja's Reiwa");
    CHECK(NameIs("en", 1, dateNamesEras, 0 * 2 + 1, "AD"), "the ISO calendar's Gregorian eras");
    CHECK(NameIs("zh", 16, dateNamesCyclic, 1 * 60 + 0, "甲子"), "the Chinese cycle's first year");
    CHECK(NameIs("zh", 16, dateNamesLeap, 0 * 3 + 1, "闰{0}"), "zh's leap month pattern");
    mlocLocale en;
    DateNameTables t;
    const uint8_t* set = nullptr;
    CHECK(mlocParseLocale("en", 2, &en).status == mloc_success &&
              mlocFindDateNames(&s_data, &en, 0, &t, &set) &&
              mlocDateNameList(&t, mlocLoad16(set + 2 * dateNamesCyclic)).count == 0 &&
              mlocDateNameList(&t, mlocLoad16(set + 2 * dateNamesLeap)).count == 0 &&
              mlocDateName(&t, mlocDateNameList(&t, mlocLoad16(set)), dateMonthListSize).size == 0,
          "no cyclic years or leap patterns in the Gregorian calendar, nothing past a list");
    CHECK(!mlocFindDateNames(&s_data, &en, dateCalendarCount, &t, &set),
          "names of a calendar past MlocCalendar's");
}

// Day periods by context, width (abbreviated, wide, narrow) and period.
static uint32_t Period(uint32_t context, uint32_t width, uint32_t period)
{
    return (context * 3 + width) * dateDayPeriodCount + period;
}

// A locale's day period rules: the period of an hour and the flags.
static bool RulesAre(const char* id, uint32_t hour, uint32_t period, uint32_t flags)
{
    mlocLocale locale;
    const uint8_t* rules = nullptr;
    return mlocParseLocale(id, strlen(id), &locale).status == mloc_success &&
           mlocFindDayPeriodRules(&s_data, &locale, &rules) && rules[hour] == period &&
           rules[24] == flags;
}

static void TestDayPeriods(void)
{
    CHECK(NameIs("en", 0, dateNamesDayPeriods, Period(0, 0, dateDayPeriodAm), "AM") &&
              NameIs("en", 0, dateNamesDayPeriods, Period(0, 1, dateDayPeriodNoon), "noon"),
          "en's AM and noon");
    CHECK(NameIs("zh_Hant", 0, dateNamesDayPeriods, Period(0, 0, dateDayPeriodEvening1), "晚上"),
          "zh_Hant's evening");
    CHECK(NameIs("en", 4, dateNamesDayPeriods, Period(1, 2, dateDayPeriodPm), "PM"),
          "the Japanese calendar's day periods, the Gregorian ones by root's alias");
    CHECK(RulesAre("en", 13, dateDayPeriodAfternoon1, dateRuleNoon | dateRuleMidnight) &&
              RulesAre("en_GB", 21, dateDayPeriodNight1, dateRuleNoon | dateRuleMidnight),
          "en's rules, en_GB's by its language");
    CHECK(RulesAre("zh_Hant", 21, dateDayPeriodEvening1, dateRuleMidnight) &&
              RulesAre("zh_Hant", 12, dateDayPeriodAfternoon1, dateRuleMidnight),
          "zh_Hant's rules are zh's, by truncation");
    CHECK(RulesAre("yo", 13, dateDayPeriodPm, 0), "root's am and pm where the language has none");
}

static bool OpenFile(const char* path, uint8_t* buffer, size_t capacity, mlocData* out)
{
    FILE* file = fopen(path, "rb");
    size_t read = file == nullptr ? 0 : fread(buffer, 1, capacity, file);
    if (file != nullptr)
    {
        fclose(file);
    }
    return read > 0 && read < capacity && mlocOpenData(out, buffer, read) == mloc_success;
}

int main(void)
{
    TestPatterns();
    TestUnit();
    TestSetPastUnit();
    TestNamesUnit();
    TestNamesPastUnit();
    TestUnwritablePattern();
    alignas(8) static uint8_t s_dates[1 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 2};
    if (!OpenFile(MLOC_DATES_FILE, s_dates, sizeof(s_dates), &s_sources[1]))
    {
        fputs("test_date_data: cannot open " MLOC_DATES_FILE "\n", stderr);
        return 2;
    }
    TestFile();
    TestTimes();
    TestNames();
    TestDayPeriods();
    return s_failures == 0 ? 0 : 1;
}
