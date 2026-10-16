// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Relative time (record mloc-0043): a signed amount of a unit written as
// a locale says it ("in 3 days", "2 hr. ago", "yesterday", "next
// Monday"), as ECMA-402's Intl.RelativeTimeFormat and ICU4C's
// RelativeDateTimeFormatter write it. The amount is the caller's: the
// library never reads a clock. The names and patterns are a data file
// of their own, src/generated/relative.mloc.

#ifndef MAUL_LOCALE_RELATIVE_H
#define MAUL_LOCALE_RELATIVE_H

#include "maul-locale/number.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // The units: ECMA-402's eight, then the weekdays, ICU4C's ("next
    // Monday", "in 3 Mondays").
    typedef enum mlocRelativeUnit
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
    } mlocRelativeUnit;

    // How much room the text takes, as ECMA-402's style: long ("in 3
    // hours"), short ("in 3 hr.") or narrow ("in 3h"); a width the locale
    // lacks takes the next wider one's.
    typedef enum mlocRelativeWidth
    {
        mloc_relativeWidthLong = 0,
        mloc_relativeWidthShort = 1,
        mloc_relativeWidthNarrow = 2,
    } mlocRelativeWidth;

    // Whether a near amount takes its name, as ECMA-402's numeric: always
    // the number ("in 1 day"), or auto, the locale's name of an amount
    // that equals -2, -1, 0, 1 or 2 where it has one ("tomorrow"; -0 is 0;
    // "1.0" is 1), the number otherwise.
    typedef enum mlocRelativeNumeric
    {
        mloc_relativeNumericAlways = 0,
        mloc_relativeNumericAuto = 1,
    } mlocRelativeNumeric;

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
    ///         relative time; mloc_errorData for a chain of more relative
    ///         units than the formatter holds; mloc_errorInvalid for a
    ///         null pointer or options out of range or in conflict (a
    ///         style other than decimal, a sign display other than auto).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocInitRelativeFormatter(mlocRelativeFormatter* formatterOut, const mlocDataSet* data,
                              const mlocLocale* locale, const mlocRelativeOptions* options);

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
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values: the number's, and
    ///                   literal for the pattern's text and for a name.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed) or rounding
    ///         passes the decimal's limits; mloc_errorNotFound for a unit
    ///         whose patterns the locale's chain lacks; mloc_errorInvalid
    ///         for a null pointer, an amount not finite, a unit out of
    ///         range, or a formatter or decimal the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatRelative(const mlocRelativeFormatter* formatter,
                                                          const mlocDecimal* amount,
                                                          mlocRelativeUnit unit,
                                                          mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
