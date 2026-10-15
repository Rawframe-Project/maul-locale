// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The date pattern units of the dates data file (record mloc-0042),
// which the generator compiles by matching each semantic skeleton to a
// locale's patterns as ICU's DateTimePatternGenerator does. All integers
// are little-endian.
//
// Date patterns, a unit of no locale. The payload is a header (u32
// pattern count, u32 row count, u32 set count, u32 text size), then these
// sections, each starting on an 8-byte boundary:
//   offsets  each pattern's offset in the text (u32), count + 1 of them,
//            from 0 to the text size
//   rows     a field set's patterns at the three lengths, long, medium
//            and short (u16 pattern indexes, dateRowSize bytes)
//   sets     a calendar's date patterns in a locale (dateSetSize bytes):
//            the row index (u16) of each entry of DateSetEntry, then the
//            pattern index (u16) of the standard full, long, medium and
//            short date formats
//   text     the compiled patterns
// A compiled pattern is literal UTF-8 text (no byte below 0x20, quotes
// already resolved) and field items: datePatternField, the field's
// letter (one of datePatternLetters) and its width (1 to 9); or
// datePatternOverride, the letter, the width, the length (1 to 8) and
// the lower-case id of the numbering system that writes the field
// (CLDR's numbers="hanidec", "d=hanidays" and the like, which ICU4X
// carries to a length's semantic patterns).
//
// A locale's dates, a unit per locale filed where the resolved data
// differs from its chain's: the set index (u16) of each calendar, in the
// order of MlocCalendar (the ISO calendar's is the Gregorian one's).

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

enum
{
    datePatternsHeaderSize = 16,
    // Added to a year entry for the year style.
    dateStyleFull = 1,
    dateStyleEra = 2,
    dateLengthCount = 3,
    dateRowSize = 2 * dateLengthCount,
    dateStandardCount = 4,
    dateSetSize = 2 * (dateEntryCount + dateStandardCount),
    dateCalendarCount = 18,
    dateLocaleSize = 2 * dateCalendarCount,
    datePatternField = 1,
    datePatternOverride = 2,
    dateMaxWidth = 9,
    dateMaxSystemId = 8,
};

// The field letters a compiled pattern may hold.
#define datePatternLetters "GyYuUrQqMLlwWdDFgEecabBhHKkmsSAzZOvVXx"

#endif
