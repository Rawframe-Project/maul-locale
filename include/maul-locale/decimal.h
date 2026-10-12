// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Decimal values and their plural operands (record mloc-0019): a
// number is formatted from its decimal digits, never from a binary
// value, so the digits a user typed are the digits shown.

#ifndef MAUL_LOCALE_DECIMAL_H
#define MAUL_LOCALE_DECIMAL_H

#include "maul-locale/base.h"

#ifdef __cplusplus
extern "C"
{
#endif

    enum
    {
        // The significant digits a decimal holds.
        mlocDecimalDigits = 40,
        // The largest power of ten a decimal's digits reach, either way.
        mlocDecimalMaxExponent = 999999,
    };

    // What a decimal is.
    typedef enum mlocDecimalKind
    {
        mloc_decimalFinite = 0,
        mloc_decimalInfinity = 1,
        mloc_decimalNaN = 2,
    } mlocDecimalKind;

    // A decimal number with the fraction digits it shows. The library
    // writes every byte, so two decimals compare equal with memcmp when
    // they have the same value and show the same digits. Make one with
    // mlocMakeDecimal, mlocMakeUnsignedDecimal or mlocParseDecimal.
    typedef struct mlocDecimal
    {
        // The significant digits, 0 to 9, most significant first: the
        // first and last are not 0. Unused bytes are 0.
        uint8_t digits[mlocDecimalDigits];
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
        uint8_t reserved;
    } mlocDecimal;

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
    ///         mloc_errorCapacity with offset 0 for more than 40
    ///         significant digits, or digits or fraction digits shown
    ///         past 10^999999 either way; mloc_errorInvalid for a null
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
    /// @param output   Where the text goes.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorInvalid for a null pointer or a decimal the
    ///         library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatDecimal(const mlocDecimal* decimal,
                                                         mlocOutput* output);

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
