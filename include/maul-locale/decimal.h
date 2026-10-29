// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decimal values and their plural operands: a number is formatted from its
// decimal digits, never from a binary value, so the digits a user typed
// are the digits shown.

#ifndef MAUL_LOCALE_DECIMAL_H
#define MAUL_LOCALE_DECIMAL_H

#include "maul-locale/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

// The most significant digits a decimal holds. A value that needs more is
// refused with mloc_errorLimit.
#define MLOC_MAX_DECIMAL_DIGITS 40

// The largest power of ten a decimal's digits reach, either way, and the
// most fraction digits it shows. A value past it is refused with
// mloc_errorLimit.
#define MLOC_MAX_DECIMAL_EXPONENT 999999

    // What a decimal is.
    typedef enum mlocDecimalKind
    {
        mloc_decimalFinite = 0,
        mloc_decimalInfinity = 1,
        mloc_decimalNaN = 2,
    } mlocDecimalKind;

    // A decimal number with the fraction digits it shows. Make one with
    // mlocMakeDecimal, mlocMakeUnsignedDecimal or mlocParseDecimal.
    typedef struct mlocDecimal
    {
        // The significant digits, 0 to 9, most significant first: the
        // first and last are not 0. Unused bytes are 0.
        uint8_t digits[MLOC_MAX_DECIMAL_DIGITS];
        // The power of ten of the last digit; 0 for zero.
        int32_t exponent;
        // The fraction digits shown, at least those the digits need:
        // 2 for "1.50", whose digits are 15 with exponent -1.
        int32_t fractionDigits;
        // The digits held, 0 for zero.
        uint8_t count;
        // 1 for a negative value, "-0" included; 0 for NaN.
        uint8_t negative;
        // An mlocDecimalKind; a decimal that is not finite has no digits.
        uint8_t kind;
    } mlocDecimal;

    // How a value between two multiples rounds (ECMA-402's names): toward
    // positive infinity, negative infinity, away from zero or toward
    // zero; or to the nearer, a tie going as named. The zero value is
    // ECMA-402's default.
    typedef enum mlocRoundingMode
    {
        mloc_roundHalfExpand = 0,
        mloc_roundHalfEven = 1,
        mloc_roundHalfTrunc = 2,
        mloc_roundHalfCeil = 3,
        mloc_roundHalfFloor = 4,
        mloc_roundCeil = 5,
        mloc_roundFloor = 6,
        mloc_roundExpand = 7,
        mloc_roundTrunc = 8,
    } mlocRoundingMode;

    // The operands CLDR's plural rules read (UTS #35 Part 3). An integer
    // of more than 18 digits is 10^18 plus its low 18 digits: it equals
    // no literal of a rule and keeps every remainder by a power of ten up
    // to 10^18.
    typedef struct mlocPluralOperands
    {
        // The integer digits.
        uint64_t i;
        // The fraction digits shown, as an integer.
        uint64_t f;
        // The same without trailing zeros.
        uint64_t t;
        // The count of fraction digits shown.
        uint32_t v;
        // The same without trailing zeros.
        uint32_t w;
        // The compact exponent (c, also e): set by compact formatting,
        // 0 otherwise.
        uint32_t c;
    } mlocPluralOperands;

    /// Makes the decimal of a signed integer, with no fraction digits.
    ///
    /// @param value       The integer.
    /// @param decimalOut  Receives the decimal.
    /// @return mloc_success; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeDecimal(int64_t value, mlocDecimal* decimalOut);

    /// Makes the decimal of an unsigned integer, with no fraction digits.
    ///
    /// @param value       The integer.
    /// @param decimalOut  Receives the decimal.
    /// @return mloc_success; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeUnsignedDecimal(uint64_t value,
                                                               mlocDecimal* decimalOut);

    /// Makes the shortest decimal that reads back as a double: of the
    /// decimals with the fewest digits that round to it, the nearest,
    /// ties to even (0.1 is "0.1", not "0.1000000000000000055511151231257827").
    /// Its fraction digits shown are those its digits need. Infinity and
    /// NaN give decimals of those kinds, and -0.0 a negative zero. The
    /// digits come from integer arithmetic, the same on every platform.
    ///
    /// @param value       The double.
    /// @param decimalOut  Receives the decimal.
    /// @return mloc_success; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMakeShortestDecimal(double value,
                                                               mlocDecimal* decimalOut);

    /// Parses a decimal: an optional "+" or "-", digits with an optional
    /// "." and fraction (at least one digit in all), and an optional
    /// exponent, "e" or "E" with an optional sign and digits. The
    /// fraction digits written are the ones shown, the exponent moving
    /// the point: "1.50" shows two, "1.50e1" is 15.0 and shows one.
    ///
    /// @param text        The text, without spaces.
    /// @param length      Its length in bytes.
    /// @param decimalOut  Receives the decimal; left unchanged on failure.
    /// @return status mloc_success with offset the length;
    ///         mloc_errorSyntax with the offset of the byte at fault;
    ///         mloc_errorLimit with offset 0 for more than
    ///         MLOC_MAX_DECIMAL_DIGITS significant digits, or digits or
    ///         fraction digits shown past 10^MLOC_MAX_DECIMAL_EXPONENT either
    ///         way; mloc_errorInvalid for a null
    ///         pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocTextResult mlocParseDecimal(const char* text, size_t length,
                                                            mlocDecimal* decimalOut);

    /// Writes a decimal as plain text with every digit and the fraction
    /// digits it shows, such as "-1234.50"; "Infinity", "-Infinity" or
    /// "NaN" for the others. mlocParseDecimal reads a finite one back to
    /// the same value. It reports no parts.
    ///
    /// @param decimal  The decimal.
    /// @param textOut  Where the text goes.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorInvalid for a null pointer or a decimal the
    ///         library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDecimal(const mlocDecimal* decimal,
                                                         mlocOutput* textOut);

    /// Rounds a decimal to a multiple of increment times 10^position: at
    /// position -2 with increment 5, 1.234 becomes 1.25 and 1.2 stays. It
    /// then shows the fewer of its fraction digits and -position (at
    /// least those its digits need), so 1.999 at -2 is 2.00 and 1.5 at -2
    /// stays 1.5. A value that rounds to zero keeps its sign. Infinity and
    /// NaN are left as they are.
    ///
    /// @param decimal    The decimal, rounded in place; left unchanged on
    ///                   failure.
    /// @param position   The power of ten of the increment's unit, within
    ///                   ±MLOC_MAX_DECIMAL_EXPONENT.
    /// @param increment  1, 2, 5 or 25 (ECMA-402's 10, 20, 50, 250 and
    ///                   larger are these at a higher position); 0 means 1.
    /// @param mode       How a value between two multiples rounds.
    /// @return mloc_success; mloc_errorLimit for a position past
    ///         MLOC_MAX_DECIMAL_EXPONENT either way, or a rounded value past
    ///         10^MLOC_MAX_DECIMAL_EXPONENT or MLOC_MAX_DECIMAL_DIGITS digits; mloc_errorInvalid
    ///         for a null pointer, a decimal the library did not make, or another increment or
    ///         mode.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocRoundDecimal(mlocDecimal* decimal, int32_t position,
                                                        uint32_t increment, mlocRoundingMode mode);

    /// Sets the fraction digits a decimal shows, never fewer than its
    /// digits need: 1.5 shows 1.500 with three and 1.50 shows 1.5 with
    /// zero. The value is unchanged; mlocRoundDecimal removes digits.
    ///
    /// @param decimal  The decimal, changed in place; left unchanged on
    ///                 failure.
    /// @param count    The fraction digits to show, 0 to
    ///                 MLOC_MAX_DECIMAL_EXPONENT.
    /// @return mloc_success, also for infinity and NaN, which show none;
    ///         mloc_errorLimit for a count past MLOC_MAX_DECIMAL_EXPONENT;
    ///         mloc_errorInvalid for a null pointer, a decimal the library
    ///         did not make, or a negative count.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocShowFractionDigits(mlocDecimal* decimal, int32_t count);

    /// Computes the plural operands of a finite decimal; c is 0.
    ///
    /// @param decimal      The decimal.
    /// @param operandsOut  Receives the operands.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, a
    ///         decimal that is not finite, or one the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetPluralOperands(const mlocDecimal* decimal,
                                                             mlocPluralOperands* operandsOut);

#ifdef __cplusplus
}
#endif

#endif
