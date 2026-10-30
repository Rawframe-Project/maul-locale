// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Relative time: a signed amount of a unit written as a locale says it
// ("in 3 days", "2 hr. ago", "yesterday", "next Monday"), as ECMA-402's
// Intl.RelativeTimeFormat and ICU4C's RelativeDateTimeFormatter write it.
// The amount is the caller's: the library never reads a clock. The names
// and patterns are a data file of their own, src/generated/relative.mloc.

#ifndef MAUL_LOCALE_RELATIVE_H
#define MAUL_LOCALE_RELATIVE_H

#include "maul-locale/datetime.h"
#include "maul-locale/number.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // The units: ECMA-402's eight, then the weekdays, ICU4C's ("next
    // Monday", "in 3 Mondays").
    typedef int32_t mlocRelativeUnit;
    enum
    {
        mloc_relativeYear = 0,
        mloc_relativeQuarter = 1,
        mloc_relativeMonth = 2,
        mloc_relativeWeek = 3,
        mloc_relativeDay = 4,
        mloc_relativeHour = 5,
        mloc_relativeMinute = 6,
        mloc_relativeSecond = 7,
        mloc_relativeSunday = 8,
        mloc_relativeMonday = 9,
        mloc_relativeTuesday = 10,
        mloc_relativeWednesday = 11,
        mloc_relativeThursday = 12,
        mloc_relativeFriday = 13,
        mloc_relativeSaturday = 14,
    };

    // How much room the text takes, as ECMA-402's style: long ("in 3
    // hours"), short ("in 3 hr.") or narrow ("in 3h"); a width the locale
    // lacks takes the next wider one's.
    typedef int32_t mlocRelativeWidth;
    enum
    {
        mloc_relativeWidthLong = 0,
        mloc_relativeWidthShort = 1,
        mloc_relativeWidthNarrow = 2,
    };

    // Whether a near amount takes its name, as ECMA-402's numeric: always
    // the number ("in 1 day"), or auto, the locale's name of an amount
    // that equals -2, -1, 0, 1 or 2 where it has one ("tomorrow"; -0 is 0;
    // "1.0" is 1), the number otherwise.
    typedef int32_t mlocRelativeNumeric;
    enum
    {
        mloc_relativeNumericAlways = 0,
        mloc_relativeNumericAuto = 1,
    };

    // A relative time formatter's options; the zero value is the default.
    typedef struct mlocRelativeOptions
    {
        // The notation, grouping, minimum integer digits and precision of
        // the amount, as for numbers; the style must be decimal and the
        // sign display auto, the sign being the direction.
        mlocNumberOptions number;
        mlocRelativeWidth width;
        mlocRelativeNumeric numeric;
        // The first letter titlecased: at a sentence's start always, in a
        // list or menu or standing alone where the locale's relative
        // context transforms say so (CLDR's), as the names part does.
        mlocCapitalization capitalization;
    } mlocRelativeOptions;

    // A locale's relative time formatter: a value that points into the
    // data it was made from, which must outlive it. Opaque; make it with
    // mlocInitRelativeFormatter.
    typedef struct mlocRelativeFormatter
    {
        uint64_t opaque[56];
    } mlocRelativeFormatter;

    /// Makes a locale's relative time formatter.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units, the plural
    ///                      rules and relative time (relative.mloc).
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the number units or
    ///         relative time; mloc_errorFormat for a chain of more relative
    ///         units than the formatter holds; mloc_errorInvalid for a
    ///         null pointer or options out of range or in conflict (a
    ///         style other than decimal, a sign display other than auto).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitRelativeFormatter(
        const mlocDataSet* data, const mlocLocale* locale, const mlocRelativeOptions* options,
        mlocRelativeFormatter* formatterOut);

    /// Writes an amount of a unit relative to now: negative amounts (-0
    /// included) in the past, the others in the future; the amount's
    /// magnitude by the formatter's number options in the pattern of its
    /// plural category as written ("in 1 day", "in 1.5 days"), a category
    /// the locale lacks taking the other category's, a pattern without
    /// the number writing none (Arabic's "in one year"); under auto, an
    /// amount equal to a named one writes the name ("yesterday", "now").
    ///
    /// @param formatter  A formatter from mlocInitRelativeFormatter.
    /// @param amount     The amount, finite.
    /// @param unit       The unit.
    /// @param textOut    Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values: the number's, and
    ///                   literal for the pattern's text and for a name. Left
    ///                   as it was on any failure but mloc_errorCapacity.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (textOut->length is then the length needed); mloc_errorLimit
    ///         when rounding passes MLOC_MAX_DECIMAL_EXPONENT; mloc_errorNotFound for a unit
    ///         whose patterns the locale's chain lacks; mloc_errorInvalid
    ///         for a null pointer, an amount not finite, a unit out of
    ///         range, or a formatter or decimal the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocRelativeFormatter_Format(const mlocRelativeFormatter* formatter, const mlocDecimal* amount,
                                 mlocRelativeUnit unit, mlocOutput* textOut);

    /// Writes a day relative to today and a time of day joined by the
    /// locale's relative date-time glue ("tomorrow at 3:00 PM", "in 3 days,
    /// 10:00"): the day as mlocRelativeFormatter_Format writes an amount of days, the
    /// time as the time formatter writes it, joined by the glue of the
    /// time formatter's calendar at the length the relative width gives
    /// (long the long glue, short the medium, narrow the short; ICU4C takes
    /// the medium atTime glue whatever the width). The day is capitalized
    /// as the relative formatter asks only where the glue writes it first.
    ///
    /// @param relative   A formatter from mlocInitRelativeFormatter.
    /// @param time       A formatter from mlocInitTimeFormatter, without a
    ///                   zone; its data holds the glue.
    /// @param days       The day's distance from today, finite.
    /// @param timeOfDay  The time.
    /// @param textOut    Receives the text; its part function, when set,
    ///                   receives mlocDatePart values: the day whole as
    ///                   mloc_datePartRelativeDay, the time's fields, and
    ///                   literal for the glue's text. Left as it was on any
    ///                   failure but mloc_errorCapacity.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (textOut->length is then the length needed); mloc_errorLimit
    ///         when rounding passes MLOC_MAX_DECIMAL_EXPONENT; mloc_errorNotFound for a day
    ///         whose patterns the locale's chain lacks; mloc_errorInvalid
    ///         for a null pointer, an amount not finite, a time out of
    ///         range, or a formatter the library did not make (a time
    ///         formatter with a zone among them).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRelativeFormatter_FormatDayTime(
        const mlocRelativeFormatter* relative, const mlocTimeFormatter* time,
        const mlocDecimal* days, const mlocTime* timeOfDay, mlocOutput* textOut);

#ifdef __cplusplus
}
#endif

#endif
