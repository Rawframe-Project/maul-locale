// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units of the dates data file, which the generator
// compiles by matching each semantic skeleton to a locale's patterns as
// ICU's DateTimePatternGenerator does. All integers are little-endian.
//
// Date patterns, a unit of no locale. The payload is a header (the u32
// counts of patterns, rows, sets, time sets, interval sets, interval date
// parts, interval rows, interval time parts, interval prefixes and
// interval pairs, then the u32 text size), then these sections, each
// starting on an 8-byte boundary:
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
//   interval sets a calendar's interval patterns in a locale
//            (dateIntervalSetSize bytes): the date part's
//            and the time part's index (u16), then pattern indexes
//            (u16): the fallback ("{0} – {1}", its two values in literal
//            text), the short date with the full year (yMd) and with the
//            era (GyMd) that a time's date differences write
//   interval date parts the dates' pairs (dateIntervalDateSize bytes): the
//            interval row index (u16) of each entry of DateSetEntry; then
//            for each standard date format, full to short, its pair's
//            index, its prefix's index and its own pattern's index (u16)
//   interval rows a date entry's pair and prefix indexes (u16) at the
//            lengths long, medium and short (dateIntervalRowSize bytes)
//   interval time parts the times' pairs (dateIntervalTimeSize bytes): the
//            pair index (u16) of a time without a zone and with each of
//            mlocZoneStyle's eight forms, on the 12-hour then the 24-hour
//            clock, at the hour, minute and second; then for each
//            standard time format, full to short, its pair's index and
//            its own pattern's index (u16)
//   interval prefixes a date's patterns with the larger fields it lacks
//            (dateIntervalPrefixSize bytes), which a date-time's date
//            differences write: the pattern index (u16) of its skeleton
//            with d, with M and d, with y, M and d and with G, y, M and d
//            prefixed, each that adds a missing field (dateIntervalNone
//            for one present), then the glue style of each (u16, two bits
//            each from bit 0: 0 full, 1 long, 2 medium, 3 short)
//   interval pairs the halves of an interval's patterns by its greatest
//            difference (dateIntervalPairSize bytes): for era, year,
//            month, day, am or pm, hour and minute, the first half's and
//            the second half's pattern index (u16; dateIntervalNone for
//            none): both, the earlier value writes the first and the
//            later the second; the second alone, a fallback, the whole
//            pattern written for each value in the fallback; neither,
//            none; then flags (u16), bit dateIntervalLaterFirst + the
//            difference when the later value writes the first half
//            (the date parts' and rows' pairs first, then the time
//            parts', which the validator relies on to read each once)
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
// time set index (u16) of each calendar, the interval set index (u16) of
// each calendar, and the index (u16) of the locale's day period rules in
// the names unit.

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
// another clock (hmm, hmmss, Hmm, Hmmss), a weekday with a time, which
// one item covers (the 12-hour, then the 24-hour clock; EEEE, then EEE;
// the hour, minute and second), a zone alone in each of the eight zone
// forms (zzzz, z, vvvv, v, VVVV, VVV, OOOO, O: mlocZoneStyle's order), a
// time with a zone (each form; the 12-hour, then the 24-hour clock; the
// hour, minute and second), as ICU matches them, the full and long
// formats on each clock as ICU matches them for another clock (hmmsszzzz,
// hmmssz, Hmmsszzzz, Hmmssz), the relative date-time glue at the four
// lengths ("{1} 'at' {0}" after "tomorrow"; atTime's where the calendar
// has none); then flags, not a pattern:
// dateTimeOwnTwelve when the locale's own hour (UTS #35's j, by its
// region) is on the 12-hour clock, and from bit dateTimeMonthShift two
// bits a length (long, medium, short) for the month of its
// datetimeSkeleton: 0 for one or two letters or none, 1 for three, 2 for
// four, which pick a date-time's glue.
typedef enum DateTimeEntry
{
    dateTimeClock12 = 0,
    dateTimeClock24 = 3,
    dateTimeStandard = 6,
    dateTimeGlue = 10,
    dateTimeAtGlue = 14,
    dateTimeStyle12 = 18,
    dateTimeStyle24 = 20,
    dateTimeWeekday = 22,
    dateTimeZoneAlone = 34,
    dateTimeZoned = 42,
    dateTimeZoneStyle12 = 90,
    dateTimeZoneStyle24 = 92,
    dateTimeRelativeGlue = 94,
    dateTimePatternCount = 98,
    dateTimeFlags = 98,
    dateTimeEntryCount = 99,
} DateTimeEntry;

// The options an interval set answers, each the skeleton the generator
// resolves as ICU4C's DateIntervalFormat does: a date field set
// (DateSetEntry) at each length, long, medium and short; the standard date
// formats, full to short; a time without or with a zone (none, then
// mlocZoneStyle's eight forms; the 12-hour, then the 24-hour clock; the
// hour, minute and second); the standard time formats, full to short. A
// time's date differences are left out (none): ICU writes them as
// fallbacks of the short date with the full year, and the era for an era
// difference, and the time, matched whole, which the run time joins as a
// date-time.
typedef enum DateIntervalEntry
{
    dateIntervalDate = 0,
    dateIntervalDateStyle = 54,
    dateIntervalTime = 58,
    dateIntervalTimeStyle = 112,
    dateIntervalEntryCount = 116,
} DateIntervalEntry;

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
    datePatternsHeaderSize = 44,
    // Added to a year entry for the year style.
    dateStyleFull = 1,
    dateStyleEra = 2,
    dateLengthCount = 3,
    dateRowSize = 2 * dateLengthCount,
    dateStandardCount = 4,
    dateSetSize = 2 * (dateEntryCount + dateStandardCount),
    dateTimeSetSize = 2 * dateTimeEntryCount,
    dateTimeOwnTwelve = 1,
    dateTimeMonthShift = 1,
    // The flags' largest value: every month code 2.
    dateTimeFlagsMax = 1 | 0x2A << 1,
    dateCalendarCount = 18,
    dateIntervalSetSize = 10,
    dateIntervalDateSize = 2 * dateEntryCount + 6 * dateStandardCount,
    dateIntervalRowSize = 4 * dateLengthCount,
    dateIntervalTimeSize = 2 * (dateIntervalTimeStyle - dateIntervalTime) + 4 * dateStandardCount,
    dateIntervalPrefixSize = 10,
    // The prefixes: d, M, y and G.
    dateIntervalPrefixes = 4,
    // The greatest differences of a pair: era, year, month, day, am or
    // pm, hour, minute.
    dateIntervalFields = 7,
    dateIntervalPairSize = 4 * dateIntervalFields + 2,
    dateIntervalNone = 0xFFFF,
    dateIntervalLaterFirst = 0,
    dateLocaleSize = 8 * dateCalendarCount + 2,
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
