// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates and times formatted (UTS #35 Part 4, record mloc-0042): a date
// written in a locale by what it should show, a semantic field set at a
// length ("Jan 5, 2026" for year, month and day, medium), as ICU4X's
// semantic skeletons and CLDR's datetime conformance data name them, or
// by the locale's four standard formats; a time of day by its hour cycle
// and precision ("4:03 PM"), or by a standard format. The patterns were matched to the
// locale's data by the generator as ICU4C's DateTimePatternGenerator
// matches them; they and the calendar names are in a data file of their
// own, dates.mloc, which a program adds to its data set.

#ifndef MAUL_LOCALE_DATETIME_H
#define MAUL_LOCALE_DATETIME_H

#include "maul-locale/calendar.h"
#include "maul-locale/locale.h"
#include "maul-locale/zone.h"

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

    // The parts of a formatted date or time.
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
        mloc_datePartHour = 8,
        mloc_datePartMinute = 9,
        mloc_datePartSecond = 10,
        // The digits after the second's decimal separator.
        mloc_datePartFraction = 11,
        // AM or PM, noon, or a period of the day ("in the evening").
        mloc_datePartDayPeriod = 12,
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

    // How hours are numbered (UTS #35's hour cycle). Auto takes -u-hc as
    // the cycle when the locale gives one, else the clock of the locale's
    // region (mlocLocalePreferences, -u-rg first) as the locale writes
    // it. H11 to H24 number hours 0 to 11, 1 to 12, 0 to 23 or 1 to 24 on
    // the locale's pattern of that clock; Clock12 and Clock24 take the
    // locale's own 12-hour or 24-hour clock ("aK:mm" in Japanese).
    typedef enum mlocHourCycle
    {
        mloc_hourCycleAuto = 0,
        mloc_hourCycleH11 = 1,
        mloc_hourCycleH12 = 2,
        mloc_hourCycleH23 = 3,
        mloc_hourCycleH24 = 4,
        mloc_hourCycleClock12 = 5,
        mloc_hourCycleClock24 = 6,
    } mlocHourCycle;

    // How precisely a time is written: to the second, the hour, the
    // minute, the minute unless it is zero (then the hour), or the second
    // with 1 to 9 digits of its fraction. A time is truncated, never
    // rounded: 11:59:59 is "11:59" to the minute.
    typedef enum mlocTimePrecision
    {
        mloc_timePrecisionSecond = 0,
        mloc_timePrecisionHour = 1,
        mloc_timePrecisionMinute = 2,
        mloc_timePrecisionMinuteOptional = 3,
        mloc_timePrecisionFraction1 = 4,
        mloc_timePrecisionFraction2 = 5,
        mloc_timePrecisionFraction3 = 6,
        mloc_timePrecisionFraction4 = 7,
        mloc_timePrecisionFraction5 = 8,
        mloc_timePrecisionFraction6 = 9,
        mloc_timePrecisionFraction7 = 10,
        mloc_timePrecisionFraction8 = 11,
        mloc_timePrecisionFraction9 = 12,
    } mlocTimePrecision;

    // One of the locale's four standard time formats instead of a
    // precision, or none. The full and long ones name a time zone.
    typedef enum mlocTimeStyle
    {
        mloc_timeStyleNone = 0,
        mloc_timeStyleFull = 1,
        mloc_timeStyleLong = 2,
        mloc_timeStyleMedium = 3,
        mloc_timeStyleShort = 4,
    } mlocTimeStyle;

    // The options of a time formatter; zeroed, the locale's hour cycle
    // to the second.
    typedef struct mlocTimeOptions
    {
        mlocHourCycle hourCycle;
        mlocTimePrecision precision;
        // A standard format, which the precision then does not change.
        // An hour cycle asked for (here or by -u-hc) or a region by -u-rg
        // takes the locale's minutes (short) or seconds (medium) on that
        // clock instead, as ICU4C does.
        mlocTimeStyle style;
    } mlocTimeOptions;

    // A time formatter: a locale's time patterns for the options in its
    // calendar, in the data it was made from, which must outlive it.
    // Opaque; make it with mlocInitTimeFormatter.
    typedef struct mlocTimeFormatter
    {
        uint64_t opaque[24];
    } mlocTimeFormatter;

    /// Makes a locale's time formatter. The patterns are those of the
    /// locale's calendar (mlocLocalePreferences), the digits and the
    /// decimal separator its numbering system's (-u-nu, else its
    /// default).
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the dates (dates.mloc), the
    ///                      number units, the preferences and the
    ///                      parents.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the dates, the number
    ///         units or the preferences; mloc_errorData for a pattern the
    ///         formatter cannot write; mloc_errorInvalid for a null
    ///         pointer, options out of range, or the full or long style,
    ///         which names a zone a time of day does not have.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitTimeFormatter(mlocTimeFormatter* formatterOut,
                                                             const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const mlocTimeOptions* options);

    /// Writes a time of day by the formatter. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitTimeFormatter.
    /// @param time       The time.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a time out of range, or a formatter the
    ///         library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTime(const mlocTimeFormatter* formatter,
                                                      const mlocTime* time, mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
