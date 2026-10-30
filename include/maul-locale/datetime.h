// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Dates and times formatted (UTS #35 Part 4): a date written in a locale
// by what it should show, a semantic field set at a length ("Jan 5, 2026"
// for year, month and day, medium), as ICU4X's semantic skeletons and
// CLDR's datetime conformance data name them, or by the locale's four
// standard formats; a time of day by its hour cycle and precision ("4:03
// PM"), or by a standard format. The patterns were matched to the locale's
// data by the generator as ICU4C's DateTimePatternGenerator matches them;
// they and the calendar names are in a data file of their own, dates.mloc,
// which a program adds to its data set.

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
    typedef int32_t mlocDateFieldSet;
    enum
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
    };

    // How much room the date takes: names written out ("January 5,
    // 2026"), abbreviated ("Jan 5, 2026"), or numeric where the locale
    // has a numeric form ("1/5/26").
    typedef int32_t mlocDateLength;
    enum
    {
        mloc_dateLengthLong = 0,
        mloc_dateLengthMedium = 1,
        mloc_dateLengthShort = 2,
    };

    // How a year is written: as the length's pattern writes it where the
    // year is clear, else with its century, else with its era too (a
    // Gregorian year from 1950 to 2049 is clear in two digits; one before
    // 1000 or before the common era needs its era); always with its
    // century; always with its era.
    typedef int32_t mlocYearStyle;
    enum
    {
        mloc_yearStyleAuto = 0,
        mloc_yearStyleFull = 1,
        mloc_yearStyleWithEra = 2,
    };

    // One of the locale's four standard date formats instead of a field
    // set, or none.
    typedef int32_t mlocDateStyle;
    enum
    {
        mloc_dateStyleNone = 0,
        mloc_dateStyleFull = 1,
        mloc_dateStyleLong = 2,
        mloc_dateStyleMedium = 3,
        mloc_dateStyleShort = 4,
    };

    // How a time zone is written (UTS #35's zone formats, chosen as
    // ICU4X's zone field sets choose them), each falling back as ICU4C
    // falls back where the locale has no name.
    typedef int32_t mlocZoneStyle;
    enum
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
    };

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
        // is ("January 5, 2026 at PT"), for mlocDateFormatter_FormatInZone; none for
        // a date alone.
        mlocZoneStyle zone;
    } mlocDateOptions;

    // The parts of a formatted date or time.
    typedef int32_t mlocDatePart;
    enum
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
        // A day relative to today, whole ("tomorrow", "in 3 days";
        // mlocRelativeFormatter_FormatDayTime).
        mloc_datePartRelativeDay = 14,
    };

    // In a range's parts, the value a part belongs to, added to its
    // mlocDatePart: the start, the end, or neither for what both share
    // (a field written once for both, the text around the values), as
    // ICU4C's interval spans mark them: a value's parts run from its
    // first field of a kind the other value also writes to its last.
    enum
    {
        mloc_dateRangeStart = 0x100,
        mloc_dateRangeEnd = 0x200,
    };

    // A date formatter: a locale's patterns for the options in its
    // calendar, in the data it was made from, which must outlive it.
    // Opaque; make it with mlocInitDateFormatter.
    typedef struct mlocDateFormatter
    {
        uint64_t opaque[128];
    } mlocDateFormatter;

    /// Makes a locale's date formatter. The calendar is the locale's
    /// (mlocLocalePreferences: -u-ca, else its region's first); the
    /// numbering system its -u-nu keyword's, else its default, with the
    /// systems a pattern names for its fields (Japanese years by
    /// jpanyear, Hebrew dates by hebr).
    ///
    /// @param data          The data with the dates (dates.mloc), the
    ///                      number units, the preferences, the parents
    ///                      and, for a pattern naming an algorithmic
    ///                      numbering system, the spell-out rules; with a
    ///                      zone, what mlocInitZoneFormatter reads too.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the dates, the number
    ///         units or the preferences, or a numbering system the
    ///         spell-out rules lack, or with a zone without the zone
    ///         names; mloc_errorFormat for a pattern the formatter cannot
    ///         write; mloc_errorInvalid for a null pointer, options out of
    ///         range, or a zone with a calendar period (a month, a year).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitDateFormatter(const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const mlocDateOptions* options,
                                                             mlocDateFormatter* formatterOut);

    /// Writes a date by the formatter, read in the formatter's calendar
    /// (the same day). Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateFormatter.
    /// @param date       The date.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a date outside Temporal's range, or a
    ///         formatter the library did not make or made with a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateFormatter_Format(const mlocDateFormatter* formatter,
                                                                const mlocDate* date,
                                                                mlocOutput* textOut);

    /// Writes the date in a zone at an instant, and the zone, by a
    /// formatter made with a zone (mlocDateOptions' zone): the zone's
    /// local date in the formatter's calendar, the zone as
    /// mlocZoneFormatter_Format writes it. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateFormatter, with a
    ///                   zone.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make or made without a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocDateFormatter_FormatInZone(const mlocDateFormatter* formatter, const mlocTimeZone* zone,
                                   const mlocInstant* instant, mlocOutput* textOut);

    /// Writes a range of two dates by the formatter (ECMA-402's
    /// formatRange), as ICU4C's DateIntervalFormat writes the formatter's
    /// skeleton: the locale's interval pattern for the greatest field the
    /// dates differ in ("Jan 5 – 8, 2026", "Jan 5 – Feb 8, 2026"), else
    /// the locale's fallback around both dates ("1/5/2026 – 1/8/2026");
    /// one date when they differ in no field shown. Both are read in the
    /// formatter's calendar and written in the year style the one that
    /// needs more asks; the start may pass the end.
    ///
    /// @param formatter  A formatter from mlocInitDateFormatter, without a
    ///                   zone.
    /// @param start      The range's start.
    /// @param end        Its end.
    /// @param textOut    Where the text goes; its part
    ///                   function, when set, receives mlocDatePart values
    ///                   with the date each belongs to (mloc_dateRangeStart,
    ///                   mloc_dateRangeEnd, or 0 for both).
    /// @return As mlocDateFormatter_Format.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocDateFormatter_FormatRange(const mlocDateFormatter* formatter, const mlocDate* start,
                                  const mlocDate* end, mlocOutput* textOut);

    /// Writes a range of two instants' dates in a zone, each with the zone,
    /// by a formatter made with one: on one local day the start's date and
    /// zone; on two, each whole around the fallback, the date by the
    /// pattern with the greatest difference's field prefixed where the
    /// date lacks it ("Jan 5 at PST – Feb 8 at PST" as "Jan 5, 2026 at PST
    /// – Feb 8, 2027 at PST" across years), as ICU4C's DateIntervalFormat
    /// writes the date's skeleton with the zone's.
    ///
    /// @param formatter  A formatter from mlocInitDateFormatter, with a
    ///                   zone.
    /// @param zone       The zone.
    /// @param start      The range's start, in Temporal's range.
    /// @param end        Its end, likewise.
    /// @param textOut    Where the text goes; parts as
    ///                   mlocDateFormatter_FormatRange's.
    /// @return As mlocDateFormatter_FormatInZone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateFormatter_FormatRangeInZone(
        const mlocDateFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* start,
        const mlocInstant* end, mlocOutput* textOut);

    // How hours are numbered (UTS #35's hour cycle). Auto takes -u-hc as
    // the cycle when the locale gives one, else the clock of the locale's
    // region (mlocLocalePreferences, -u-rg first) as the locale writes
    // it. H11 to H24 number hours 0 to 11, 1 to 12, 0 to 23 or 1 to 24 on
    // the locale's pattern of that clock; Clock12 and Clock24 take the
    // locale's own 12-hour or 24-hour clock ("aK:mm" in Japanese).
    typedef int32_t mlocHourCycle;
    enum
    {
        mloc_hourCycleAuto = 0,
        mloc_hourCycleH11 = 1,
        mloc_hourCycleH12 = 2,
        mloc_hourCycleH23 = 3,
        mloc_hourCycleH24 = 4,
        mloc_hourCycleClock12 = 5,
        mloc_hourCycleClock24 = 6,
    };

    // How precisely a time is written: to the second, the hour, the
    // minute, the minute unless it is zero (then the hour), or the second
    // with 1 to 9 digits of its fraction. A time is truncated, never
    // rounded: 11:59:59 is "11:59" to the minute.
    typedef int32_t mlocTimePrecision;
    enum
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
    };

    // One of the locale's four standard time formats instead of a
    // precision, or none. The full and long ones name a time zone, so
    // they write a zoned time only (mlocTimeFormatter_FormatInZone).
    typedef int32_t mlocTimeStyle;
    enum
    {
        mloc_timeStyleNone = 0,
        mloc_timeStyleFull = 1,
        mloc_timeStyleLong = 2,
        mloc_timeStyleMedium = 3,
        mloc_timeStyleShort = 4,
    };

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
        // join them, for mlocTimeFormatter_FormatInZone; none for a time alone, and
        // with a standard format.
        mlocZoneStyle zone;
    } mlocTimeOptions;

    // A time formatter: a locale's time patterns for the options in its
    // calendar, in the data it was made from, which must outlive it.
    // Opaque; make it with mlocInitTimeFormatter.
    typedef struct mlocTimeFormatter
    {
        uint64_t opaque[80];
    } mlocTimeFormatter;

    /// Makes a locale's time formatter. The patterns are those of the
    /// locale's calendar (mlocLocalePreferences), the digits and the
    /// decimal separator its numbering system's (-u-nu, else its
    /// default).
    ///
    /// @param data          The data with the dates (dates.mloc), the
    ///                      number units, the preferences and the
    ///                      parents; with a zone (a zone style, or the
    ///                      full or long format), what
    ///                      mlocInitZoneFormatter reads too.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the dates, the number
    ///         units or the preferences, or with a zone without the zone
    ///         names; mloc_errorFormat for a pattern the formatter cannot
    ///         write; mloc_errorInvalid for a null pointer, options out of
    ///         range, or a zone style with a standard format.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitTimeFormatter(const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const mlocTimeOptions* options,
                                                             mlocTimeFormatter* formatterOut);

    /// Writes a time of day by the formatter. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitTimeFormatter.
    /// @param time       The time.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a time out of range, or a formatter the
    ///         library did not make or made with a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocTimeFormatter_Format(const mlocTimeFormatter* formatter,
                                                                const mlocTime* time,
                                                                mlocOutput* textOut);

    /// Writes the time of day in a zone at an instant, and the zone, by a
    /// formatter made with one (a zone style, or the full or long format):
    /// the zone's local time, the zone as mlocZoneFormatter_Format writes it in the
    /// form the pattern names. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitTimeFormatter, with a
    ///                   zone.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make or made without a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocTimeFormatter_FormatInZone(const mlocTimeFormatter* formatter, const mlocTimeZone* zone,
                                   const mlocInstant* instant, mlocOutput* textOut);

    /// Writes a range of two times of day by the formatter, as ICU4C's
    /// DateIntervalFormat writes the formatter's skeleton: the locale's
    /// interval pattern for the greatest field they differ in (am or pm,
    /// the hour, the minute: "10:00 – 11:30 AM"), else the fallback
    /// around both; one time when they differ in no field shown. Under
    /// minute-optional precision both take the hour's pattern only when
    /// both minutes are zero. The start may pass the end.
    ///
    /// @param formatter  A formatter from mlocInitTimeFormatter, without a
    ///                   zone.
    /// @param start      The range's start.
    /// @param end        Its end.
    /// @param textOut    Where the text goes; parts as
    ///                   mlocDateFormatter_FormatRange's.
    /// @return As mlocTimeFormatter_Format.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocTimeFormatter_FormatRange(const mlocTimeFormatter* formatter, const mlocTime* start,
                                  const mlocTime* end, mlocOutput* textOut);

    /// Writes a range of two instants' times of day in a zone by a
    /// formatter made with one, each with its zone as the pattern names
    /// it. Instants on two local dates are written whole around the
    /// fallback, each as the short date with the full year (with the era
    /// across eras) and the time joined by the short atTime glue, as
    /// ICU4C writes them ("1/5/2026, 10:00 PM PST – 1/6/2026, 2:00 AM
    /// PST"); the dates take a date formatter's state in the locale's
    /// calendar, made then from the data and locale the formatter was
    /// made with.
    ///
    /// @param formatter  A formatter from mlocInitTimeFormatter, with a
    ///                   zone.
    /// @param zone       The zone.
    /// @param start      The range's start, in Temporal's range.
    /// @param end        Its end, likewise.
    /// @param textOut    Where the text goes; parts as
    ///                   mlocDateFormatter_FormatRange's.
    /// @return As mlocTimeFormatter_FormatInZone; across dates, also as
    ///         mlocInitDateFormatter for the dates (mloc_errorNotFound for
    ///         data without a numbering system's spell-out rules the dates
    ///         name).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocTimeFormatter_FormatRangeInZone(
        const mlocTimeFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* start,
        const mlocInstant* end, mlocOutput* textOut);

    // Which of the calendar's glue joins a date and a time: atTime ("{1}
    // 'at' {0}", UTS #35's default since CLDR 48, as ICU4C 79 and ICU4X
    // join them), or the standard one ("{1}, {0}"), CLDR's
    // dateTimeFormatType.
    typedef int32_t mlocDateTimeGlue;
    enum
    {
        mloc_dateTimeGlueAtTime = 0,
        mloc_dateTimeGlueStandard = 1,
    };

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
        uint64_t opaque[176];
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
    MLOC_NODISCARD MLOC_API mlocResult mlocInitDateTimeFormatter(
        const mlocDataSet* data, const mlocLocale* locale, const mlocDateTimeOptions* options,
        mlocDateTimeFormatter* formatterOut);

    /// Writes a date and time of day by the formatter, the date read in
    /// the formatter's calendar. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateTimeFormatter.
    /// @param dateTime   The date and time.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a date-time out of range, or a formatter
    ///         the library did not make or made with a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeFormatter_Format(
        const mlocDateTimeFormatter* formatter, const mlocDateTime* dateTime, mlocOutput* textOut);

    /// Writes the date and time of day in a zone at an instant, and the
    /// zone, by a formatter made with one: the zone's local date-time,
    /// the date read in the formatter's calendar. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitDateTimeFormatter, with
    ///                   a zone.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make or made without a zone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeFormatter_FormatInZone(
        const mlocDateTimeFormatter* formatter, const mlocTimeZone* zone,
        const mlocInstant* instant, mlocOutput* textOut);

    /// Writes a range of two date-times by the formatter, as ICU4C's
    /// DateIntervalFormat writes the formatter's skeleton: on one day the
    /// date once and the time's range inside the Gregorian medium
    /// date-time glue ("Jan 5, 2026, 10:00 – 11:30 AM"), or one date-time
    /// where the time shows no difference; on two days each whole around
    /// the fallback, the date by the pattern with the greatest difference's
    /// field prefixed where the date lacks it, joined by the formatter's
    /// glue of that pattern's length. The start may pass the end.
    ///
    /// @param formatter  A formatter from mlocInitDateTimeFormatter,
    ///                   without a zone.
    /// @param start      The range's start.
    /// @param end        Its end.
    /// @param textOut    Where the text goes; parts as
    ///                   mlocDateFormatter_FormatRange's.
    /// @return As mlocDateTimeFormatter_Format.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeFormatter_FormatRange(
        const mlocDateTimeFormatter* formatter, const mlocDateTime* start, const mlocDateTime* end,
        mlocOutput* textOut);

    /// Writes a range of two instants' date-times in a zone by a formatter
    /// made with one, as mlocDateTimeFormatter_FormatRange writes the zone's local
    /// date-times, each time with its zone as the pattern names it.
    ///
    /// @param formatter  A formatter from mlocInitDateTimeFormatter, with
    ///                   a zone.
    /// @param zone       The zone.
    /// @param start      The range's start, in Temporal's range.
    /// @param end        Its end, likewise.
    /// @param textOut    Where the text goes; parts as
    ///                   mlocDateFormatter_FormatRange's.
    /// @return As mlocDateTimeFormatter_FormatInZone.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeFormatter_FormatRangeInZone(
        const mlocDateTimeFormatter* formatter, const mlocTimeZone* zone, const mlocInstant* start,
        const mlocInstant* end, mlocOutput* textOut);

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
    /// @param data          The data with the zone names (zone-names.mloc),
    ///                      the zone codes, the number units, the likely
    ///                      subtags and the parents; the region names
    ///                      (names.mloc) for the location formats.
    /// @param locale        The locale, canonicalized.
    /// @param style         How zones are written.
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the zone names, the
    ///         zone codes or the number units; mloc_errorInvalid for a null
    ///         pointer or a style out of range or none.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitZoneFormatter(const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             mlocZoneStyle style,
                                                             mlocZoneFormatter* formatterOut);

    /// Writes a time zone at an instant by the formatter (the instant
    /// picks the metazone, standard or daylight time and the offset). A
    /// zone CLDR does not know, an offset zone and a zone without a
    /// region fall back to the offset. Parts: mlocDatePart.
    ///
    /// @param formatter  A formatter from mlocInitZoneFormatter.
    /// @param zone       The zone.
    /// @param instant    The instant, in Temporal's range.
    /// @param textOut    Where the text goes.
    /// @return mloc_success; mloc_errorCapacity when the text does not
    ///         fit (textOut->length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a zone the library did not make, an
    ///         instant out of range, or a formatter the library did not
    ///         make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocZoneFormatter_Format(const mlocZoneFormatter* formatter,
                                                                const mlocTimeZone* zone,
                                                                const mlocInstant* instant,
                                                                mlocOutput* textOut);

#ifdef __cplusplus
}
#endif

#endif
