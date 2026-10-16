// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Durations (record mloc-0044): a Temporal duration's fields written as
// a locale says them ("1 hr, 2 min, 3 sec", "1:02:03", "2 days and 3
// hours"), as ECMA-402's Intl.DurationFormat writes them. The fields are
// written as given, never balanced ("90 min"); the zone part's
// arithmetic balances and rounds them (mlocGetDurationValues). The unit
// names, lists and duration patterns are those of the unit and list
// parts.

#ifndef MAUL_LOCALE_DURATION_H
#define MAUL_LOCALE_DURATION_H

#include "maul-locale/number.h"
#include "maul-locale/zone.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // The fields, largest first, as mlocDurationValues holds them.
    typedef enum mlocDurationField
    {
        mloc_durationYears = 0,
        mloc_durationMonths = 1,
        mloc_durationWeeks = 2,
        mloc_durationDays = 3,
        mloc_durationHours = 4,
        mloc_durationMinutes = 5,
        mloc_durationSeconds = 6,
        mloc_durationMilliseconds = 7,
        mloc_durationMicroseconds = 8,
        mloc_durationNanoseconds = 9,
        mloc_durationFieldCount = 10,
    } mlocDurationField;

    // The style of the whole, ECMA-402's style: short ("1 hr, 2 min"),
    // long ("1 hour, 2 minutes"), narrow ("1h 2m") or digital ("1:02:00").
    typedef enum mlocDurationStyle
    {
        mloc_durationStyleShort = 0,
        mloc_durationStyleLong = 1,
        mloc_durationStyleNarrow = 2,
        mloc_durationStyleDigital = 3,
    } mlocDurationStyle;

    // A field's style. Default takes ECMA-402's: under digital, short for
    // years to days and numeric for the rest; after a numeric field,
    // numeric; otherwise the whole's style. Years to days take long,
    // short or narrow; hours to seconds also numeric and two-digit;
    // milliseconds to nanoseconds also numeric, which writes them as the
    // fraction of the next larger field ("1.5 sec", "1:02:03.456").
    // Minutes and seconds after a numeric field are written with two
    // digits, and hours too where the locale pads them.
    typedef enum mlocDurationFieldStyle
    {
        mloc_durationFieldDefault = 0,
        mloc_durationFieldLong = 1,
        mloc_durationFieldShort = 2,
        mloc_durationFieldNarrow = 3,
        mloc_durationFieldNumeric = 4,
        mloc_durationFieldTwoDigit = 5,
    } mlocDurationFieldStyle;

    // Whether a field of 0 is written. Default takes ECMA-402's: always
    // for hours to seconds under digital and for minutes and seconds
    // after a numeric field, auto otherwise.
    typedef enum mlocDurationDisplay
    {
        mloc_durationDisplayDefault = 0,
        mloc_durationDisplayAuto = 1,
        mloc_durationDisplayAlways = 2,
    } mlocDurationDisplay;

    // A duration formatter's options; the zero value is ECMA-402's
    // defaults. The numbering system is the locale's (its -u-nu).
    typedef struct mlocDurationOptions
    {
        mlocDurationStyle style;
        // By mlocDurationField.
        mlocDurationFieldStyle fields[mloc_durationFieldCount];
        mlocDurationDisplay displays[mloc_durationFieldCount];
        // The fraction digits of the field that takes the numeric ones'
        // fraction: fractionalDigits (0 to 9) when fixed, else at most 9
        // as needed. The fraction is truncated, never rounded.
        bool fixedFractionalDigits;
        uint8_t fractionalDigits;
    } mlocDurationOptions;

    enum
    {
        // A duration part's kind is an mlocNumberPart plus this times the
        // field it belongs to plus one (mlocDurationField + 1), the field
        // 0 for the list's text and the separators of numeric fields:
        // "1" of hours is mloc_numberPartInteger + 5 * this.
        mloc_durationPartField = 0x100,
    };

    // A locale's duration formatter: a value that points into the data
    // it was made from, which must outlive it. Opaque; make it with
    // mlocInitDurationFormatter.
    typedef struct mlocDurationFormatter
    {
        uint64_t opaque[512];
    } mlocDurationFormatter;

    /// Makes a locale's duration formatter.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units, the plural
    ///                      rules, the unit names and the lists.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without those parts;
    ///         mloc_errorData for a locale whose duration pattern is not
    ///         hours, minutes and seconds between two separators;
    ///         mloc_errorCapacity when the names do not fit the formatter;
    ///         mloc_errorInvalid for a null pointer or options out of
    ///         range or in conflict, as ECMA-402 refuses them (a field's
    ///         style it does not take, display always on a fraction, a
    ///         field not numeric after a numeric one, a field not a
    ///         fraction after one).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocInitDurationFormatter(mlocDurationFormatter* formatterOut, const mlocDataSet* data,
                              const mlocLocale* locale, const mlocDurationOptions* options);

    /// Gives a formatter's options as resolved: every field's style and
    /// display (numeric for a fraction), never the defaults.
    ///
    /// @param formatter   A formatter from mlocInitDurationFormatter.
    /// @param optionsOut  Receives the options.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or a
    ///         formatter the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocGetDurationOptions(const mlocDurationFormatter* formatter, mlocDurationOptions* optionsOut);

    /// Writes a duration as ECMA-402's PartitionDurationFormatPattern
    /// does: the fields in order up to the first numeric one, each shown
    /// by its display as a number with the unit's name in its plural
    /// category, the numeric fields as one item joined by the locale's
    /// separators, the items joined by the locale's unit list of the
    /// style (short for digital). The first field shown carries the sign
    /// (-0 for a field of 0 in a negative duration). Each field is its
    /// exact value, and the fractions' sum is exact.
    ///
    /// @param formatter  A formatter from mlocInitDurationFormatter.
    /// @param duration   The fields: integers of one sign, valid as
    ///                   mlocMakeDuration takes them.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives the parts (mloc_durationPartField).
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed);
    ///         mloc_errorInvalid for a null pointer, a duration
    ///         mlocMakeDuration refuses, or a formatter the library did not
    ///         make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDuration(const mlocDurationFormatter* formatter,
                                                          const mlocDurationValues* duration,
                                                          mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
