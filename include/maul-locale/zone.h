// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time zones (record mloc-0041): the IANA time zone database's zones as
// ECMA-262's Temporal and ECMA-402 read them. A zone is found by an
// identifier, a Zone or Link name of the database in any letter case
// ("America/New_York", "asia/calcutta") or a fixed offset ("+05:30"), and
// keeps the identifier as given; two zones are equal when their
// identifiers resolve to the same primary identifier. Its offset at any
// instant comes from the data's zones unit, compiled from the pinned
// database release (mlocGetDataVersion names it), and runs over
// Temporal's whole range. A date and a time of day make a date-time,
// which a zone maps to the instants it names (none in a gap, two in a
// fold) and back.

#ifndef MAUL_LOCALE_ZONE_H
#define MAUL_LOCALE_ZONE_H

#include "maul-locale/base.h"
#include "maul-locale/calendar.h"
#include "maul-locale/data.h"

// bool, for C17 callers; C23 and C++ have it built in.
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // An exact time: seconds since January 1 of 1970 in universal time, and the
    // nanoseconds past them (0 to 999,999,999). Temporal's range is 10^8
    // days either side of 1970: seconds from -8,640,000,000,000 to
    // 8,640,000,000,000, the last with no nanoseconds.
    typedef struct mlocInstant
    {
        int64_t seconds;
        int32_t nanoseconds;
    } mlocInstant;

    // A time zone, made by mlocFindTimeZone or mlocGetTimeZone and read
    // through the functions here; its fields are the library's. A named
    // zone points into its data, which must outlive it.
    typedef struct mlocTimeZone
    {
        // The zones unit, or null for an offset zone.
        const void* zones;
        uint32_t zonesSize;
        // The identifier's place in the unit.
        uint16_t identifier;
        // An offset zone's offset in minutes, -1439 to 1439.
        int16_t offsetMinutes;
    } mlocTimeZone;

    // Which way mlocGetTimeZoneTransition looks.
    typedef enum mlocTransitionDirection
    {
        mloc_transitionNext = 0,
        mloc_transitionPrevious = 1,
    } mlocTransitionDirection;

    /// Finds a time zone by its identifier: an available Zone or Link
    /// name compared with ASCII letters folded, or an offset as
    /// ECMA-262's UTCOffset without seconds: a sign ("+", "-" or U+2212)
    /// and hours 00 to 23, then optionally minutes, after a colon or not
    /// ("+05", "+0530", "-05:30").
    ///
    /// @param data     The data with the zones unit; may be null for an
    ///                 offset.
    /// @param id       The identifier, UTF-8.
    /// @param length   Its length in bytes.
    /// @param zoneOut  Receives the zone; left unchanged on failure.
    /// @return mloc_success; mloc_errorSyntax for an offset that is not
    ///         one of those forms; mloc_errorNotFound for a name the data
    ///         does not have, or data without the zones unit;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindTimeZone(const mlocDataSet* data, const char* id,
                                                        size_t length, mlocTimeZone* zoneOut);

    /// Writes a zone's identifier in its normalized form: a name in the
    /// database's letter case, as given otherwise ("Asia/Calcutta"
    /// stays itself), or an offset as "+HH:MM" ("+00:00" for zero). It
    /// reports no parts.
    ///
    /// @param zone    The zone.
    /// @param output  Where the identifier goes.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorInvalid for a null pointer or a zone the library
    ///         did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeZoneId(const mlocTimeZone* zone,
                                                            mlocOutput* output);

    /// Writes a zone's primary identifier, as ECMA-402's
    /// AvailableNamedTimeZoneIdentifiers resolves it ("Asia/Calcutta"
    /// gives "Asia/Kolkata", "Etc/GMT" gives "UTC"); an offset zone's
    /// is its identifier. It reports no parts.
    ///
    /// @param zone    The zone.
    /// @param output  Where the identifier goes.
    /// @return As mlocFormatTimeZoneId.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatPrimaryTimeZoneId(const mlocTimeZone* zone,
                                                                   mlocOutput* output);

    /// Tells whether two zones are equal as ECMA-262's TimeZoneEquals
    /// says: named zones whose primary identifiers are the same, or
    /// offset zones of the same offset. A named zone never equals an
    /// offset zone, even "UTC" and "+00:00".
    ///
    /// @param one       A zone.
    /// @param two       Another.
    /// @param equalOut  Receives whether they are equal.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or a
    ///         zone the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocTimeZonesEqual(const mlocTimeZone* one,
                                                          const mlocTimeZone* two, bool* equalOut);

    /// Returns the number of primary time zone identifiers the data has,
    /// the list ECMA-402's Intl.supportedValuesOf("timeZone") gives.
    ///
    /// @param data  The data with the zones unit.
    /// @return The count; 0 for data without the zones unit or a null
    ///         pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_API size_t mlocGetTimeZoneCount(const mlocDataSet* data);

    /// Returns the zone of a primary identifier, in the order of the
    /// identifiers' bytes ("Etc/UTC" before "Etc/Universal" were both
    /// primary).
    ///
    /// @param data     The data with the zones unit.
    /// @param index    Below mlocGetTimeZoneCount.
    /// @param zoneOut  Receives the zone.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or an
    ///         index out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZone(const mlocDataSet* data, size_t index,
                                                       mlocTimeZone* zoneOut);

    /// Returns a zone's offset from universal time at an instant, in
    /// seconds: local time is the instant plus the offset. The database's
    /// offsets before 1900 or so are local mean time, to the second.
    ///
    /// @param zone              The zone.
    /// @param instant           An instant in Temporal's range.
    /// @param offsetSecondsOut  Receives the offset.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         the library did not make or an instant out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZoneOffset(const mlocTimeZone* zone,
                                                             const mlocInstant* instant,
                                                             int32_t* offsetSecondsOut);

    /// Finds a zone's first transition strictly after an instant, or its
    /// last strictly before, as Temporal's getTimeZoneTransition: the
    /// first instant of a new offset (one that changes only whether
    /// daylight saving time is in effect is not a transition).
    ///
    /// @param zone           The zone.
    /// @param instant        An instant in Temporal's range.
    /// @param direction      Next or previous.
    /// @param transitionOut  Receives the transition, whose nanoseconds
    ///                       are 0.
    /// @return mloc_success; mloc_errorNotFound when there is none within
    ///         Temporal's range (an offset zone has none);
    ///         mloc_errorInvalid for a null pointer, a zone the library did
    ///         not make, an instant out of range or another direction.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZoneTransition(const mlocTimeZone* zone,
                                                                 const mlocInstant* instant,
                                                                 mlocTransitionDirection direction,
                                                                 mlocInstant* transitionOut);

    // A time of day, as Temporal's PlainTime: hour 0 to 23, minute and
    // second 0 to 59, and the nanoseconds past the second (0 to
    // 999,999,999: Temporal's millisecond, microsecond and nanosecond
    // fields together).
    typedef struct mlocTime
    {
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        // Zero.
        uint8_t reserved;
        uint32_t nanosecond;
    } mlocTime;

    // The fields a time of day is made from, any integers.
    typedef struct mlocTimeInput
    {
        int64_t hour;
        int64_t minute;
        int64_t second;
        int64_t millisecond;
        int64_t microsecond;
        int64_t nanosecond;
    } mlocTimeInput;

    // A date and a time of day, as Temporal's PlainDateTime.
    typedef struct mlocDateTime
    {
        mlocDate date;
        mlocTime time;
    } mlocDateTime;

    // Which instant a local date-time names when a zone's clocks skip it
    // or repeat it, as Temporal's disambiguation option: compatible takes
    // the earlier of a fold and moves forward across a gap by its
    // length; earlier takes the earlier and moves back; later takes the
    // later and moves forward; reject refuses both.
    typedef enum mlocDisambiguation
    {
        mloc_disambiguationCompatible = 0,
        mloc_disambiguationEarlier = 1,
        mloc_disambiguationLater = 2,
        mloc_disambiguationReject = 3,
    } mlocDisambiguation;

    // The instants a zone's local date-time names, in time order.
    typedef struct mlocPossibleInstants
    {
        // The earliest and the latest: both zero when count is 0, the
        // same when it is 1.
        mlocInstant earliest;
        mlocInstant latest;
        // How many: 0 in a gap, 1 most of the time, 2 in a fold (more
        // only were a zone's folds to overlap).
        uint32_t count;
    } mlocPossibleInstants;

    /// Makes a time of day from its fields, as Temporal's RegulateTime
    /// does: a field out of its range is clamped into it
    /// (mloc_overflowConstrain; a leap second's 60 becomes 59) or refused
    /// (mloc_overflowReject). Milliseconds, microseconds and nanoseconds
    /// range 0 to 999 each.
    ///
    /// @param input     The fields.
    /// @param overflow  What a field out of range does.
    /// @param timeOut   Receives the time; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, another
    ///         overflow, or a field refused.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeTime(const mlocTimeInput* input,
                                                    mlocOverflow overflow, mlocTime* timeOut);

    /// Makes a date-time of a date and a time of day, within Temporal's
    /// limits for one: every date and time but midnight at the start of
    /// the first day (April 19 of year -271821 in the ISO calendar), which
    /// lies a whole day before the first instant.
    ///
    /// @param date         The date.
    /// @param time         The time of day.
    /// @param dateTimeOut  Receives the date-time; left unchanged on
    ///                     failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a date
    ///         or time the library did not make, or that midnight.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeDateTime(const mlocDate* date, const mlocTime* time,
                                                        mlocDateTime* dateTimeOut);

    /// Finds the instants at which a zone's clocks read a date-time
    /// (ECMA-262's GetPossibleEpochNanoseconds): none when a forward
    /// change skips it, two when a backward change repeats it. The
    /// date-time's calendar does not matter.
    ///
    /// @param zone         The zone.
    /// @param dateTime     The local date-time.
    /// @param instantsOut  Receives the instants.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or date-time the library did not make, or an instant
    ///         outside Temporal's range among them.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetPossibleInstants(const mlocTimeZone* zone,
                                                               const mlocDateTime* dateTime,
                                                               mlocPossibleInstants* instantsOut);

    /// Finds the one instant a zone's local date-time names, choosing in
    /// a gap or fold as the disambiguation says (ECMA-262's
    /// GetEpochNanosecondsFor). In a gap the date-time moves by the gap's
    /// length: for 02:30 in a gap from 02:00 to 03:00, compatible and
    /// later give 03:30, earlier 01:30.
    ///
    /// @param zone            The zone.
    /// @param dateTime        The local date-time.
    /// @param disambiguation  How to choose.
    /// @param instantOut      Receives the instant.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or date-time the library did not make, another
    ///         disambiguation, a gap or fold refused by
    ///         mloc_disambiguationReject, or an instant outside Temporal's
    ///         range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetInstantFor(const mlocTimeZone* zone,
                                                         const mlocDateTime* dateTime,
                                                         mlocDisambiguation disambiguation,
                                                         mlocInstant* instantOut);

    /// Finds the first instant of a date in a zone (ECMA-262's
    /// GetStartOfDay): midnight's earlier instant, or the end of the gap
    /// when a forward change skips midnight.
    ///
    /// @param zone        The zone.
    /// @param date        The date.
    /// @param instantOut  Receives the instant.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or date the library did not make, or an instant outside
    ///         Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetStartOfDay(const mlocTimeZone* zone,
                                                         const mlocDate* date,
                                                         mlocInstant* instantOut);

    /// Finds a zone's local date-time at an instant, in a calendar
    /// (ECMA-262's GetISODateTimeFor): the instant plus the zone's offset
    /// then.
    ///
    /// @param zone         The zone.
    /// @param instant      An instant in Temporal's range.
    /// @param calendar     The calendar the date is read in.
    /// @param dateTimeOut  Receives the date-time.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         the library did not make, an instant out of range or a
    ///         calendar out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetDateTimeFor(const mlocTimeZone* zone,
                                                          const mlocInstant* instant,
                                                          mlocCalendar calendar,
                                                          mlocDateTime* dateTimeOut);

#ifdef __cplusplus
}
#endif

#endif
