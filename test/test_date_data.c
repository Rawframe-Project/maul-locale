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

#include <stdio.h>
#include <string.h>

static uint8_t s_unit[4096];

static void Put(uint8_t* p, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; i++)
    {
        p[i] = (uint8_t)(value >> (8 * i));
    }
}

static bool Valid(const char* pattern, size_t length)
{
    return mlocValidDatePattern((const uint8_t*)pattern, (uint32_t)length);
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

// Builds a unit of two patterns, one row and one set; its size.
static uint32_t BuildUnit(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    static const char s_text[] = "\x01y\x01\x01M\x01";
    Put(s_unit, 2, 4);
    Put(s_unit + 4, 1, 4);
    Put(s_unit + 8, 1, 4);
    Put(s_unit + 12, 6, 4);
    Put(s_unit + 16, 0, 4);
    Put(s_unit + 20, 3, 4);
    Put(s_unit + 24, 6, 4);
    // Rows at 32: one row of patterns 0, 1, 0.
    Put(s_unit + 34, 1, 2);
    // Sets at 40: every entry row 0, the standard patterns 1.
    for (int i = 0; i < dateStandardCount; i++)
    {
        Put(s_unit + 40 + 2 * dateEntryCount + 2 * i, 1, 2);
    }
    // The text at 88, the sets' end aligned to 8.
    memcpy(s_unit + 88, s_text, 6);
    return 88 + 6;
}

static bool ValidUnit(uint32_t size)
{
    return mlocValidDatePatterns(nullptr, (DataSpan){s_unit, size});
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
    CHECK(!ValidUnit(size - 1) && !ValidUnit(size + 1) && !ValidUnit(8),
          "a size that is not the layout's");
    BuildUnit();
    Put(s_unit, 3, 4);
    CHECK(!ValidUnit(size), "a pattern count past the offsets");
    BuildUnit();
    Put(s_unit + 16, 1, 4);
    CHECK(!ValidUnit(size), "a first offset that is not 0");
    BuildUnit();
    Put(s_unit + 24, 5, 4);
    CHECK(!ValidUnit(size), "a last offset short of the text");
    BuildUnit();
    Put(s_unit + 20, 7, 4);
    CHECK(!ValidUnit(size), "offsets out of order");
    BuildUnit();
    s_unit[88 + 2] = 0;
    CHECK(!ValidUnit(size), "a malformed pattern");
    BuildUnit();
    Put(s_unit + 36, 2, 2);
    CHECK(!ValidUnit(size), "a row's pattern past the patterns");
    BuildUnit();
    Put(s_unit + 40 + 2 * (dateEntryCount - 1), 1, 2);
    CHECK(!ValidUnit(size), "a set's row past the rows");
    BuildUnit();
    Put(s_unit + 40 + 2 * (dateEntryCount + dateStandardCount - 1), 2, 2);
    CHECK(!ValidUnit(size), "a set's standard pattern past the patterns");
    CHECK(mlocValidDates(nullptr, (DataSpan){s_unit, dateLocaleSize}) &&
              !mlocValidDates(nullptr, (DataSpan){s_unit, dateLocaleSize - 2}),
          "a locale's dates of its size");
}

// Builds a names unit: names "", "a" and "{0}x"; lists of months,
// weekdays and quarters of "a", an empty one and leap patterns of "{0}x";
// one set of them. Its size.
static uint32_t BuildNames(void)
{
    memset(s_unit, 0, sizeof(s_unit));
    static const uint32_t s_sizes[5] = {dateMonthListSize, dateWeekdayListSize, dateQuarterListSize,
                                        0, dateLeapListSize};
    Put(s_unit, 3, 4);
    Put(s_unit + 4, 5, 4);
    Put(s_unit + 8, 1, 4);
    Put(s_unit + 12, 5, 4);
    Put(s_unit + 20, 0, 4);
    Put(s_unit + 24, 1, 4);
    Put(s_unit + 28, 5, 4);
    uint32_t item = 0;
    for (uint32_t l = 0; l <= 5; l++)
    {
        Put(s_unit + 32 + 4 * l, item, 4);
        for (uint32_t i = 0; l < 5 && i < s_sizes[l]; i++)
        {
            Put(s_unit + 56 + 2 * (item + i), l == 4 ? 2 : 1, 2);
        }
        item += l < 5 ? s_sizes[l] : 0;
    }
    static const uint16_t s_set[dateNameKindCount] = {0, 1, 2, 3, 3, 4};
    for (uint32_t k = 0; k < dateNameKindCount; k++)
    {
        Put(s_unit + 408 + 2 * k, s_set[k], 2);
    }
    memcpy(s_unit + 424, "a{0}x", 5);
    return 429;
}

static bool ValidNames(uint32_t size)
{
    return mlocValidDateNames(nullptr, (DataSpan){s_unit, size});
}

// Whether the unit built here is refused once a set's kind points at a
// list.
static bool RefusedWith(uint32_t kind, uint32_t list)
{
    uint32_t size = BuildNames();
    Put(s_unit + 408 + 2 * kind, list, 2);
    return !ValidNames(size);
}

static void TestNamesUnit(void)
{
    uint32_t size = BuildNames();
    CHECK(ValidNames(size), "a valid names unit");
    DateNameTables t = mlocReadDateNames((DataSpan){s_unit, size});
    DataSpan name = mlocDateName(&t, mlocDateNameList(&t, 4), 8);
    CHECK(name.size == 4 && memcmp(name.bytes, "{0}x", 4) == 0, "a name by list and item");
    CHECK(!ValidNames(size - 1) && !ValidNames(size + 1) && !ValidNames(12),
          "a size that is not the layout's");
    BuildNames();
    Put(s_unit + 24, 2, 4);
    CHECK(!ValidNames(size), "string offsets out of order");
    BuildNames();
    Put(s_unit + 16, 1, 4);
    CHECK(!ValidNames(size), "a first string offset that is not 0");
    BuildNames();
    Put(s_unit + 56, 3, 2);
    CHECK(!ValidNames(size), "an item past the names");
    BuildNames();
    s_unit[424] = 0x05;
    CHECK(!ValidNames(size), "a control byte in a name");
    BuildNames();
    Put(s_unit + 56 + 2 * (dateMonthListSize + dateWeekdayListSize + dateQuarterListSize), 1, 2);
    CHECK(!ValidNames(size), "a leap pattern without {0}");
    CHECK(RefusedWith(dateNamesMonths, 1), "months of the weekdays' size");
    CHECK(RefusedWith(dateNamesWeekdays, 2), "weekdays of the quarters' size");
    CHECK(RefusedWith(dateNamesQuarters, 3), "no quarters");
    CHECK(RefusedWith(dateNamesEras, 0), "more eras than dateMaxEras");
    CHECK(RefusedWith(dateNamesCyclic, 2), "cyclic years of the quarters' size");
    CHECK(RefusedWith(dateNamesLeap, 2), "leap patterns of the quarters' size");
    CHECK(RefusedWith(dateNamesMonths, 5), "a list past the lists");
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
    alignas(8) static uint8_t s_dates[1 << 20];
    mlocOpenBuiltinData(&s_sources[0]);
    s_data = (mlocDataSet){s_sources, 2};
    if (!OpenFile(MLOC_DATES_FILE, s_dates, sizeof(s_dates), &s_sources[1]))
    {
        fputs("test_date_data: cannot open " MLOC_DATES_FILE "\n", stderr);
        return 2;
    }
    TestFile();
    TestNames();
    return s_failures == 0 ? 0 : 1;
}
