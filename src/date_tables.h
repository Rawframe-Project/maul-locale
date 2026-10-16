// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units of the dates data file (record mloc-0042),
// which the generator compiles by matching each semantic skeleton to a
// locale's patterns as ICU's DateTimePatternGenerator does. All integers
// are little-endian.
//
// Date patterns, a unit of no locale. The payload is a header (u32
// pattern count, u32 row count, u32 set count, u32 time set count, u32
// text size), then these sections, each starting on an 8-byte boundary:
//   offsets  each pattern's offset in the text (u32), count + 1 of them,
//            from 0 to the text size
//   rows     a field set's patterns at the three lengths, long, medium
//            and short (u16 pattern indexes, dateRowSize bytes)
//   sets     a calendar's date patterns in a locale (dateSetSize bytes):
//            the row index (u16) of each entry of DateSetEntry, then the
//            pattern index (u16) of the standard full, long, medium and
//            short date formats
//   time sets a calendar's time patterns in a locale (dateTimeSetSize
//            bytes): the pattern index (u16) of each entry of
//            DateTimeEntry, then its flags (u16)
//   text     the compiled patterns
// A compiled pattern is literal UTF-8 text (no byte below 0x20, quotes
// already resolved) and field items: datePatternField, the field's
// letter (one of datePatternLetters) and its width (1 to 9); or
// datePatternOverride, the letter, the width, the length (1 to 8) and
// the lower-case id of the numbering system that writes the field
// (CLDR's numbers="hanidec", "d=hanidays" and the like, which ICU4X
// carries to a length's semantic patterns).
//
// Date names, a unit of no locale. The payload is a header (u32 string
// count, u32 list count, u32 set count, u32 rule count, u32 text size),
// then these sections, each starting on an 8-byte boundary:
//   strings  each name's offset in the text (u32), count + 1 of them,
//            from 0 to the text size
//   lists    each list's offset in the list items (u32, in items), count
//            + 1 of them; then the items, string indexes (u16)
//   sets     a calendar's names in a locale (dateNameSetSize bytes): the
//            list index (u16) of each kind of DateNameKind
//   rules    a locale's day period rules (dateRuleSize bytes): the
//            DateDayPeriod of each hour from 0 to 23 (am, pm or a
//            flexible one), then flags: dateRuleNoon when the rules name
//            noon at 12:00, dateRuleMidnight when they name midnight
//   text     the names, UTF-8 without bytes below 0x20 (empty for a name
//            CLDR does not give)
// A list holds, in order: months, by context (format, stand-alone), width
// (abbreviated, wide, narrow) and month 1 to 13, then the Hebrew leap
// year's month 7; weekdays, by context, width (abbreviated, wide,
// narrow, short) and day from Sunday; quarters, by context and width as
// the months; eras, by width and the calendar's eras in the order of
// dateEraCodes; cyclic years, by width and year 1 to 60 (empty for a
// calendar without them); leap month patterns, by context (format,
// stand-alone, numeric) and width, each holding "{0}" as the generator
// writes them, which their user checks (empty for a calendar without
// them); day periods, by context (format, stand-alone), width
// (abbreviated, wide, narrow) and DateDayPeriod.
//
// A locale's dates, a unit per locale filed where the resolved data
// differs from its chain's: the pattern set index (u16) of each
// calendar, in the order of MlocCalendar (the ISO calendar's is the
// Gregorian one's), then the name set index (u16) of each calendar, the
// time set index (u16) of each calendar, and the index (u16) of the
// locale's day period rules in the names unit.

#ifndef MLOC_DATE_TABLES_H
#define MLOC_DATE_TABLES_H

// The date entries of a set: the field sets shown without a year, then
// those with a year in each year style (the standard pattern, the full
// year, the year with its era).
typedef enum DateSetEntry
{
    dateDay = 0,
    dateWeekday = 1,
    dateDayWeekday = 2,
    dateMonthDay = 3,
    dateMonthDayWeekday = 4,
    dateMonth = 5,
    dateYearMonthDay = 6,
    dateYearMonthDayWeekday = 9,
    dateYear = 12,
    dateYearMonth = 15,
    dateEntryCount = 18,
} DateSetEntry;

