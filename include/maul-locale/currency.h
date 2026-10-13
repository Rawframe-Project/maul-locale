// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Currency formatting (UTS #35 Part 3, record mloc-0022): an amount of an
// ISO 4217 currency in a locale's currency pattern, with its symbol or
// code, rounded to the currency's digits unless the options say
// otherwise. The options follow ECMA-402's NumberFormat with the
// currency style; their zero value is the default. Lookups give a
// currency's symbol, name and digits, and the currency a locale or a
// region uses on a day.

#ifndef MAUL_LOCALE_CURRENCY_H
#define MAUL_LOCALE_CURRENCY_H

#include "maul-locale/number.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // How the currency is shown: the locale's symbol ("$", "US$", "€"),
    // its narrow symbol ("$" for any dollar), its ISO code ("USD"), its
    // formal or variant symbol where the locale has one, not at all
    // (digits as money, in the locale's noCurrency pattern, for a column
    // of amounts in one currency), or by name after the number in its
    // plural form ("3.00 US dollars", the decimal pattern and the
    // locale's unit pattern; the names are in the currency names data
    // file, record mloc-0022). A narrow, formal or variant symbol the
    // locale lacks is its symbol; a symbol it lacks is the code; a name
    // of a plural category is the other category's, else the currency's
    // name, else the code.
    typedef enum mlocCurrencyDisplay
    {
        mloc_currencySymbol = 0,
        mloc_currencyNarrowSymbol = 1,
        mloc_currencyCode = 2,
        mloc_currencyFormalSymbol = 3,
        mloc_currencyVariantSymbol = 4,
        mloc_currencyHidden = 5,
        mloc_currencyName = 6,
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
        // The notation, grouping, sign display, minimum integer digits and
        // precision, as for numbers; the style must be decimal. The zero
        // precision rounds to the currency's digits and rounding
        // increment (UTS #35's currencyData), its rounding mode and
        // trailing zeros as given; in compact notation it rounds as for
        // numbers. Compact notation takes the locale's short currency
        // patterns at either length ("$1.2K"; CLDR has no long ones),
        // and, hidden, its short decimal patterns.
        mlocNumberOptions number;
        mlocCurrencyDisplay display;
        mlocCurrencyUsage usage;
        // 1 to write, with a symbol display, the code after the amount in
        // the locale's currencyPatternAppendISO pattern ("$1,432.00 USD"),
        // which UTS #35 recommends where a narrow symbol is ambiguous; 0
        // not to.
        uint8_t appendCode;
    } mlocCurrencyOptions;

    // A locale's formatter of one currency: a value that points into the
    // data it was made from, which must outlive it. Opaque; make it with
    // mlocInitCurrencyFormatter.
    typedef struct mlocCurrencyFormatter
    {
        uint64_t opaque[96];
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
    ///         units (or, for names, the currency names);
    ///         mloc_errorInvalid for a null pointer, a code that is not
    ///         three letters, or options out of range or in conflict (a
    ///         percent style, the code appended to a display that is no
    ///         symbol).
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
    /// "~$5.00"). A name, or the appended code, is written once when both
    /// ends take the same plural category ("3.00–5.00 US dollars"), else
    /// for each.
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

    /// Reads an amount of money as a person types it (ICU's
    /// DecimalFormat::parse on a currency formatter, or with anyCurrency
    /// its parseCurrency), as mlocParseNumber reads a number: the
    /// formatter's separators and pattern, and the currency where the
    /// pattern or the person puts it: the formatter's symbol as written
    /// (or the text it shows, a narrow symbol), its code in any case, its
    /// long names case folded ("12.50 US dollars"). With anyCurrency, any
    /// currency the locale's chain names: its symbol (with root's
    /// equivalent signs, "＄" for "$"), its code, its names uppercased as
    /// the locale uppercases; a currency is then required. A formatter
    /// of long names or of the ISO append reads the text around its
    /// number as it writes it ("12.50 US dollars" in strict mode too).
    ///
    /// @param formatter    A formatter from mlocInitCurrencyFormatter, not
    ///                     of compact notation. Its long names are read
    ///                     when its data set had the names data file.
    /// @param text         UTF-8 text; it need not end in a 0 byte.
    /// @param length       Its length in bytes.
    /// @param mode         An mlocParseMode.
    /// @param anyCurrency  1 to read any currency, 0 for the formatter's
    ///                     alone.
    /// @param decimalOut   Receives the amount.
    /// @param codeOut      Receives the currency's code, three letters
    ///                     (the formatter's when none was typed).
    /// @param endOut       Receives the offset after the amount, or on
    ///                     mloc_errorSyntax the offset reading stopped at.
    /// @return mloc_success; mloc_errorSyntax when no amount is read (or
    ///         no currency, with anyCurrency); mloc_errorCapacity as for
    ///         mlocParseNumber; mloc_errorNotFound for data without the
    ///         parse sets, or with anyCurrency a chain of more than eight
    ///         currency units of a kind; mloc_errorInvalid for a null
    ///         pointer, a mode out of range, or a formatter the library did
    ///         not make or of compact notation, or anyCurrency past 1.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseCurrency(const mlocCurrencyFormatter* formatter,
                                                         const char* text, size_t length,
                                                         mlocParseMode mode, uint8_t anyCurrency,
                                                         mlocDecimal* decimalOut, char* codeOut,
                                                         size_t* endOut);

    /// Writes a currency's symbol, or a variant of it, as a locale shows
    /// it (the ECMA-402 DisplayNames of type currency take names, below).
    ///
    /// @param data        The data with the currency symbols.
    /// @param locale      The locale, canonicalized.
    /// @param code        The currency's ISO 4217 code, three ASCII letters.
    /// @param codeLength  Its length in bytes, 3.
    /// @param display     A symbol display, or the code.
    /// @param output      Receives the text.
    /// @return mloc_success; mloc_fellBack when the locale has no symbol and
    ///         the code is written instead; mloc_errorCapacity when the
    ///         text does not fit (output->length is then the length
    ///         needed); mloc_errorNotFound when the chain cannot be walked;
    ///         mloc_errorInvalid for a null pointer, a code that is not
    ///         three letters, or a display that is no symbol.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetCurrencySymbol(const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const char* code, size_t codeLength,
                                                             mlocCurrencyDisplay display,
                                                             mlocOutput* output);

    /// Writes a currency's name in a locale ("US Dollar", "Dollar
    /// américain"), as a menu of currencies shows it.
    ///
    /// @param data        The data with the currency names data file.
    /// @param locale      The locale, canonicalized.
    /// @param code        The currency's ISO 4217 code, three ASCII letters.
    /// @param codeLength  Its length in bytes, 3.
    /// @param output      Receives the text.
    /// @return mloc_success; mloc_fellBack when the data names it in no
    ///         locale of the chain and the code is written instead;
    ///         mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed);
    ///         mloc_errorNotFound when the chain cannot be walked;
    ///         mloc_errorInvalid for a null pointer or a code that is not
    ///         three letters.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetCurrencyName(const mlocDataSet* data,
                                                           const mlocLocale* locale,
                                                           const char* code, size_t codeLength,
                                                           mlocOutput* output);

    /// Gives a currency's fraction digits and rounding increment
    /// (supplementalData.xml): JPY 0, BHD 3, CHF cash 2 by 5.
    ///
    /// @param data          The data with the currency fractions.
    /// @param code          The currency's ISO 4217 code, three ASCII
    ///                      letters; a code the data does not list has
    ///                      the default, 2 digits.
    /// @param codeLength    Its length in bytes, 3.
    /// @param usage         Standard or cash.
    /// @param digitsOut     Receives the fraction digits.
    /// @param incrementOut  Receives the rounding increment in units of the
    ///                      last digit, 1 for none.
    /// @return mloc_success; mloc_errorNotFound for data without the
    ///         fractions; mloc_errorInvalid for a null pointer, a code
    ///         that is not three letters or a usage out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetCurrencyDigits(const mlocDataSet* data,
                                                             const char* code, size_t codeLength,
                                                             mlocCurrencyUsage usage,
                                                             uint8_t* digitsOut,
                                                             uint16_t* incrementOut);

    // A currency a region uses or used (UTS #35's currencyData). Days
    // count from the Unix epoch's day in the region's civil calendar (for
    // UTC, a Unix time divided by 86,400, rounded down), and a span
    // includes both its days.
    typedef struct mlocRegionCurrency
    {
        // The ISO 4217 code, NUL-padded.
        char code[4];
        // The first day in use; INT32_MIN when the data gives none.
        int32_t from;
        // The last day in use; INT32_MAX while in use.
        int32_t to;
        // 1 for legal tender, 0 for a code that is none (funds, metals,
        // the code of no currency).
        uint8_t tender;
    } mlocRegionCurrency;

    /// Finds the currency a locale uses on a day: its -u-cu key's
    /// ("en-US-u-cu-eur" takes EUR), else the first legal tender in use
    /// that day of its region, the one mlocGetLocalePreferences takes
    /// ("de" takes Germany's: DEM in 1990, EUR now), else the first code
    /// in use that is none (Antarctica's code of no currency). The
    /// region's currencies are in CLDR's order, which puts the primary
    /// one first.
    ///
    /// @param data     The data with the currency regions and the likely
    ///                 subtags.
    /// @param locale   The locale, canonicalized.
    /// @param day      The day (mlocRegionCurrency); INT32_MAX asks for
    ///                 the currencies still in use.
    /// @param codeOut  Receives the ISO 4217 code, NUL-padded, 4 bytes;
    ///                 left unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound for data without the
    ///         currency regions, or a region with no currency in use on
    ///         the day; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFindCurrency(const mlocDataSet* data,
                                                        const mlocLocale* locale, int32_t day,
                                                        char* codeOut);

    /// Lists the currencies a region uses and used, in CLDR's order: on
    /// any day the first legal tender in use is the region's primary one.
    ///
    /// @param data           The data with the currency regions.
    /// @param region         The region, two ASCII letters in any case or
    ///                       three digits.
    /// @param regionLength   Its length in bytes.
    /// @param currenciesOut  Receives the currencies; may be null when
    ///                       capacity is 0.
    /// @param capacity       The room in currenciesOut.
    /// @param countOut       Receives the count, 0 for a region the data
    ///                       does not list; with mloc_errorCapacity the
    ///                       count needed.
    /// @return mloc_success; mloc_errorCapacity when the currencies do
    ///         not fit; mloc_errorNotFound for data without the currency
    ///         regions; mloc_errorInvalid for a null pointer or a region
    ///         that is not two letters or three digits.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetRegionCurrencies(const mlocDataSet* data,
                                                               const char* region,
                                                               size_t regionLength,
                                                               mlocRegionCurrency* currenciesOut,
                                                               size_t capacity, size_t* countOut);

#ifdef __cplusplus
}
#endif

#endif
