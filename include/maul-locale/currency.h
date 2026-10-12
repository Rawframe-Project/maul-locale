// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency formatting (UTS #35 Part 3, record mloc-0022): an amount of an
// ISO 4217 currency in a locale's currency pattern, with its symbol or
// code, rounded to the currency's digits unless the options say
// otherwise. The options follow ECMA-402's NumberFormat with the
// currency style; their zero value is the default.

#ifndef MAUL_LOCALE_CURRENCY_H
#define MAUL_LOCALE_CURRENCY_H

#include "maul-locale/number.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // How the currency is shown: the locale's symbol ("$", "US$", "€"),
    // its narrow symbol ("$" for any dollar), its ISO code ("USD"), its
    // formal or variant symbol where the locale has one, or not at all
    // (digits as money, in the locale's noCurrency pattern, for a column
    // of amounts in one currency). A narrow, formal or variant symbol the
    // locale lacks is its symbol; a symbol it lacks is the code.
    typedef enum mlocCurrencyDisplay
    {
        mloc_currencySymbol = 0,
        mloc_currencyNarrowSymbol = 1,
        mloc_currencyCode = 2,
        mloc_currencyFormalSymbol = 3,
        mloc_currencyVariantSymbol = 4,
        mloc_currencyHidden = 5,
    } mlocCurrencyDisplay;

    // Which digits and rounding the zero precision takes: the currency's
    // usual ones, or those of its cash (Swiss francs to 0.05).
    typedef enum mlocCurrencyUsage
    {
        mloc_currencyUsageStandard = 0,
        mloc_currencyUsageCash = 1,
    } mlocCurrencyUsage;

    // A currency formatter's options; the zero value is the default.
    typedef struct mlocCurrencyOptions
    {
        // The notation (standard, scientific or engineering), grouping,
        // sign display, minimum integer digits and precision, as for
        // numbers; the style must be decimal. The zero precision rounds
        // to the currency's digits and rounding increment (UTS #35's
        // currencyData), its rounding mode and trailing zeros as given.
        mlocNumberOptions number;
        mlocCurrencyDisplay display;
        mlocCurrencyUsage usage;
    } mlocCurrencyOptions;

    // A locale's formatter of one currency: a value that points into the
    // data it was made from, which must outlive it. Opaque; make it with
    // mlocInitCurrencyFormatter.
    typedef struct mlocCurrencyFormatter
    {
        uint64_t opaque[32];
    } mlocCurrencyFormatter;

    /// Makes a locale's formatter of a currency. The numbering system is
    /// the locale's, as for numbers; the locale's -u-cf-account keyword
    /// asks for its accounting pattern ("($3.00)"). The symbol meets the
    /// digits as UTS #35 prefers: a locale's alphaNextToNumber pattern
    /// spaces a symbol whose character next to the digits is a letter
    /// ("USD 3.00", "$3.00"). A code the data does not know shows as its
    /// code with the default digits.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number and currency units,
    ///                      the parents and the likely subtags.
    /// @param locale        The locale, canonicalized.
    /// @param code          The currency's ISO 4217 code, three ASCII
    ///                      letters in any case.
    /// @param codeLength    Its length in bytes, 3.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for data without the number or currency
    ///         units; mloc_errorInvalid for a null pointer, a code that is
    ///         not three letters, or options out of range or in conflict
    ///         (a percent style, compact notation).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitCurrencyFormatter(
        mlocCurrencyFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale,
        const char* code, size_t codeLength, const mlocCurrencyOptions* options);

    /// Formats an amount of the formatter's currency.
    ///
    /// @param formatter  A formatter from mlocInitCurrencyFormatter.
    /// @param decimal    The amount.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values, the currency as
    ///                   mloc_numberPartCurrency.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed) or rounding
    ///         passes the decimal's limits; mloc_errorInvalid for a null
    ///         pointer, or a formatter or decimal the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatCurrency(const mlocCurrencyFormatter* formatter,
                                                          const mlocDecimal* decimal,
                                                          mlocOutput* output);

    /// Formats a range of two amounts, as mlocFormatNumberRange does
    /// ("$3.00 – $5.00", "3,00–5,00 €"; ends that write alike give
    /// "~$5.00").
    ///
    /// @param formatter  A formatter from mlocInitCurrencyFormatter.
    /// @param start      The range's start.
    /// @param end        Its end.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values with the end each
    ///                   belongs to (mloc_numberRangeStart,
    ///                   mloc_numberRangeEnd, or 0 for both).
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed) or rounding
    ///         passes the decimal's limits; mloc_errorInvalid for a null
    ///         pointer, a NaN end, or a formatter or decimal the library
    ///         did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocFormatCurrencyRange(const mlocCurrencyFormatter* formatter, const mlocDecimal* start,
                            const mlocDecimal* end, mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
