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

#ifdef __cplusplus
}
#endif

#endif
