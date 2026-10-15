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
// Temporal's whole range.

#ifndef MAUL_LOCALE_ZONE_H
#define MAUL_LOCALE_ZONE_H

#include "maul-locale/base.h"
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

#ifdef __cplusplus
}
#endif

#endif
