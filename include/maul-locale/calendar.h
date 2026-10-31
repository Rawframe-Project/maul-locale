// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Calendars (UTS #35 Part 4): dates as values in CLDR's calendars, as
// Temporal models them. A date is a day, counted from January 1 of year 1
// in the proleptic Gregorian calendar (day 1), read in a calendar; its
// fields (year, era, month code, day) are computed from it.

#ifndef MAUL_LOCALE_CALENDAR_H
#define MAUL_LOCALE_CALENDAR_H

#include "maul-locale/base.h"
#include "maul-locale/decimal.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // CLDR's calendars, by their BCP 47 ids.
    typedef int32_t mlocCalendar;
    enum
    {
        // "gregory": the proleptic Gregorian calendar, eras ce and bce.
        mloc_calendarGregorian = 0,
        // "iso8601": the Gregorian calendar with no era.
        mloc_calendarIso8601 = 1,
        // "buddhist": Gregorian, years of the Buddhist era (be).
        mloc_calendarBuddhist = 2,
        // "roc": Gregorian, years of the Republic of China (roc, broc).
        mloc_calendarRoc = 3,
        // "japanese": Gregorian, years of Japan's eras from Meiji 6
        // (1873), when Japan took the Gregorian calendar; ce and bce
        // before.
        mloc_calendarJapanese = 4,
        // "coptic": thirteen months, era am.
        mloc_calendarCoptic = 5,
        // "ethiopic": the Coptic months, eras am and aa.
        mloc_calendarEthiopic = 6,
        // "ethioaa": the Coptic months, era aa alone.
        mloc_calendarEthiopicAmeteAlem = 7,
        // "indian": the Indian national calendar, era shaka.
        mloc_calendarIndian = 8,
        // "persian": the Solar Hijri calendar, era ap.
        mloc_calendarPersian = 9,
        // "hebrew": era am, with a leap month.
        mloc_calendarHebrew = 10,
        // "islamic-civil": tabular, civil epoch, eras ah and bh.
        mloc_calendarIslamicCivil = 11,
        // "islamic-tbla": tabular, astronomical epoch, eras ah and bh.
        mloc_calendarIslamicTbla = 12,
        // "islamic-umalqura": Umm al-Qura, eras ah and bh.
        mloc_calendarIslamicUmalqura = 13,
        // "islamic": read as Umm al-Qura.
        mloc_calendarIslamic = 14,
        // "islamic-rgsa": read as Umm al-Qura.
        mloc_calendarIslamicRgsa = 15,
        // "chinese": lunisolar, with leap months, no era.
        mloc_calendarChinese = 16,
        // "dangi": the Korean lunisolar calendar, no era.
        mloc_calendarDangi = 17,
    };

    // What a field out of range does: clamped to the nearest valid value,
    // or refused (Temporal's overflow option).
    typedef int32_t mlocOverflow;
    enum
    {
        mloc_overflowConstrain = 0,
        mloc_overflowReject = 1,
    };

    // The first and last day a date may be: Temporal's range,
    // April 19 of year -271821 and September 13 of 275760 in the ISO
    // calendar.
    enum
    {
        mlocDateMinDay = -99280838,
        mlocDateMaxDay = 100719163,
    };

    // A date: a day and the calendar it is read in. Two dates are the same
    // day when their days are equal, whatever their calendars.
    typedef struct mlocDate
    {
        // Day 1 is January 1 of year 1 in the proleptic Gregorian calendar.
        int32_t day;
        mlocCalendar calendar;
    } mlocDate;

    // The fields of a date in its calendar.
    typedef struct mlocDateFields
    {
        // The calendar's year counted on across eras (Temporal's year):
        // 0 is 1 BCE in the Gregorian calendar.
        int32_t year;
        // The year in its era, and the era's code (Temporal's: "ce", "bce",
        // "be", "roc", "broc", "am", "aa", "shaka", "ap", "ah", "bh", and
        // Japan's "meiji", "taisho", "showa", "heisei", "reiwa"),
        // NUL-padded; eraLength 0 and eraYear the year in a calendar with no
        // eras.
        int32_t eraYear;
        char era[16];
        uint8_t eraLength;
        // The month's code: 'M01' to 'M13', 'M05L' for a leap month (in
        // Hebrew, Adar I of a leap year, Adar II keeping 'M06');
        // NUL-padded, of monthCodeLength bytes.
        char monthCode[5];
        uint8_t monthCodeLength;
        // The month's place in its year, from 1.
        uint8_t month;
        uint8_t day;
        // 1 for Monday to 7 for Sunday.
        uint8_t dayOfWeek;
        uint8_t daysInMonth;
        uint8_t monthsInYear;
        // 1 in a leap year (one with a leap day or a leap month), else 0.
        uint8_t inLeapYear;
        uint16_t dayOfYear;
        uint16_t daysInYear;
    } mlocDateFields;

    // The fields a date is made from: a year (or an era and a year in it),
    // a month by code or by place, and a day.
    typedef struct mlocDateInput
    {
        // The era's code, of eraLength bytes; null for none, year then the
        // calendar's year counted on across eras.
        const char* era;
        size_t eraLength;
        int32_t year;
        // The month's code ('M05', 'M05L'), of monthCodeLength bytes, or
        // null; month its place in the year, or 0. When both are given they
        // must name one month.
        const char* monthCode;
        size_t monthCodeLength;
        uint32_t month;
        uint32_t day;
    } mlocDateInput;

    /// Makes a date from its fields in a calendar, as Temporal's
    /// PlainDate.from does: a month or day past the calendar's is clamped
    /// to the last (mloc_overflowConstrain) or refused
    /// (mloc_overflowReject), and so is a leap month the year lacks (made
    /// the month after it); a month code no year of the calendar has, an
    /// unknown era, or a month or day below 1 is refused either way.
    ///
    /// @param calendar  The calendar.
    /// @param input     The fields.
    /// @param overflow  What a month or day past the calendar's does.
    /// @param dateOut   Receives the date; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         value out of range or refused, or a date outside
    ///         Temporal's range; mloc_errorSyntax for an era or month code
    ///         not of the calendar.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeDate(mlocCalendar calendar,
                                                    const mlocDateInput* input,
                                                    mlocOverflow overflow, mlocDate* dateOut);

    /// Makes the date of a day in a calendar.
    ///
    /// @param calendar  The calendar.
    /// @param day       The day, 1 for January 1 of year 1 in the proleptic
    ///                  Gregorian calendar.
    /// @param dateOut   Receives the date; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         calendar out of range, or a day outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day,
                                                           mlocDate* dateOut);

    /// Computes a date's fields in its calendar.
    ///
    /// @param date       The date.
    /// @param fieldsOut  Receives the fields; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or a
    ///         date the library did not make (a day out of range or an
    ///         unknown calendar).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetDateFields(const mlocDate* date,
                                                         mlocDateFields* fieldsOut);

    // A date duration, Temporal's date part of a duration: years, months,
    // weeks and days, none of another sign than the rest, years, months
    // and weeks below 2^32 in size and days at most 104249991374 (2^53
    // seconds).
    typedef struct mlocDateDuration
    {
        int64_t years;
        int64_t months;
        int64_t weeks;
        int64_t days;
    } mlocDateDuration;

    /// Adds a duration to a date in its calendar, as Temporal's
    /// PlainDate.add does: the years first, keeping the month code (a leap
    /// month the new year lacks is clamped as mlocMakeDate clamps it, or
    /// refused), then the months, then the day clamped to its month or
    /// refused, then weeks and days as days. Subtracting is adding the
    /// negated duration.
    ///
    /// @param date      The date.
    /// @param duration  The duration.
    /// @param overflow  What a month code or day the result lacks does.
    /// @param dateOut   Receives the date, in the date's calendar; left
    ///                  unchanged on failure. May be date itself.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a date
    ///         the library did not make, a duration out of its limits or of
    ///         two signs, a month code or day refused, or a result outside
    ///         Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocAddToDate(const mlocDate* date,
                                                     const mlocDateDuration* duration,
                                                     mlocOverflow overflow, mlocDate* dateOut);

    // The largest unit a difference of dates is given in.
    typedef int32_t mlocDateUnit;
    enum
    {
        mloc_dateUnitYear = 0,
        mloc_dateUnitMonth = 1,
        mloc_dateUnitWeek = 2,
        mloc_dateUnitDay = 3,
    };

    /// Computes the duration from one date to another of the same
    /// calendar, as Temporal's PlainDate.until does: the most years whose
    /// addition does not pass the second date (judged by year, month code
    /// and day), then the most months, then weeks if they are the largest
    /// unit, then days; none larger than the largest unit. Temporal's
    /// since of the first date and the second is this duration negated
    /// (not the duration from the second to the first, which may differ
    /// by the clamping of days).
    ///
    /// @param one          The date from.
    /// @param two          The date to.
    /// @param largestUnit  The largest unit to give.
    /// @param durationOut  Receives the duration, of one sign; left
    ///                     unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a date
    ///         the library did not make, dates of two calendars, or an
    ///         unknown unit.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateUntil(const mlocDate* one, const mlocDate* two,
                                                     mlocDateUnit largestUnit,
                                                     mlocDateDuration* durationOut);

    // How a difference of dates is rounded (Temporal's difference
    // settings): to a multiple of increment (1 to 10^9) of smallestUnit,
    // which is no larger than largestUnit, by mode. Temporal's default
    // mode for a difference is mloc_roundTrunc.
    typedef struct mlocDateRounding
    {
        mlocDateUnit largestUnit;
        mlocDateUnit smallestUnit;
        uint32_t increment;
        mlocRoundingMode mode;
    } mlocDateRounding;

    /// Computes the duration from one date to another of the same calendar
    /// and rounds it, as Temporal's PlainDate.until does with rounding
    /// options: the difference of mlocDateUntil, then for a smallest unit
    /// of years, months or weeks the multiples of the increment around it,
    /// added to the first date, with the second date's place between them
    /// deciding by mode; for days, the days rounded to the increment. A
    /// rounding that reaches a larger unit's next value carries into it,
    /// up to the largest unit. Temporal's since is this from the same
    /// first date with the mode negated (ceil for floor, half-ceil for
    /// half-floor, and back), and the result negated.
    ///
    /// @param one          The date from.
    /// @param two          The date to.
    /// @param rounding     The units, increment and mode.
    /// @param durationOut  Receives the duration; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a date
    ///         the library did not make, dates of two calendars, an unknown
    ///         unit or mode, a smallest unit larger than the largest, an
    ///         increment out of range, or a multiple whose date falls
    ///         outside Temporal's range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocDateUntilRounded(const mlocDate* one,
                                                            const mlocDate* two,
                                                            const mlocDateRounding* rounding,
                                                            mlocDateDuration* durationOut);

    // Which fields an mlocDateChanges gives.
    enum
    {
        mloc_givenEra = 1,
        mloc_givenEraYear = 2,
        mloc_givenYear = 4,
        mloc_givenMonth = 8,
        mloc_givenMonthCode = 16,
        mloc_givenDay = 32,
    };

    // Fields given by name, as a Temporal property bag gives them: given
    // is a set of mloc_given flags, and only the flagged fields are read.
    // An era comes with its era year; a year given with them must name
    // the same year.
    typedef struct mlocDateChanges
    {
        uint32_t given;
        // The era's code, of eraLength bytes.
        const char* era;
        size_t eraLength;
        int32_t eraYear;
        // The calendar's year counted on across eras.
        int32_t year;
        // The month's code, of monthCodeLength bytes; month its place.
        const char* monthCode;
        size_t monthCodeLength;
        uint32_t month;
        uint32_t day;
    } mlocDateChanges;

    /// Makes a date with some of a date's fields changed, as Temporal's
    /// PlainDate.with does: the date's year, month code and day, less the
    /// month code where a month or month code is given and less the year
    /// where a year, era or era year is given, with the given fields over
    /// them, made as mlocMakeDate makes a date.
    ///
    /// @param date      The date.
    /// @param changes   The fields to change.
    /// @param overflow  What a month or day past the calendar's does.
    /// @param dateOut   Receives the date; left unchanged on failure. May be date itself.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a date
    ///         the library did not make, an era without its era year or
    ///         the reverse, a year, month or day refused or disagreeing,
    ///         or a date outside Temporal's range; mloc_errorSyntax for an
    ///         era or month code not of the calendar.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocWithDate(const mlocDate* date,
                                                    const mlocDateChanges* changes,
                                                    mlocOverflow overflow, mlocDate* dateOut);

    /// Finds a month and day's reference date, as Temporal's
    /// PlainMonthDay.from does in the Intl Era and Month Code proposal: a
    /// month code (or, with a year, a month or month code of that year)
    /// and a day, clamped to the month's longest length (30 in Chinese and
    /// Dangi, the given year's month with a year) or refused; then the
    /// latest date from 1900 to 1972 in the ISO calendar with that month
    /// code and day, else the earliest from 1973 to 2035. Chinese and
    /// Dangi take the proposal's table of reference years, a month and
    /// day it lacks becoming the month of the same number (or refused).
    ///
    /// @param calendar      The calendar.
    /// @param fields        A day, and a month code, or a year (or era and
    ///                      era year) and a month or month code.
    /// @param overflow      What a month or day past the calendar's does.
    /// @param referenceOut  Receives the reference date, in the calendar;
    ///                      left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, missing
    ///         or disagreeing fields (a month's place without a year among
    ///         them), or a value refused; mloc_errorSyntax
    ///         for an era or month code not of the calendar.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeMonthDay(mlocCalendar calendar,
                                                        const mlocDateChanges* fields,
                                                        mlocOverflow overflow,
                                                        mlocDate* referenceOut);

#ifdef __cplusplus
}
#endif

#endif
