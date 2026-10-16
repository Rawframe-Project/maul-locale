// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Temporal's strings (record mloc-0041): ISO 8601 date-times with RFC
// 9557's annotations, durations, UTC offsets, and time zone and calendar
// strings, read as ECMA-262's grammar reads them. The text is UTF-8;
// every terminal of the grammar is ASCII.

#ifndef MAUL_LOCALE_IXDTF_H
#define MAUL_LOCALE_IXDTF_H

#include "maul-locale/zone.h"

// bool, for C17 callers; C23 and C++ have it built in.
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // The grammar's goals a date-time string may match, as bits; a parse
    // tries those allowed in this order and the first that matches wins,
    // as every list of goals in ECMA-262 has them.
    typedef enum mlocTemporalFormat
    {
        // TemporalDateTimeString[+Zoned]: a time zone annotation required.
        mloc_formatZonedDateTime = 1,
        // TemporalDateTimeString[~Zoned].
        mloc_formatDateTime = 2,
        // TemporalInstantString: Z or an offset required.
        mloc_formatInstant = 4,
        // TemporalTimeString.
        mloc_formatTime = 8,
        // TemporalMonthDayString.
        mloc_formatMonthDay = 16,
        // TemporalYearMonthString.
        mloc_formatYearMonth = 32,
    } mlocTemporalFormat;

    // A part of the parsed text: its first byte and length; a length of 0
    // when the part is absent.
    typedef struct mlocTextSpan
    {
        size_t start;
        size_t length;
    } mlocTextSpan;

    // What a date-time string holds (ECMA-262's ISO Date-Time Parse
    // Record): an ISO date, the year absent for a month-day given without
    // one; a time of day, absent for the start of the day; the UTC
    // designator or an offset; and the time zone annotation and calendar
    // as spans of the text, for the caller to look up.
    typedef struct mlocTemporalParse
    {
        // The goal that matched, one of mlocTemporalFormat.
        uint32_t format;
        int32_t year;
        uint8_t month;
        uint8_t day;
        bool hasYear;
        bool hasTime;
        // A second of 60 reads as 59.
        mlocTime time;
        // Z or z.
        bool utc;
        bool hasOffset;
        // Whether the offset gave seconds, which decides whether a zoned
        // date-time's offset is matched exactly or to the minute.
        bool offsetHasSeconds;
        int64_t offsetNanoseconds;
        // The time zone annotation's identifier, its brackets and
        // critical flag left out.
        mlocTextSpan timeZone;
        // The first u-ca annotation's value.
        mlocTextSpan calendar;
    } mlocTemporalParse;

    /// Reads a date-time string (ECMA-262's ParseISODateTime) as one of
    /// the goals allowed: dates in basic or extended form, times to the
    /// nanosecond, offsets, a time zone annotation and annotations with
    /// their rules for u-ca and for critical flags.
    ///
    /// @param text     The string, UTF-8; it need not end in a zero byte.
    /// @param length   Its length in bytes.
    /// @param formats  The goals allowed, mlocTemporalFormat bits.
    /// @param parseOut Receives what the string holds.
    /// @return mloc_success; mloc_errorSyntax for a string no allowed goal
    ///         matches, or that the grammar's early errors or the annotation
    ///         rules refuse; mloc_errorInvalid for a null pointer or no goal
    ///         allowed.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseTemporalString(const char* text, size_t length,
                                                               uint32_t formats,
                                                               mlocTemporalParse* parseOut);

    /// Reads a UTC offset (ECMA-262's ParseDateTimeUTCOffset): ±HH,
    /// ±HH:MM or ±HHMM, with seconds and up to nine digits of a fraction.
    ///
    /// @param text               The string, UTF-8.
    /// @param length             Its length in bytes.
    /// @param nanosecondsOut     Receives the offset.
    /// @param hasSecondsOut      Receives whether it gave seconds; may be
    ///                           null.
    /// @return mloc_success; mloc_errorSyntax for a string that is no UTC
    ///         offset; mloc_errorInvalid for a null text or nanosecondsOut.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseUTCOffset(const char* text, size_t length,
                                                          int64_t* nanosecondsOut,
                                                          bool* hasSecondsOut);

    // A time zone string read (ECMA-262's Time Zone Identifier Parse
    // Record): a name, which the caller finds with mlocFindTimeZone, or an
    // offset in minutes. utc is set for a string whose only zone is Z,
    // which names UTC.
    typedef struct mlocTimeZoneParse
    {
        mlocTextSpan name;
        bool utc;
        bool hasOffset;
        int16_t offsetMinutes;
    } mlocTimeZoneParse;

    /// Reads a time zone string (ECMA-262's ParseTemporalTimeZoneString):
    /// a time zone identifier, or a date-time string whose annotation, Z
    /// or offset names the zone.
    ///
    /// @param text      The string, UTF-8.
    /// @param length    Its length in bytes.
    /// @param parseOut  Receives the zone read.
    /// @return mloc_success; mloc_errorSyntax for a string that names no
    ///         zone by the grammar; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseTimeZoneString(const char* text, size_t length,
                                                               mlocTimeZoneParse* parseOut);

    /// Reads a calendar string (ECMA-262's ParseTemporalCalendarString):
    /// a date-time string's calendar annotation, an empty span when it has
    /// none (the ISO calendar), or the string itself when it is a calendar
    /// id by the grammar.
    ///
    /// @param text        The string, UTF-8.
    /// @param length      Its length in bytes.
    /// @param calendarOut Receives the calendar id's span.
    /// @return mloc_success; mloc_errorSyntax for a string that is
    ///         neither; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseCalendarString(const char* text, size_t length,
                                                               mlocTextSpan* calendarOut);

    /// Reads a duration string (ECMA-262's ParseTemporalDurationString):
    /// P with years, months, weeks and days, then T with hours, minutes
    /// and seconds, the last of which may have a fraction that carries
    /// down exactly; the fields, as float64 values, pass Temporal's
    /// limits.
    ///
    /// @param text       The string, UTF-8.
    /// @param length     Its length in bytes.
    /// @param valuesOut  Receives the fields.
    /// @return mloc_success; mloc_errorSyntax for a string that is no
    ///         duration; mloc_errorInvalid for a null pointer or fields past
    ///         the limits.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseDuration(const char* text, size_t length,
                                                         mlocDurationValues* valuesOut);

    // How a string shows seconds (ToSecondsStringPrecisionRecord's
    // [[Precision]]). The writers truncate; a caller rounds first, as each
    // toString does.
    typedef enum mlocSecondsPrecision
    {
        // As many digits of a fraction as it needs, none for a whole second.
        mloc_secondsAuto = 0,
        // Hours and minutes alone.
        mloc_secondsMinute = 1,
        // Seconds and the options' fractionDigits digits, 0 to 9.
        mloc_secondsDigits = 2,
    } mlocSecondsPrecision;

    // When a string shows its calendar annotation (Temporal's
    // calendarName option): auto leaves out the ISO calendar's.
    typedef enum mlocShowCalendar
    {
        mloc_showCalendarAuto = 0,
        mloc_showCalendarAlways = 1,
        mloc_showCalendarNever = 2,
        mloc_showCalendarCritical = 3,
    } mlocShowCalendar;

    // When a zoned date-time's string shows its time zone annotation
    // (Temporal's timeZoneName option).
    typedef enum mlocShowTimeZone
    {
        mloc_showTimeZoneAuto = 0,
        mloc_showTimeZoneNever = 1,
        mloc_showTimeZoneCritical = 2,
    } mlocShowTimeZone;

    // What a Temporal string shows, as its toString's options give it.
    // Zeroed, it shows a fraction as long as needed, the calendar unless
    // ISO, and a zoned date-time's offset and zone.
    typedef struct mlocTemporalStringOptions
    {
        // The calendar's id as the caller keeps it (the library maps
        // iso8601 and gregory to one calendar); null for iso8601. It is
        // checked against the grammar's calendar ids.
        const char* calendar;
        size_t calendarLength;
        // An mlocSecondsPrecision, and the digits for mloc_secondsDigits.
        uint8_t secondsPrecision;
        uint8_t fractionDigits;
        // An mlocShowCalendar.
        uint8_t showCalendar;
        // An mlocShowTimeZone.
        uint8_t showTimeZone;
        // Whether a zoned date-time's string leaves out its offset
        // (Temporal's offset option of never).
        bool hideOffset;
    } mlocTemporalStringOptions;

    /// Writes a date (ECMA-262's TemporalDateToString): its ISO year,
    /// month and day and the calendar annotation.
    ///
    /// @param date     The date.
    /// @param options  The calendar and its annotation; may be null.
    /// @param output   Receives the string.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         short; mloc_errorInvalid for a null date or output, a date
    ///         the library did not make, or options out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalDate(
        const mlocDate* date, const mlocTemporalStringOptions* options, mlocOutput* output);

    /// Writes a year-month (ECMA-262's TemporalYearMonthToString): its ISO
    /// year and month, and the reference day with the annotation when the
    /// calendar is not ISO or is shown.
    ///
    /// @param date     The year-month's reference date.
    /// @param options  The calendar and its annotation; may be null.
    /// @param output   Receives the string.
    /// @return As mlocFormatTemporalDate.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatYearMonth(const mlocDate* date,
                                                           const mlocTemporalStringOptions* options,
                                                           mlocOutput* output);

    /// Writes a month-day (ECMA-262's TemporalMonthDayToString): its ISO
    /// month and day, and the reference year with the annotation when the
    /// calendar is not ISO or is shown.
    ///
    /// @param date     The month-day's reference date.
    /// @param options  The calendar and its annotation; may be null.
    /// @param output   Receives the string.
    /// @return As mlocFormatTemporalDate.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatMonthDay(const mlocDate* date,
                                                          const mlocTemporalStringOptions* options,
                                                          mlocOutput* output);

    /// Writes a time of day (ECMA-262's TimeRecordToString) to the
    /// options' precision, truncated.
    ///
    /// @param time     The time.
    /// @param options  The precision; may be null.
    /// @param output   Receives the string.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         short; mloc_errorInvalid for a null time or output, a time
    ///         the library did not make, or options out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalTime(
        const mlocTime* time, const mlocTemporalStringOptions* options, mlocOutput* output);

    /// Writes a date-time (ECMA-262's FormatISODateTime): the date, T,
    /// the time to the options' precision, and the calendar annotation.
    ///
    /// @param dateTime  The date-time.
    /// @param options   The precision, calendar and annotation; may be
    ///                  null.
    /// @param output    Receives the string.
    /// @return As mlocFormatTemporalDate.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTemporalDateTime(
        const mlocDateTime* dateTime, const mlocTemporalStringOptions* options, mlocOutput* output);

    /// Writes an instant (ECMA-262's TemporalInstantToString): its
    /// date-time in universal time and Z, or with a zone its local
    /// date-time and offset rounded to the minute.
    ///
    /// @param instant  The instant.
    /// @param zone     The zone; null for universal time.
    /// @param options  The precision; may be null.
    /// @param output   Receives the string.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         short; mloc_errorInvalid for a null instant or output, an
    ///         instant out of range, a zone the library did not make, or
    ///         options out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatInstant(const mlocInstant* instant,
                                                         const mlocTimeZone* zone,
                                                         const mlocTemporalStringOptions* options,
                                                         mlocOutput* output);

    /// Writes a zoned date-time (ECMA-262's TemporalZonedDateTimeToString):
    /// its local date-time, the offset rounded to the minute, the zone's
    /// identifier and the calendar annotation, each as the options show
    /// them.
    ///
    /// @param zone     The zone.
    /// @param instant  The instant.
    /// @param options  The precision, calendar and what is shown; may be
    ///                 null.
    /// @param output   Receives the string.
    /// @return As mlocFormatInstant.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocFormatZonedDateTime(const mlocTimeZone* zone, const mlocInstant* instant,
                            const mlocTemporalStringOptions* options, mlocOutput* output);

    /// Writes a duration (ECMA-262's TemporalDurationToString): each
    /// field not zero with its designator, the seconds from the exact sum
    /// of seconds and the sub-second fields to the options' precision,
    /// truncated (mloc_secondsMinute is refused).
    ///
    /// @param values   The fields, which pass Temporal's limits.
    /// @param options  The precision; may be null.
    /// @param output   Receives the string.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         short; mloc_errorInvalid for a null pointer, fields past
    ///         the limits or of mixed signs, or options out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocFormatTemporalDuration(const mlocDurationValues* values,
                               const mlocTemporalStringOptions* options, mlocOutput* output);

    /// Writes a UTC offset (ECMA-262's FormatUTCOffsetNanoseconds):
    /// ±HH:MM, with seconds and as many digits of a fraction as it needs.
    ///
    /// @param offsetNanoseconds  The offset, less than a day either way.
    /// @param output             Receives the string.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         short; mloc_errorInvalid for a null output or an offset of
    ///         a day or more.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatUTCOffset(int64_t offsetNanoseconds,
                                                           mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
