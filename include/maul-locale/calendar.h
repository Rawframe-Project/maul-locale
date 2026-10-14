// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Calendars (UTS #35 Part 4, record mloc-0040): dates as values in
// CLDR's calendars, as Temporal models them. A date is a day, counted
// from January 1 of year 1 in the proleptic Gregorian calendar (day 1), read in a
// calendar; its fields (year, era, month code, day) are computed from
// it. Calendars without a built implementation yet refuse with
// mloc_errorNotFound.

#ifndef MAUL_LOCALE_CALENDAR_H
#define MAUL_LOCALE_CALENDAR_H

#include "maul-locale/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // CLDR's calendars, by their BCP 47 ids.
    typedef enum mlocCalendar
    {
        // "gregory": the proleptic Gregorian calendar, eras ce and bce.
        mloc_calendarGregorian = 0,
        // "iso8601": the Gregorian calendar with no era.
        mloc_calendarIso8601 = 1,
        // "buddhist": Gregorian, years of the Buddhist era (be).
        mloc_calendarBuddhist = 2,
        // "roc": Gregorian, years of the Republic of China (roc, broc).
        mloc_calendarRoc = 3,
        // "japanese": Gregorian, years of the Japanese eras since Meiji.
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
        // "islamic": read as Umm al-Qura (record mloc-0040).
        mloc_calendarIslamic = 14,
        // "islamic-rgsa": read as Umm al-Qura (record mloc-0040).
        mloc_calendarIslamicRgsa = 15,
        // "chinese": lunisolar, with leap months, no era.
        mloc_calendarChinese = 16,
        // "dangi": the Korean lunisolar calendar, no era.
        mloc_calendarDangi = 17,
    } mlocCalendar;

    // What a field out of range does: clamped to the nearest valid value,
    // or refused (Temporal's overflow option).
    typedef enum mlocOverflow
    {
        mloc_overflowConstrain = 0,
        mloc_overflowReject = 1,
    } mlocOverflow;

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
        // An mlocCalendar.
        uint8_t calendar;
        // Zero.
        uint8_t reserved[3];
    } mlocDate;

    // The fields of a date in its calendar.
    typedef struct mlocDateFields
    {
        // The calendar's year counted on across eras (Temporal's year):
        // 0 is 1 BCE in the Gregorian calendar.
        int32_t year;
        // The year in its era, and the era's code (Temporal's: "ce", "bce",
        // "be", "roc", "broc", "am", "aa", "shaka", "ap", "ah", "bh"),
        // NUL-padded; eraLength 0 and eraYear the year in a calendar with no
        // eras.
        int32_t eraYear;
        char era[16];
        uint8_t eraLength;
        // The month's code: 'M01' to 'M13', 'M05L' for a leap month;
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
    /// (mloc_overflowReject); a month code no year of the calendar has, an
    /// unknown era, or a month or day below 1 is refused either way.
    ///
    /// @param calendar  The calendar.
    /// @param input     The fields.
    /// @param overflow  What a month or day past the calendar's does.
    /// @param dateOut   Receives the date; left unchanged on failure.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         value out of range or refused, or a date outside
    ///         Temporal's range; mloc_errorSyntax for an era or month code
    ///         not of the calendar; mloc_errorNotFound for a calendar not
    ///         built yet.
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
    ///         calendar out of range, or a day outside Temporal's range;
    ///         mloc_errorNotFound for a calendar not built yet.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeDateFromDay(mlocCalendar calendar, int32_t day,
                                                           mlocDate* dateOut);

    /// Computes a date's fields in its calendar.
    ///
    /// @param date       The date.
    /// @param fieldsOut  Receives the fields.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or a
    ///         date the library did not make (a day out of range, an unknown
    ///         calendar, reserved bytes not zero); mloc_errorNotFound for a
    ///         calendar not built yet.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetDateFields(const mlocDate* date,
                                                         mlocDateFields* fieldsOut);

#ifdef __cplusplus
}
#endif

#endif