// The time entries of a time set: the hour, minute and second patterns
// of the 12-hour and 24-hour clocks as matched, the standard full, long,
// medium and short time formats, then the standard and atTime date-time
// glue at those lengths ("{1}, {0}", "{1} 'at' {0}"), the minutes and
// seconds of each clock as ICU matches them for a standard format on
// another clock (hmm, hmmss, Hmm, Hmmss); then flags, not a pattern:
// dateTimeOwnTwelve when the locale's own hour (UTS #35's j, by its
// region) is on the 12-hour clock.
typedef enum DateTimeEntry
{
    dateTimeClock12 = 0,
    dateTimeClock24 = 3,
    dateTimeStandard = 6,
    dateTimeGlue = 10,
    dateTimeAtGlue = 14,
    dateTimeStyle12 = 18,
    dateTimeStyle24 = 20,
    dateTimePatternCount = 22,
    dateTimeFlags = 22,
    dateTimeEntryCount = 23,
} DateTimeEntry;

// The day periods, as CLDR names them.
typedef enum DateDayPeriod
{
    dateDayPeriodAm = 0,
    dateDayPeriodPm = 1,
    dateDayPeriodMidnight = 2,
    dateDayPeriodNoon = 3,
    dateDayPeriodMorning1 = 4,
    dateDayPeriodMorning2 = 5,
    dateDayPeriodAfternoon1 = 6,
    dateDayPeriodAfternoon2 = 7,
    dateDayPeriodEvening1 = 8,
    dateDayPeriodEvening2 = 9,
    dateDayPeriodNight1 = 10,
    dateDayPeriodNight2 = 11,
    dateDayPeriodCount = 12,
} DateDayPeriod;

enum
{
    datePatternsHeaderSize = 20,
    // Added to a year entry for the year style.
    dateStyleFull = 1,
    dateStyleEra = 2,
    dateLengthCount = 3,
    dateRowSize = 2 * dateLengthCount,
    dateStandardCount = 4,
    dateSetSize = 2 * (dateEntryCount + dateStandardCount),
    dateTimeSetSize = 2 * dateTimeEntryCount,
    dateTimeOwnTwelve = 1,
    dateCalendarCount = 18,
    dateLocaleSize = 6 * dateCalendarCount + 2,
    dateNamesHeaderSize = 20,
    dateMonthCount = 14,
    dateMonthListSize = 2 * 3 * dateMonthCount,
    dateWeekdayListSize = 2 * 4 * 7,
    dateQuarterListSize = 2 * 3 * 4,
    dateCyclicListSize = 3 * 60,
    dateLeapListSize = 3 * 3,
    dateDayPeriodListSize = 2 * 3 * dateDayPeriodCount,
    dateRuleSize = 25,
    dateRuleNoon = 1,
    dateRuleMidnight = 2,
    dateMaxEras = 8,
    datePatternField = 1,
    datePatternOverride = 2,
    dateMaxWidth = 9,
    dateMaxSystemId = 8,
};

// The kinds of list in a name set.
typedef enum DateNameKind
{
    dateNamesMonths = 0,
    dateNamesWeekdays = 1,
    dateNamesQuarters = 2,
    dateNamesEras = 3,
    dateNamesCyclic = 4,
    dateNamesLeap = 5,
    dateNamesDayPeriods = 6,
    dateNameKindCount = 7,
} DateNameKind;

enum
{
    dateNameSetSize = 2 * dateNameKindCount,
};

// Each calendar's era codes in the order of its era names, by
// MlocCalendar, as CLDR's calendarData gives them (the Chinese and Dangi
// calendars have one era without a code).
#define dateEraCodes                                                                               \
    {"bce ce", "bce ce", "be",    "broc roc", "meiji taisho showa heisei reiwa",                   \
     "am",     "aa am",  "aa",    "shaka",    "ap",                                                \
     "am",     "ah bh",  "ah bh", "ah bh",    "ah bh",                                             \
     "ah bh",  "",       ""}

// The field letters a compiled pattern may hold.
#define datePatternLetters "GyYuUrQqMLlwWdDFgEecabBhHKkmsSAzZOvVXx"

#endif
