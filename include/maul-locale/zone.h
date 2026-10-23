// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Time zones: the IANA time zone database's zones as ECMA-262's Temporal
// and ECMA-402 read them. A zone is found by an identifier, a Zone or Link
// name of the database in any letter case ("America/New_York",
// "asia/calcutta") or a fixed offset ("+05:30"), and keeps the identifier
// as given; two zones are equal when their identifiers resolve to the same
// primary identifier. Its offset at any instant comes from the data's
// zones unit, compiled from the pinned database release
// (mlocGetDataVersion names it), and runs over Temporal's whole range. A
// date and a time of day make a date-time, which a zone maps to the
// instants it names (none in a gap, two in a fold) and back.

#ifndef MAUL_LOCALE_ZONE_H
#define MAUL_LOCALE_ZONE_H

#include "maul-locale/base.h"
#include "maul-locale/calendar.h"
#include "maul-locale/data.h"
#include "maul-locale/decimal.h"

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

    /// Writes the time zone database release a data set's zones are
    /// compiled from, such as "2026e": that of the first source with
    /// zones, which every zone lookup in the set reads. A zones file
    /// (mlocgen zonesfile) put before the compiled-in data gives a
    /// release other than the library's.
    ///
    /// @param data    The data with the zones unit.
    /// @param output  Receives the release's name.
    /// @return mloc_success; mloc_errorCapacity when the buffer is too
    ///         short; mloc_errorInvalid for a null pointer; mloc_errorNotFound
    ///         for data without the zones unit.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetTimeZoneRelease(const mlocDataSet* data,
                                                              mlocOutput* output);

    /// Finds a time zone by its BCP 47 id, the value of a locale's "tz"
    /// key ("usnyc" for America/New_York), as CLDR lists them: the zone
    /// is the id's identifier, found in the set's zones. A deprecated id
    /// ("camtr") finds its replacement's zone; "unk" is refused.
    ///
    /// @param data     The data with the zone codes and the zones units.
    /// @param id       The id, ASCII letters in any case.
    /// @param length   Its length in bytes.
    /// @param zoneOut  Receives the zone; left unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound for an id CLDR does not
    ///         list, a zone the set's zones lack, or data without either
    ///         unit; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindTimeZoneBcp47(const mlocDataSet* data,
                                                             const char* id, size_t length,
                                                             mlocTimeZone* zoneOut);

    /// Writes a zone's BCP 47 id: the id whose CLDR aliases hold the
    /// zone's identifier, or else its primary identifier; for an offset
    /// zone of whole hours from -12 to +14, the id of its Etc/GMT zone
    /// ("utcw05" for -05:00, "utc" for +00:00).
    ///
    /// @param data    The data with the zone codes unit.
    /// @param zone    The zone.
    /// @param output  Receives the id, lower-case.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorNotFound for a zone CLDR gives no id, or data
    ///         without the zone codes unit; mloc_errorInvalid for a null
    ///         pointer or a zone the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeZoneBcp47(const mlocDataSet* data,
                                                               const mlocTimeZone* zone,
                                                               mlocOutput* output);

    /// Finds a time zone by its Windows name ("Eastern Standard Time"),
    /// as CLDR's windowsZones maps it: the zone of the region's mapping,
    /// or of the 001 mapping when the region has none or none is given,
    /// as ICU's TimeZone::getIDForWindowsID does.
    ///
    /// @param data          The data with the zone codes and the zones
    ///                      units.
    /// @param name          The Windows name, ASCII letters in any case.
    /// @param length        Its length in bytes.
    /// @param region        The region, two ASCII letters in any case or
    ///                      three digits; may be null when regionLength
    ///                      is 0.
    /// @param regionLength  Its length in bytes, or 0 for none.
    /// @param zoneOut       Receives the zone; left unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound for a name CLDR does not
    ///         map, a zone the set's zones lack, or data without either
    ///         unit; mloc_errorInvalid for a null pointer or a region
    ///         that is not two letters or three digits.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindTimeZoneWindows(const mlocDataSet* data,
                                                               const char* name, size_t length,
                                                               const char* region,
                                                               size_t regionLength,
                                                               mlocTimeZone* zoneOut);

    /// Writes a zone's Windows name: the name CLDR's windowsZones maps
    /// the zone's BCP 47 id to (mlocFormatTimeZoneBcp47), as ICU's
    /// TimeZone::getWindowsID gives it.
    ///
    /// @param data    The data with the zone codes unit.
    /// @param zone    The zone.
    /// @param output  Receives the name.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorNotFound for a zone with no Windows name, or
    ///         data without the zone codes unit; mloc_errorInvalid for a
    ///         null pointer or a zone the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatTimeZoneWindows(const mlocDataSet* data,
                                                                 const mlocTimeZone* zone,
                                                                 mlocOutput* output);

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

    // How mlocGetInstantForOffset weighs an offset against the zone's
    // rules, as Temporal's offset option: use takes the offset alone;
    // ignore takes the zone's rules alone; prefer takes the offset when
    // the zone gives the date-time that offset, else the zone's rules;
    // reject refuses in that case.
    typedef enum mlocOffsetOption
    {
        mloc_offsetUse = 0,
        mloc_offsetIgnore = 1,
        mloc_offsetPrefer = 2,
        mloc_offsetReject = 3,
    } mlocOffsetOption;

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

    // A time duration, as Temporal's: seconds and the nanoseconds past
    // them, of one sign (nanoseconds -999,999,999 to 999,999,999), the
    // seconds below 2^53 in size.
    typedef struct mlocTimeDuration
    {
        int64_t seconds;
        int32_t nanoseconds;
    } mlocTimeDuration;

    // A duration's time fields, with days of 24 hours.
    typedef struct mlocTimeDurationFields
    {
        int64_t days;
        int64_t hours;
        int64_t minutes;
        int64_t seconds;
        int64_t milliseconds;
        int64_t microseconds;
        int64_t nanoseconds;
    } mlocTimeDurationFields;

    // Temporal's units of a day and less.
    typedef enum mlocTimeUnit
    {
        mloc_timeUnitDay = 0,
        mloc_timeUnitHour = 1,
        mloc_timeUnitMinute = 2,
        mloc_timeUnitSecond = 3,
        mloc_timeUnitMillisecond = 4,
        mloc_timeUnitMicrosecond = 5,
        mloc_timeUnitNanosecond = 6,
    } mlocTimeUnit;

    // How a difference of times is rounded and given (Temporal's
    // difference settings): rounded to a multiple of increment of
    // smallestUnit by mode, then given in units up to largestUnit.
    typedef struct mlocTimeRounding
    {
        mlocTimeUnit largestUnit;
        mlocTimeUnit smallestUnit;
        uint32_t increment;
        mlocRoundingMode mode;
    } mlocTimeRounding;

    /// Makes a time duration of fields (ECMA-262's
    /// TimeDurationFromComponents, with days of 24 hours as
    /// Add24HourDaysToTimeDuration counts them). A field too large for
    /// int64_t is the host's to fold in.
    ///
    /// @param fields       The fields, of any signs.
    /// @param durationOut  Receives the duration; left unchanged on
    ///                     failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, or a
    ///         field or the total of 2^53 seconds or more in size.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeTimeDuration(const mlocTimeDurationFields* fields,
                                                            mlocTimeDuration* durationOut);

    /// Gives a time duration in fields up to a largest unit, each of the
    /// duration's sign (ECMA-262's TemporalDurationFromInternal for its
    /// time part): with days, hours below 24; with hours, minutes below
    /// 60; and so on down to the largest unit, which takes the rest.
    ///
    /// @param duration     The duration.
    /// @param largestUnit  The largest unit to give.
    /// @param fieldsOut    Receives the fields.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         duration the library would not make, an unknown unit, or a
    ///         largest field past int64_t: nanoseconds of a duration of
    ///         more than about 292 years, microseconds of more than about
    ///         292,000 years. The host then forms that field, which
    ///         Temporal keeps as a float64, from the duration.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocBalanceTimeDuration(const mlocTimeDuration* duration,
                                                               mlocTimeUnit largestUnit,
                                                               mlocTimeDurationFields* fieldsOut);

    /// Adds two time durations (ECMA-262's AddTimeDuration), as
    /// Temporal's Duration.add reads them with days of 24 hours.
    ///
    /// @param one          The first duration.
    /// @param two          The second.
    /// @param durationOut  Receives the sum, of one sign.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         duration the library would not make, or a sum of 2^53
    ///         seconds or more in size.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocAddTimeDurations(const mlocTimeDuration* one,
                                                            const mlocTimeDuration* two,
                                                            mlocTimeDuration* durationOut);

    /// Rounds a time duration to a multiple of an increment of a unit
    /// (ECMA-262's RoundTimeDuration), as Temporal's Duration.round reads
    /// it without a date to start from, days being 24 hours. A day takes
    /// an increment of 1 to 10^9; hours to nanoseconds one dividing the
    /// next unit (24, 60, 60, 1000, 1000, 1000) into more than one part.
    ///
    /// @param duration     The duration.
    /// @param unit         The unit.
    /// @param increment    The increment.
    /// @param mode         The rounding mode.
    /// @param durationOut  Receives the rounded duration.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         duration the library would not make, a unit, increment or
    ///         mode refused, or a result of 2^53 seconds or more in size.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRoundTimeDuration(const mlocTimeDuration* duration,
                                                             mlocTimeUnit unit, uint32_t increment,
                                                             mlocRoundingMode mode,
                                                             mlocTimeDuration* durationOut);

    /// Gives the total of a unit in a time duration (ECMA-262's
    /// TotalTimeDuration), days being 24 hours, as Temporal's
    /// Duration.total reads it without a date to start from: the float64
    /// nearest the exact quotient, which the library finds with integers
    /// alone.
    ///
    /// @param duration  The duration.
    /// @param unit      The unit.
    /// @param totalOut  Receives the total.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         duration the library would not make or an unknown unit.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocTotalTimeDuration(const mlocTimeDuration* duration,
                                                             mlocTimeUnit unit, double* totalOut);

    /// Adds a time duration to a time of day (ECMA-262's AddTime), giving
    /// the time and the days it passes, as Temporal's PlainTime.add (which
    /// drops the days) and PlainDateTime.add read it. Subtracting is
    /// adding the negated duration.
    ///
    /// @param time      The time of day.
    /// @param duration  The duration.
    /// @param timeOut   Receives the time.
    /// @param daysOut   Receives the days passed, negative going back; may
    ///                  be null.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, or a
    ///         time or duration the library would not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocAddToTime(const mlocTime* time,
                                                     const mlocTimeDuration* duration,
                                                     mlocTime* timeOut, int64_t* daysOut);

    /// Computes the duration from one time of day to another, rounded and
    /// given in fields, as Temporal's PlainTime.until does: units hour to
    /// nanosecond, the smallest no larger than the largest, an increment
    /// dividing the unit's next (24 for hours, 60 for minutes and seconds,
    /// 1000 for the rest) into more than one part. Temporal's since is
    /// this with the mode negated (ceil for floor, half-ceil for
    /// half-floor, and back) and the fields negated.
    ///
    /// @param one        The time from.
    /// @param two        The time to.
    /// @param rounding   The units, increment and mode.
    /// @param fieldsOut  Receives the fields; days are 0.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a time
    ///         the library did not make, a unit, increment or mode
    ///         refused.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocTimeUntil(const mlocTime* one, const mlocTime* two,
                                                     const mlocTimeRounding* rounding,
                                                     mlocTimeDurationFields* fieldsOut);

    /// Rounds a time of day to a multiple of an increment of a unit
    /// (ECMA-262's RoundTime), as Temporal's PlainTime.round (units hour
    /// to nanosecond, which drops the days) and PlainDateTime.round read
    /// it. A unit of hours to nanoseconds takes an increment dividing the
    /// next unit (24, 60, 60, 1000, 1000, 1000) into more than one part;
    /// a day takes 1, giving midnight of the day or the next.
    ///
    /// @param time       The time of day.
    /// @param unit       The unit.
    /// @param increment  The increment.
    /// @param mode       The rounding mode.
    /// @param timeOut    Receives the time.
    /// @param daysOut    Receives 1 when the rounding reaches the next
    ///                   midnight, else 0; may be null.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a time
    ///         the library did not make, or a unit, increment or mode
    ///         refused.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRoundTime(const mlocTime* time, mlocTimeUnit unit,
                                                     uint32_t increment, mlocRoundingMode mode,
                                                     mlocTime* timeOut, int64_t* daysOut);

    // Temporal's units, a year to a nanosecond.
    typedef enum mlocUnit
    {
        mloc_unitYear = 0,
        mloc_unitMonth = 1,
        mloc_unitWeek = 2,
        mloc_unitDay = 3,
        mloc_unitHour = 4,
        mloc_unitMinute = 5,
        mloc_unitSecond = 6,
        mloc_unitMillisecond = 7,
        mloc_unitMicrosecond = 8,
        mloc_unitNanosecond = 9,
    } mlocUnit;

    // A duration as Temporal keeps it inside (an Internal Duration
    // Record): the date fields and a time duration, of one sign.
    typedef struct mlocDateTimeDuration
    {
        mlocDateDuration date;
        mlocTimeDuration time;
    } mlocDateTimeDuration;

    // How a difference of date-times is rounded (Temporal's difference
    // settings): to a multiple of increment of smallestUnit by mode,
    // given in units up to largestUnit.
    typedef struct mlocDateTimeRounding
    {
        mlocUnit largestUnit;
        mlocUnit smallestUnit;
        uint32_t increment;
        mlocRoundingMode mode;
    } mlocDateTimeRounding;

    // Temporal.Duration's fields as a host keeps them: float64 integers of
    // one sign.
    typedef struct mlocDurationValues
    {
        double years;
        double months;
        double weeks;
        double days;
        double hours;
        double minutes;
        double seconds;
        double milliseconds;
        double microseconds;
        double nanoseconds;
    } mlocDurationValues;

    /// Makes a duration's internal record from its fields (ECMA-262's
    /// IsValidDuration and ToInternalDurationRecord): the years, months,
    /// weeks and days as they are, the hours to nanoseconds as one exact
    /// time duration. The doubles are read from their bits and summed
    /// exactly, as the specification requires of nanoseconds and
    /// microseconds past 2^53.
    ///
    /// @param values       The fields.
    /// @param durationOut  Receives the record.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a field
    ///         that is no finite integer, fields of two signs, years,
    ///         months or weeks of 2^32 or more in size, or days to
    ///         nanoseconds of 2^53 seconds or more.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeDuration(const mlocDurationValues* values,
                                                        mlocDateTimeDuration* durationOut);

    /// Gives a duration's fields from an internal record (ECMA-262's
    /// TemporalDurationFromInternal): the time part balanced up to the
    /// largest unit, and into days for a date unit, each field the float64
    /// nearest it. The fields are refused, as CreateTemporalDuration
    /// refuses them, when their float64 values pass Temporal's limits.
    ///
    /// @param duration     The record, of one sign.
    /// @param largestUnit  The largest unit.
    /// @param valuesOut    Receives the fields.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a record
    ///         of two signs or a time part the library would not make, an
    ///         unknown unit, or fields past Temporal's limits.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetDurationValues(const mlocDateTimeDuration* duration,
                                                             mlocUnit largestUnit,
                                                             mlocDurationValues* valuesOut);

    /// Adds a duration to a date-time, as Temporal's PlainDateTime.add
    /// does (AddDurationToDateTime): the time first, its days passed added
    /// to the duration's days, then the date part in the date's calendar as
    /// mlocAddToDate adds it. Subtracting is adding the negated duration.
    ///
    /// @param dateTime     The date-time.
    /// @param duration     The duration, its days of 24 hours.
    /// @param overflow     What a month code or day the result lacks does.
    /// @param dateTimeOut  Receives the date-time; left unchanged on
    ///                     failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         date-time the library did not make, a duration of two signs
    ///         or out of its limits, a month code or day refused, or a result
    ///         outside Temporal's limits for a date-time.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocAddToDateTime(const mlocDateTime* dateTime,
                                                         const mlocDateTimeDuration* duration,
                                                         mlocOverflow overflow,
                                                         mlocDateTime* dateTimeOut);

    /// Computes the duration from one date-time to another of the same
    /// calendar, rounded, as Temporal's PlainDateTime.until does
    /// (DifferencePlainDateTimeWithRounding): the date difference to the
    /// second date, or the day before or after it when the times run the
    /// other way, the time difference beside it, then the rounding of
    /// RoundRelativeDuration. A largest time unit folds the days into the
    /// time. Increments: 1 to 10^9 for date units; for time units one that
    /// divides the next unit (24, 60, 60, 1000, 1000, 1000) into more than
    /// one part. Temporal's since is this with the mode negated and the
    /// result negated. The time part is given to mlocBalanceTimeDuration
    /// with the largest unit (or days, for a date unit, whose days the host
    /// adds to the date part's).
    ///
    /// @param one          The date-time from.
    /// @param two          The date-time to.
    /// @param rounding     The units, increment and mode.
    /// @param durationOut  Receives the duration; left unchanged on
    ///                     failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         date-time the library did not make or out of the limits,
    ///         date-times of two calendars, a unit, increment or mode
    ///         refused, or a rounding that reaches a date out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeUntil(const mlocDateTime* one,
                                                         const mlocDateTime* two,
                                                         const mlocDateTimeRounding* rounding,
                                                         mlocDateTimeDuration* durationOut);

    /// Rounds a date-time's time of day as mlocRoundTime does, carrying a
    /// next midnight into the date (Temporal's PlainDateTime.round,
    /// RoundISODateTime).
    ///
    /// @param dateTime     The date-time.
    /// @param unit         The unit, a day to a nanosecond.
    /// @param increment    The increment, as mlocRoundTime takes it.
    /// @param mode         The rounding mode.
    /// @param dateTimeOut  Receives the date-time; left unchanged on
    ///                     failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         date-time the library did not make, a unit, increment or mode
    ///         refused, or a result outside Temporal's limits.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRoundDateTime(const mlocDateTime* dateTime,
                                                         mlocTimeUnit unit, uint32_t increment,
                                                         mlocRoundingMode mode,
                                                         mlocDateTime* dateTimeOut);

    /// Gives the total of a unit in the duration from one date-time to
    /// another in the same calendar (ECMA-262's
    /// DifferencePlainDateTimeWithTotal), as Temporal's Duration.total
    /// reads it from a plain date or date-time: the float64 nearest the
    /// exact total, which the library finds with integers alone. A
    /// calendar unit counts the part of the unit reached, a day 24 hours.
    ///
    /// @param one       The date-time from.
    /// @param two       The date-time to.
    /// @param unit      The unit.
    /// @param totalOut  Receives the total.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         date-time the library would not make or outside the
    ///         limits, two calendars, an unknown unit, or a date the
    ///         total reaches outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateTimeTotal(const mlocDateTime* one,
                                                         const mlocDateTime* two, mlocUnit unit,
                                                         double* totalOut);

    /// Adds a time duration to an instant, as Temporal's Instant.add does
    /// (AddDurationToInstant; the host refuses a duration with date
    /// units). Subtracting is adding the negated duration.
    ///
    /// @param instant     The instant.
    /// @param duration    The time duration.
    /// @param instantOut  Receives the instant; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, an
    ///         instant or duration the library would not make, or a result
    ///         outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocAddToInstant(const mlocInstant* instant,
                                                        const mlocTimeDuration* duration,
                                                        mlocInstant* instantOut);

    /// Computes the time from one instant to another, rounded, as
    /// Temporal's Instant.until does (DifferenceEpochNanoseconds): units
    /// hour to nanosecond, the smallest no larger than the largest, an
    /// increment dividing the unit's next (24, 60, 60, 1000, 1000, 1000)
    /// into more than one part. The largest unit is the host's to give to
    /// mlocBalanceTimeDuration. Temporal's since is this with the mode
    /// negated and the result negated.
    ///
    /// @param one          The instant from.
    /// @param two          The instant to.
    /// @param rounding     The units, increment and mode.
    /// @param durationOut  Receives the duration.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, an
    ///         instant the library would not make, or a unit, increment or
    ///         mode refused.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInstantUntil(const mlocInstant* one,
                                                        const mlocInstant* two,
                                                        const mlocTimeRounding* rounding,
                                                        mlocTimeDuration* durationOut);

    /// Rounds an instant to a multiple of an increment of a unit, as
    /// Temporal's Instant.round does (RoundEpochNanoseconds): the mode
    /// applied as if the instant were positive, so floor is always toward
    /// the past. Units hour to nanosecond; the increment must divide a
    /// day of the unit (24 hours, 1440 minutes, 86400 seconds, and so
    /// on), a whole day included.
    ///
    /// @param instant     The instant.
    /// @param unit        The unit.
    /// @param increment   The increment.
    /// @param mode        The rounding mode.
    /// @param instantOut  Receives the instant.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, an
    ///         instant the library would not make, or a unit, increment or
    ///         mode refused.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRoundInstant(const mlocInstant* instant,
                                                        mlocTimeUnit unit, uint32_t increment,
                                                        mlocRoundingMode mode,
                                                        mlocInstant* instantOut);

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

    /// Finds the instant a local date-time names with an offset from
    /// universal time (ECMA-262's InterpretISODateTimeOffset), weighing
    /// the offset against the zone's rules as the option says. With
    /// matchMinutes, as for a string that gives the offset to the minute,
    /// the zone's offset rounded to the minute also matches.
    ///
    /// @param zone                The zone.
    /// @param dateTime            The local date-time.
    /// @param offsetNanoseconds   The offset, under a day either way.
    /// @param option              How the offset is weighed.
    /// @param disambiguation      How to choose when the zone's rules
    ///                            decide.
    /// @param matchMinutes        Whether an offset rounded to the minute
    ///                            matches.
    /// @param instantOut          Receives the instant.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or date-time the library did not make, an offset of a day
    ///         or more, another option or disambiguation, an offset
    ///         refused by mloc_offsetReject, a gap or fold refused by
    ///         mloc_disambiguationReject, or an instant outside Temporal's
    ///         range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetInstantForOffset(
        const mlocTimeZone* zone, const mlocDateTime* dateTime, int64_t offsetNanoseconds,
        mlocOffsetOption option, mlocDisambiguation disambiguation, bool matchMinutes,
        mlocInstant* instantOut);

    /// Adds a duration to a zoned date-time (ECMA-262's
    /// AddZonedDateTime): the date part to the local date in its
    /// calendar, its time kept and found again in the zone (the earlier
    /// of a fold, past a gap by its length), then the time part as exact
    /// time.
    ///
    /// @param zone        The zone.
    /// @param calendar    The calendar the date part is added in.
    /// @param instant     The zoned date-time's instant.
    /// @param duration    The duration, of one sign.
    /// @param overflow    Constrain or refuse a day past its month's
    ///                    end.
    /// @param instantOut  Receives the instant.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone,
    ///         instant or duration the library did not make, a calendar
    ///         out of range, a duration of two signs or too many days, a
    ///         date refused by mloc_overflowReject, or a date-time or
    ///         instant outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocAddToZonedDateTime(
        const mlocTimeZone* zone, mlocCalendar calendar, const mlocInstant* instant,
        const mlocDateTimeDuration* duration, mlocOverflow overflow, mlocInstant* instantOut);

    /// Finds the rounded duration from one zoned date-time to another in
    /// the same zone and calendar (ECMA-262's
    /// DifferenceZonedDateTimeWithRounding): with a largest time unit the
    /// exact time between them, else the date difference of their local
    /// dates and the exact time that remains, rounded relative to the
    /// first with days as long as the zone makes them. The host balances
    /// the time part to hours with mlocBalanceTimeDuration. since is
    /// until with the mode negated and the fields negated. Temporal's
    /// until and since give a zero duration for equal instants before
    /// this; Duration.round does not, and rounding a zero duration near
    /// the range's end can be refused.
    ///
    /// @param zone         The zone.
    /// @param calendar     The calendar.
    /// @param one          The first instant.
    /// @param two          The second instant.
    /// @param rounding     The units, increment and mode, as for
    ///                     mlocDateTimeUntil.
    /// @param durationOut  Receives the duration.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or instant the library did not make, a calendar out of
    ///         range, units, an increment or a mode refused, or a date or
    ///         instant the rounding reaches outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocZonedDateTimeUntil(const mlocTimeZone* zone,
                                                              mlocCalendar calendar,
                                                              const mlocInstant* one,
                                                              const mlocInstant* two,
                                                              const mlocDateTimeRounding* rounding,
                                                              mlocDateTimeDuration* durationOut);

    /// Gives the total of a unit in the duration from one zoned date-time
    /// to another in the same zone and calendar (ECMA-262's
    /// DifferenceZonedDateTimeWithTotal), as Temporal's Duration.total
    /// reads it from a zoned date-time: the float64 nearest the exact
    /// total, which the library finds with integers alone. A day, like
    /// the calendar units, is as long as the zone makes it; a time unit
    /// counts exact time.
    ///
    /// @param zone      The zone.
    /// @param calendar  The calendar.
    /// @param one       The first instant.
    /// @param two       The second instant.
    /// @param unit      The unit.
    /// @param totalOut  Receives the total.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or instant the library did not make, a calendar out of
    ///         range, an unknown unit, or a date or instant the total
    ///         reaches outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocZonedDateTimeTotal(const mlocTimeZone* zone,
                                                              mlocCalendar calendar,
                                                              const mlocInstant* one,
                                                              const mlocInstant* two, mlocUnit unit,
                                                              double* totalOut);

    /// Rounds a zoned date-time (Temporal.ZonedDateTime's round): to a
    /// day, the nearer start of a day as long as the zone makes it;
    /// otherwise its local date-time rounded and found again with its
    /// offset preferred.
    ///
    /// @param zone        The zone.
    /// @param instant     The zoned date-time's instant.
    /// @param unit        The unit, day to nanosecond.
    /// @param increment   1 for a day; for another unit below and
    ///                    dividing the next larger.
    /// @param mode        The rounding mode.
    /// @param instantOut  Receives the instant.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a zone
    ///         or instant the library did not make, a unit, increment or
    ///         mode refused, or an instant outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRoundZonedDateTime(const mlocTimeZone* zone,
                                                              const mlocInstant* instant,
                                                              mlocTimeUnit unit, uint32_t increment,
                                                              mlocRoundingMode mode,
                                                              mlocInstant* instantOut);

#ifdef __cplusplus
}
#endif

#endif
