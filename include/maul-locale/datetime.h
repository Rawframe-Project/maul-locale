// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates formatted (UTS #35 Part 4, record mloc-0042): a date written in a
// locale by what it should show, a semantic field set at a length ("Jan
// 5, 2026" for year, month and day, medium), as ICU4X's semantic
// skeletons and CLDR's datetime conformance data name them, or by the
// locale's four standard formats. The patterns were matched to the
// locale's data by the generator as ICU4C's DateTimePatternGenerator
// matches them; they and the calendar names are in a data file of their
// own, dates.mloc, which a program adds to its data set.

#ifndef MAUL_LOCALE_DATETIME_H
#define MAUL_LOCALE_DATETIME_H

#include "maul-locale/calendar.h"
#include "maul-locale/locale.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // What a date shows: its day of the month, weekday, month and year in
    // the sets UTS #35 defines.
    typedef enum mlocDateFieldSet
    {
        mloc_dateFieldSetYearMonthDay = 0,
        mloc_dateFieldSetYearMonthDayWeekday = 1,
        mloc_dateFieldSetMonthDay = 2,
        mloc_dateFieldSetMonthDayWeekday = 3,
        mloc_dateFieldSetDay = 4,
        mloc_dateFieldSetDayWeekday = 5,
        mloc_dateFieldSetWeekday = 6,
        // Calendar periods: a month alone, a year, a month of a year.
        mloc_dateFieldSetMonth = 7,
        mloc_dateFieldSetYear = 8,
        mloc_dateFieldSetYearMonth = 9,
    } mlocDateFieldSet;

    // How much room the date takes: names written out ("January 5,
    // 2026"), abbreviated ("Jan 5, 2026"), or numeric where the locale
    // has a numeric form ("1/5/26").
    typedef enum mlocDateLength
    {
        mloc_dateLengthLong = 0,
        mloc_dateLengthMedium = 1,
        mloc_dateLengthShort = 2,
    } mlocDateLength;

    // How a year is written: as the length's pattern writes it where the
    // year is clear, else with its century, else with its era too (a
    // Gregorian year from 1950 to 2049 is clear in two digits; one before
    // 1000 or before the common era needs its era); always with its
    // century; always with its era.
    typedef enum mlocYearStyle
    {
        mloc_yearStyleAuto = 0,
        mloc_yearStyleFull = 1,
        mloc_yearStyleWithEra = 2,
    } mlocYearStyle;

    // One of the locale's four standard date formats instead of a field
    // set, or none.
    typedef enum mlocDateStyle
    {
        mloc_dateStyleNone = 0,
        mloc_dateStyleFull = 1,
        mloc_dateStyleLong = 2,
        mloc_dateStyleMedium = 3,
        mloc_dateStyleShort = 4,
    } mlocDateStyle;

    // The options of a date formatter; zeroed, a year, month and day at
    // the long length.
    typedef struct mlocDateOptions
    {
        mlocDateFieldSet fields;
        mlocDateLength length;
        mlocYearStyle yearStyle;
        // A standard format, which the fields, length and year style then
        // do not change.
        mlocDateStyle style;
    } mlocDateOptions;

    // The parts of a formatted date.
    typedef enum mlocDatePart
    {
        mloc_datePartLiteral = 0,
        mloc_datePartEra = 1,
        mloc_datePartYear = 2,
        // The Gregorian year a calendar's year begins in (Chinese
        // calendars).
        mloc_datePartRelatedYear = 3,
        // The year's name in a 60-year cycle (Chinese calendars).
        mloc_datePartYearName = 4,
        mloc_datePartMonth = 5,
        mloc_datePartWeekday = 6,
        mloc_datePartDay = 7,
    } mlocDatePart;

    // A date formatter: a locale's patterns for the options in its
    // calendar, in the data it was made from, which must outlive it.
    // Opaque; make it with mlocInitDateFormatter.
    typedef struct mlocDateFormatter
    {
        uint64_t opaque[64];
    } mlocDateFormatter;

    /// Makes a locale's date formatter. The calendar is the locale's
    /// (mlocLocalePreferences: -u-ca, else its region's first); the
    /// numbering system its -u-nu keyword's, else its default, with the
    /// systems a pattern names for its fields (Japanese years by
    /// jpanyear, Hebrew dates by hebr).
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the dates (dates.mloc), the
    ///                      number units, the preferences, the parents
    ///                      and, for a pattern naming an algorithmic
    ///                      numbering system, the spell-out rules.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the dates, the number
    ///         units or the preferences, or a numbering system the
    ///         spell-out rules lack; mloc_errorData for a pattern the
    ///         formatter cannot write; mloc_errorInvalid for a null
    ///         pointer or options out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitDateFormatter(mlocDateFormatter* formatterOut,
                                                             const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const mlocDateOptions* options);

    /// Writes a date by the formatter, read in the formatter's calendar
    /// (the same day). Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateFormatter.
    /// @param date       The date.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a date outside Temporal's range, or a
    ///         formatter the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDate(const mlocDateFormatter* formatter,
                                                      const mlocDate* date, mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
