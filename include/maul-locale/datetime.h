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

    // How a time zone is written (UTS #35's zone formats, chosen as
    // ICU4X's zone field sets choose them), each falling back as ICU4C
    // falls back where the locale has no name.
    typedef enum mlocZoneStyle
    {
        mloc_zoneStyleNone = 0,
        // "Pacific Daylight Time": the zone's standard or daylight name
        // at the instant (zzzz); else the long offset.
        mloc_zoneStyleSpecificLong = 1,
        // "PDT" (z); else the short offset.
        mloc_zoneStyleSpecificShort = 2,
        // "Pacific Time": the wall-clock name (vvvv), qualified by a
        // city or country where the locale's region names another zone
        // ("Pacific Time (Canada)"); else the location; else the long
        // offset.
        mloc_zoneStyleGenericLong = 3,
        // "PT" (v); else the location; else the short offset.
        mloc_zoneStyleGenericShort = 4,
        // "Los Angeles Time", "Japan Time" (VVVV); else the long offset.
        mloc_zoneStyleLocation = 5,
        // "Los Angeles" (VVV); else the locale's name of an unknown
        // city.
        mloc_zoneStyleCity = 6,
        // "GMT-07:00" (OOOO).
        mloc_zoneStyleOffsetLong = 7,
        // "GMT-7" (O).
        mloc_zoneStyleOffsetShort = 8,
    } mlocZoneStyle;

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
        // A zone after the date, joined by the calendar's glue as a time
        // is ("January 5, 2026 at PT"), for mlocFormatDateInZone; none for
        // a date alone.
        mlocZoneStyle zone;
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
        // A time zone's name, city or offset ("Pacific Time", "GMT-7").
        mloc_datePartTimeZone = 13,
    } mlocDatePart;

    // A date formatter: a locale's patterns for the options in its
    // calendar, in the data it was made from, which must outlive it.
    // Opaque; make it with mlocInitDateFormatter.
    typedef struct mlocDateFormatter
    {
        uint64_t opaque[112];
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
    ///                      numbering system, the spell-out rules; with a
    ///                      zone, what mlocInitZoneFormatter reads too.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the dates, the number
    ///         units or the preferences, or a numbering system the
    ///         spell-out rules lack, or with a zone without the zone
    ///         names; mloc_errorData for a pattern the formatter cannot
    ///         write; mloc_errorInvalid for a null pointer, options out of
    ///         range, or a zone with a calendar period (a month, a year).
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
    ///         formatter the library did not make or made with a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDate(const mlocDateFormatter* formatter,
                                                      const mlocDate* date, mlocOutput* output);

    /// Writes the date in a zone at an instant, and the zone, by a
    /// formatter made with a zone (mlocDateOptions' zone): the zone's
    /// local date in the formatter's calendar, the zone as
    /// mlocFormatZone writes it. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateFormatter, with a
    ///                   zone.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make or made without a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateInZone(const mlocDateFormatter* formatter,
                                                            const mlocTimeZone* zone,
                                                            const mlocInstant* instant,
                                                            mlocOutput* output);

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
    // precision, or none. The full and long ones name a time zone, so
    // they write a zoned time only (mlocFormatTimeInZone).
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
        // clock instead, as ICU4C does. The full and long formats name
        // their zone themselves.
        mlocTimeStyle style;
        // A zone after the time ("4:03 PM PT"), as the locale's patterns
        // join them, for mlocFormatTimeInZone; none for a time alone, and
        // with a standard format.
        mlocZoneStyle zone;
    } mlocTimeOptions;

    // A time formatter: a locale's time patterns for the options in its
    // calendar, in the data it was made from, which must outlive it.
    // Opaque; make it with mlocInitTimeFormatter.
    typedef struct mlocTimeFormatter
    {
        uint64_t opaque[72];
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
    ///                      parents; with a zone (a zone style, or the
    ///                      full or long format), what
    ///                      mlocInitZoneFormatter reads too.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the dates, the number
    ///         units or the preferences, or with a zone without the zone
    ///         names; mloc_errorData for a pattern the formatter cannot
    ///         write; mloc_errorInvalid for a null pointer, options out of
    ///         range, or a zone style with a standard format.
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
    ///         library did not make or made with a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTime(const mlocTimeFormatter* formatter,
                                                      const mlocTime* time, mlocOutput* output);

    /// Writes the time of day in a zone at an instant, and the zone, by a
    /// formatter made with one (a zone style, or the full or long format):
    /// the zone's local time, the zone as mlocFormatZone writes it in the
    /// form the pattern names. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitTimeFormatter, with a
    ///                   zone.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make or made without a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeInZone(const mlocTimeFormatter* formatter,
                                                            const mlocTimeZone* zone,
                                                            const mlocInstant* instant,
                                                            mlocOutput* output);

    // Which of the calendar's glue joins a date and a time: atTime ("{1}
    // 'at' {0}", UTS #35's default since CLDR 48, as ICU4C 79 and ICU4X
    // join them), or the standard one ("{1}, {0}"), CLDR's
    // dateTimeFormatType.
    typedef enum mlocDateTimeGlue
    {
        mloc_dateTimeGlueAtTime = 0,
        mloc_dateTimeGlueStandard = 1,
    } mlocDateTimeGlue;

    // The options of a date-time formatter: a date field set that shows
    // a day or a weekday (not a calendar period) with a time, or a
    // standard date format with a standard time format; the zone is the
    // time's (the date's is none). Zeroed, a year, month and day at the
    // long length with the time to the second, joined by the atTime
    // glue.
    typedef struct mlocDateTimeOptions
    {
        mlocDateOptions date;
        mlocTimeOptions time;
        mlocDateTimeGlue glue;
    } mlocDateTimeOptions;

    // A date-time formatter: a locale's date and time patterns and the
    // glue between them. Opaque; make it with mlocInitDateTimeFormatter.
    typedef struct mlocDateTimeFormatter
    {
        uint64_t opaque[144];
    } mlocDateTimeFormatter;

    /// Makes a locale's date-time formatter: the date as
    /// mlocInitDateFormatter writes it, the time as mlocInitTimeFormatter
    /// does, joined by the calendar's glue ("October 15, 2026 at 4:03
    /// PM") of the date's length as ICU4C chooses it (a field set by
    /// its month: written out with a weekday full, written out long,
    /// abbreviated medium, else short; a standard date format by its own
    /// length). A weekday with a time is matched whole where the locale
    /// has a pattern for both ("Thursday 4:03 PM"), unless a zone follows
    /// ("Thu, 4:03 PM PT", as ICU4C composes it).
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          As mlocInitDateFormatter's.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return As mlocInitDateFormatter and mlocInitTimeFormatter;
    ///         mloc_errorInvalid also for a calendar period (a month, a
    ///         year), a standard format on one side only, or a zone in
    ///         the date's options.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocInitDateTimeFormatter(mlocDateTimeFormatter* formatterOut, const mlocDataSet* data,
                              const mlocLocale* locale, const mlocDateTimeOptions* options);

    /// Writes a date and time of day by the formatter, the date read in
    /// the formatter's calendar. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateTimeFormatter.
    /// @param dateTime   The date and time.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a date-time out of range, or a formatter
    ///         the library did not make or made with a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDateTime(const mlocDateTimeFormatter* formatter,
                                                          const mlocDateTime* dateTime,
                                                          mlocOutput* output);

    /// Writes the date and time of day in a zone at an instant, and the
    /// zone, by a formatter made with one: the zone's local date-time,
    /// the date read in the formatter's calendar. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateTimeFormatter, with
    ///                   a zone.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make or made without a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocFormatDateTimeInZone(const mlocDateTimeFormatter* formatter, const mlocTimeZone* zone,
                             const mlocInstant* instant, mlocOutput* output);

    // A zone formatter: a locale's zone names and formats in the data it
    // was made from, which must outlive it, as must the data set.
    // Opaque; make it with mlocInitZoneFormatter.
    typedef struct mlocZoneFormatter
    {
        uint64_t opaque[48];
    } mlocZoneFormatter;

    /// Makes a locale's zone formatter. The names are its zone names
    /// (zone-names.mloc) along its chain, the countries' its region names
    /// (names.mloc, else the region code), the offset's digits its
    /// numbering system's (-u-nu, else its default; an algorithmic one
    /// writes ASCII digits). A generic name is qualified where the
    /// metazone's preferred zone for the locale's region (its likely
    /// region when it names none) is another zone of another offset.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the zone names (zone-names.mloc),
    ///                      the zone codes, the number units, the likely
    ///                      subtags and the parents; the region names
    ///                      (names.mloc) for the location formats.
    /// @param locale        The locale, canonicalized.
    /// @param style         How zones are written.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the zone names, the
    ///         zone codes or the number units; mloc_errorInvalid for a null
    ///         pointer or a style out of range or none.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitZoneFormatter(mlocZoneFormatter* formatterOut,
                                                             const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             mlocZoneStyle style);

    /// Writes a time zone at an instant by the formatter (the instant
    /// picks the metazone, standard or daylight time and the offset). A
    /// zone CLDR does not know, an offset zone and a zone without a
    /// region fall back to the offset. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitZoneFormatter.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param output     Where the text goes (record mloc-0015).
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (output's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatZone(const mlocZoneFormatter* formatter,
                                                      const mlocTimeZone* zone,
                                                      const mlocInstant* instant,
                                                      mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
