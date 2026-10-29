// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Number formatting (UTS #35 Part 3): a decimal in a locale's digits,
// symbols and grouping, rounded as the options say. The options follow
// ECMA-402's NumberFormat; their zero value is the default, and the zero
// precision rounds nothing. A formatter also reads a number a person
// types.

#ifndef MAUL_LOCALE_NUMBER_H
#define MAUL_LOCALE_NUMBER_H

#include "maul-locale/base.h"
#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // How a number is written: in full, with an exponent of ten (any, or
    // a multiple of three), or compact ("1.2K", "1.2 thousand"). The
    // precision rounds the mantissa of scientific and engineering
    // notation, and compact notation's scaled value, which the zero
    // precision rounds to an integer when two integer digits or more show
    // and to two significant digits otherwise. Compact notation groups
    // with two digits before the first separator unless the grouping
    // option says otherwise, and takes the decimal style only.
    typedef enum mlocNotation
    {
        mloc_notationStandard = 0,
        mloc_notationScientific = 1,
        mloc_notationEngineering = 2,
        mloc_notationCompactShort = 3,
        mloc_notationCompactLong = 4,
    } mlocNotation;

    // What a number stands for: itself, or a ratio shown as a percent or
    // per mille (the value times 100 or 1000, before rounding), in the
    // locale's percent pattern.
    typedef enum mlocNumberStyle
    {
        mloc_numberDecimal = 0,
        mloc_numberPercent = 1,
        mloc_numberPerMille = 2,
    } mlocNumberStyle;

    // When the integer digits are grouped: as the locale prefers
    // (Spanish writes 1234 but 12 345), always, only with at least two
    // digits before the first separator, or never.
    typedef enum mlocGrouping
    {
        mloc_groupingAuto = 0,
        mloc_groupingAlways = 1,
        mloc_groupingMin2 = 2,
        mloc_groupingOff = 3,
    } mlocGrouping;

    // Which numbers show a sign (ECMA-402's signDisplay): negative ones,
    // negative zero included; every one but NaN; none; every one but
    // zero; or negative ones but zero.
    typedef enum mlocSignDisplay
    {
        mloc_signAuto = 0,
        mloc_signAlways = 1,
        mloc_signNever = 2,
        mloc_signExceptZero = 3,
        mloc_signNegative = 4,
    } mlocSignDisplay;

    // Which digit limits round the number (ECMA-402's roundingType): none
    // (the decimal shows its own digits), fraction digits, significant
    // digits, or both, the result with more or with less precision
    // winning.
    typedef enum mlocPrecisionKind
    {
        mloc_precisionNone = 0,
        mloc_precisionFraction = 1,
        mloc_precisionSignificant = 2,
        mloc_precisionMorePrecision = 3,
        mloc_precisionLessPrecision = 4,
    } mlocPrecisionKind;

    // Whether a rounded integer keeps the fraction zeros the minimum
    // fraction digits ask for (ECMA-402's trailingZeroDisplay).
    typedef enum mlocTrailingZeros
    {
        mloc_trailingZerosAuto = 0,
        mloc_trailingZerosStripIfInteger = 1,
    } mlocTrailingZeros;

    // How a number is rounded. Fields a kind does not read must be 0.
    typedef struct mlocPrecision
    {
        mlocPrecisionKind kind;
        mlocRoundingMode mode;
        mlocTrailingZeros trailingZeros;
        // Fraction digits, 0 to 100, minimum at most maximum: read by the
        // fraction, more and less precision kinds.
        uint8_t minFractionDigits;
        uint8_t maxFractionDigits;
        // Significant digits, 1 to 21, minimum at most maximum: read by the
        // significant, more and less precision kinds.
        uint8_t minSignificantDigits;
        uint8_t maxSignificantDigits;
        // A rounding increment in units of the last fraction digit: 0 or 1
        // for none, else 2, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000,
        // 2000, 2500 or 5000, with the fraction kind and equal fraction
        // limits.
        uint16_t increment;
    } mlocPrecision;

    // A number formatter's options; the zero value is the default.
    typedef struct mlocNumberOptions
    {
        mlocNotation notation;
        mlocNumberStyle style;
        mlocGrouping grouping;
        mlocSignDisplay signDisplay;
        mlocPrecision precision;
        // At least this many integer digits, zeros before; 0 to 21, 0
        // meaning 1.
        uint8_t minIntegerDigits;
    } mlocNumberOptions;

    // A locale's number formatter: a value that points into the data it
    // was made from, which must outlive it. Opaque; make it with
    // mlocInitNumberFormatter.
    typedef struct mlocNumberFormatter
    {
        uint64_t opaque[32];
    } mlocNumberFormatter;

    // The parts of a formatted number the output's part function
    // receives.
    typedef enum mlocNumberPart
    {
        mloc_numberPartInteger = 0,
        mloc_numberPartGroup = 1,
        mloc_numberPartDecimal = 2,
        mloc_numberPartFraction = 3,
        mloc_numberPartMinusSign = 4,
        mloc_numberPartPlusSign = 5,
        mloc_numberPartPercentSign = 6,
        mloc_numberPartPerMilleSign = 7,
        mloc_numberPartExponentSeparator = 8,
        mloc_numberPartExponentMinusSign = 9,
        mloc_numberPartExponentInteger = 10,
        mloc_numberPartInfinity = 11,
        mloc_numberPartNaN = 12,
        mloc_numberPartLiteral = 13,
        // A compact pattern's words or symbol ("K", "thousand"), the
        // spaces around it being literal.
        mloc_numberPartCompact = 14,
        // The approximately sign of a range whose ends write alike ("~5").
        mloc_numberPartApproximatelySign = 15,
        // A currency's symbol, code or name.
        mloc_numberPartCurrency = 16,
        // A unit's name ("m", "meters"), the spaces around it being
        // literal.
        mloc_numberPartUnit = 17,
    } mlocNumberPart;

    // In a range's parts, the end a part belongs to, added to its
    // mlocNumberPart: the start, the end, or neither for what both share
    // (the separator, the approximately sign's number, a collapsed
    // affix).
    enum
    {
        mloc_numberRangeStart = 0x100,
        mloc_numberRangeEnd = 0x200,
    };

    /// Makes a locale's number formatter. The numbering system is the
    /// locale's -u-nu keyword ("native", "traditio" and "finance" as the
    /// locale names them), else the locale's default; one without digits
    /// (Roman, for one) takes the default, which the spellout part
    /// formats instead.
    ///
    /// @param data          The data with the number units, the parents and
    ///                      the likely subtags.
    /// @param locale        The locale, canonicalized.
    /// @param options       The options, or null for the defaults.
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_fellBack when the numbering system
    ///         asked for has no digits and the default stands instead (the
    ///         locale's data is always found along its chain, root's last,
    ///         so whether a locale is supported is a question for language
    ///         matching); mloc_errorNotFound for data without numbers;
    ///         mloc_errorInvalid for a null pointer or options out of
    ///         range or in conflict.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitNumberFormatter(const mlocDataSet* data,
                                                               const mlocLocale* locale,
                                                               const mlocNumberOptions* options,
                                                               mlocNumberFormatter* formatterOut);

    /// Formats a decimal.
    ///
    /// @param formatter  A formatter from mlocInitNumberFormatter.
    /// @param decimal    The number.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed); mloc_errorLimit
    ///         when rounding passes MLOC_MAX_DECIMAL_EXPONENT; mloc_errorInvalid for a null
    ///         pointer, or a formatter or decimal the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocNumberFormatter_Format(
        const mlocNumberFormatter* formatter, const mlocDecimal* decimal, mlocOutput* output);

    /// Formats a range of two decimals (ECMA-402's formatRange): both ends
    /// by the formatter, around the locale's range separator, spaced when
    /// the ends have affixes of their own ("3–5", "-5 – -3", "3K – 5K"),
    /// an affix both share written once when it is longer than one
    /// character ("3–5 thousand"); ends that write alike give one number
    /// with the approximately sign ("~5"). The start may pass the end.
    ///
    /// @param formatter  A formatter from mlocInitNumberFormatter.
    /// @param start      The range's start.
    /// @param end        Its end.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values with the end each
    ///                   belongs to (mloc_numberRangeStart,
    ///                   mloc_numberRangeEnd, or 0 for both).
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed); mloc_errorLimit
    ///         when rounding passes MLOC_MAX_DECIMAL_EXPONENT; mloc_errorInvalid for a null
    ///         pointer, a NaN end, or a formatter or decimal the library
    ///         did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocNumberFormatter_FormatRange(const mlocNumberFormatter* formatter, const mlocDecimal* start,
                                    const mlocDecimal* end, mlocOutput* output);

    // How strictly a number is read (ICU's lenient and strict parse
    // modes). Lenient: ignorables anywhere, separators by their
    // equivalence classes and grouping sizes unchecked, signs, percent
    // and per mille signs read where the pattern has none, affixes alone.
    // Strict: the grouping sizes, the strict separator classes, the
    // pattern's full affixes, bidi controls the only ignorables.
    typedef enum mlocParseMode
    {
        mloc_parseLenient = 0,
        mloc_parseStrict = 1,
    } mlocParseMode;

    /// Reads a number as a person types it in the formatter's locale
    /// (ICU's DecimalFormat::parse): its digits, any Unicode decimal
    /// digits, its separators, signs, pattern affixes, percent or per
    /// mille sign (the value then divided by 100 or 1000, as the style
    /// multiplies it), NaN, infinity, and an exponent. Reading starts at
    /// the first byte and stops where no matcher reads more: "12 apples"
    /// gives 12, ending at 2; compare the end with the length to require
    /// the whole text. The decimal is exact, its fraction digits those
    /// typed (English "1.50" gives 1.50).
    ///
    /// @param formatter   A formatter from mlocInitNumberFormatter, of
    ///                    standard, scientific or engineering notation.
    /// @param text        UTF-8 text; it need not end in a 0 byte.
    /// @param length      Its length in bytes.
    /// @param mode        An mlocParseMode.
    /// @param decimalOut  Receives the number read.
    /// @param endOut      Receives the offset after the number read, or on
    ///                    mloc_errorSyntax the offset reading stopped at.
    /// @return mloc_success; mloc_errorSyntax when no number is read (or,
    ///         in strict mode, the affixes or grouping do not match);
    ///         mloc_errorLimit when the digits or the exponent pass
    ///         MLOC_MAX_DECIMAL_DIGITS or MLOC_MAX_DECIMAL_EXPONENT; mloc_errorNotFound for data
    ///         without the parse sets; mloc_errorInvalid for a null pointer, a mode out of range, a
    ///         formatter the library did not make or one of compact notation.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocNumberFormatter_Parse(const mlocNumberFormatter* formatter, const char* text, size_t length,
                              mlocParseMode mode, mlocDecimal* decimalOut, size_t* endOut);

#ifdef __cplusplus
}
#endif

#endif
