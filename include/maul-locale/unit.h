// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units of measurement (UTS #35 Parts 2 and 6, records mloc-0023 and
// mloc-0024): CLDR's unit identifiers ("kilometer-per-hour",
// "square-foot", "foot-and-inch") and exact conversion between them by
// CLDR's units.xml, the units a locale prefers for a usage, and amounts
// of a unit formatted with its name in a locale. The names are in a data
// file of their own, units.mloc, which a program adds to its data set.

#ifndef MAUL_LOCALE_UNIT_H
#define MAUL_LOCALE_UNIT_H

#include "maul-locale/locale.h"
#include "maul-locale/number.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /// Converts a value from one unit to another of the same dimensions
    /// ("mile" to "kilometer", "foot-per-minute" to "mile-per-hour") or of
    /// the opposite ones ("mile-per-gallon" to "liter-per-100-kilometer",
    /// as UTS #35's reciprocals). The units are CLDR core unit
    /// identifiers: simple units with SI and binary prefixes, powers,
    /// products, "per" and unit constants ("per-100-kilometer");
    /// deprecated ones are replaced ("metric-ton" is "tonne"). The
    /// conversion is exact, by CLDR's factors as fractions, and the value
    /// is rounded once, by the precision. A temperature alone converts
    /// with its offset ("celsius" to "fahrenheit"); "beaufort" by UTS #35's
    /// force ranges.
    ///
    /// @param data        The data with the unit table.
    /// @param from        The unit of the value.
    /// @param fromLength  Its length in bytes.
    /// @param to          The unit wanted.
    /// @param toLength    Its length in bytes.
    /// @param precision   How to round, or null: the zero precision gives
    ///                    an exact result as it is and any other rounded
    ///                    half even to 38 significant digits. A precision
    ///                    that keeps more than 38 significant digits of an
    ///                    inexact result shows digits past the 38th that
    ///                    may be one unit off.
    /// @param value       The value.
    /// @param valueOut    Receives the converted value; left unchanged on
    ///                    failure.
    /// @return mloc_success; mloc_errorSyntax for an identifier that is
    ///         not well-formed; mloc_errorNotFound for a simple unit the
    ///         data does not know, a private-use unit, or data without the
    ///         unit table; mloc_errorInvalid for a null pointer, an invalid
    ///         precision or decimal, a mixed unit, or units that do not
    ///         convert to each other (a currency unit converts to none);
    ///         mloc_errorCapacity when a unit's size or the value passes
    ///         what the exact integers (2,048 bits) or a decimal hold.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocConvertUnit(const mlocDataSet* data, const char* from,
                                                       size_t fromLength, const char* to,
                                                       size_t toLength,
                                                       const mlocPrecision* precision,
                                                       const mlocDecimal* value,
                                                       mlocDecimal* valueOut);

    enum
    {
        // The most parts a mixed unit has ("hour-and-minute-and-second").
        mlocUnitMaxParts = 4,
    };

    // The amounts of a unit chosen by preference: one, or one per part of
    // a mixed unit, the largest first. Parts before the last are integers;
    // only the first carries the sign ("-3 hours 27 minutes").
    typedef struct mlocUnitAmounts
    {
        mlocDecimal amounts[mlocUnitMaxParts];
        uint8_t count;
        // The preference's rounding increment in units of the last part
        // (CLDR's skeleton "precision-increment/50"), 0 for none.
        uint16_t increment;
    } mlocUnitAmounts;

    /// Chooses the unit a locale prefers for a value of a usage ("road",
    /// "person-height"; UTS #35 Part 6, Unit Preferences) and converts the
    /// value to it. A valid -u-mu key ("celsius", "fahrenhe", "kelvin")
    /// that converts from the unit decides alone. Otherwise the unit's
    /// quantity (by its base unit: its quantity, the reciprocal's, then
    /// the same with numerator and denominator cancelled) selects
    /// CLDR's preferences for the usage, falling back by dropping the
    /// usage's last part, then to "default"; the region is the one
    /// mlocGetLocalePreferences takes, its preferences or else 001's, and
    /// -u-ms ("metric", "ussystem", "uksystem") moves to 001, the US or
    /// Great Britain when a unit there is of another system. The first
    /// unit whose threshold the value reaches, in that unit, is chosen
    /// (NaN reaches every one). A quantity without preferences takes its
    /// base unit; a unit of no quantity, its base units ("candela-per-bit"
    /// for "candela-per-byte"). A mixed unit's value is split exactly:
    /// whole larger parts, the rest in the last, which alone is rounded;
    /// a part that rounds up to the next part's size carries into it.
    ///
    /// @param data        The data with the unit table, the unit
    ///                    preferences and the likely subtags.
    /// @param locale      The locale, canonicalized.
    /// @param unit        The value's unit, a core unit identifier.
    /// @param unitLength  Its length in bytes.
    /// @param usage       The usage, or null with length 0 for "default".
    /// @param usageLength Its length in bytes.
    /// @param value       The value.
    /// @param precision   How to round the last amount, or null not to round
    ///                    (as mlocConvertUnit's zero precision: exact, or 38
    ///                    significant digits). The zero precision takes the
    ///                    preference's increment when it has one, else an
    ///                    integer of at least two significant digits, as
    ///                    ICU does.
    /// @param unitOut     Receives the chosen unit's identifier.
    /// @param amountsOut  Receives the amounts; left unchanged on failure.
    /// @return mloc_success; mloc_errorCapacity when the identifier does
    ///         not fit unitOut (its length is then the length needed) or a
    ///         value passes the exact integers; mloc_errorSyntax for a unit
    ///         or usage that is not well-formed; mloc_errorNotFound for a
    ///         simple unit the data does not know or data without the
    ///         tables; mloc_errorInvalid for a null pointer, an invalid
    ///         decimal or precision, or a unit that is mixed or does not
    ///         convert.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitPreference(
        const mlocDataSet* data, const mlocLocale* locale, const char* unit, size_t unitLength,
        const char* usage, size_t usageLength, const mlocDecimal* value,
        const mlocPrecision* precision, mlocOutput* unitOut, mlocUnitAmounts* amountsOut);

    // How a unit's name is written, as ECMA-402's unitDisplay: short
    // ("3 km"), narrow ("3km") or long ("3 kilometers").
    typedef enum mlocUnitWidth
    {
        mloc_unitWidthShort = 0,
        mloc_unitWidthNarrow = 1,
        mloc_unitWidthLong = 2,
    } mlocUnitWidth;

    // A unit formatter's options; the zero value is the default.
    typedef struct mlocUnitOptions
    {
        // The notation, grouping, sign display, minimum integer digits and
        // precision, as for numbers; the style must be decimal.
        mlocNumberOptions number;
        mlocUnitWidth width;
    } mlocUnitOptions;

    // A locale's formatter of one unit: a value that points into the data
    // it was made from, which must outlive it. Opaque; make it with
    // mlocInitUnitFormatter.
    typedef struct mlocUnitFormatter
    {
        uint64_t opaque[168];
    } mlocUnitFormatter;

    /// Makes a locale's formatter of a unit: the number as for numbers, in
    /// the unit's pattern for the plural category of the number as
    /// rounded ("1 meter", "2 meters"), a category the locale lacks
    /// taking the other category's. A pattern may leave the number out
    /// (Arabic's dual names), as CLDR's data does.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units, the plural
    ///                      rules and the unit names (units.mloc).
    /// @param locale        The locale, canonicalized.
    /// @param unit          A unit CLDR names ("meter", "kilometer-per-hour",
    ///                      "celsius").
    /// @param unitLength    Its length in bytes.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorNotFound for a unit the names do not know or data
    ///         without the number units or the names; mloc_errorCapacity
    ///         for a unit whose patterns pass the formatter's room;
    ///         mloc_errorInvalid for a null pointer, an empty unit, or
    ///         options out of range or in conflict (a percent style).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitUnitFormatter(mlocUnitFormatter* formatterOut,
                                                             const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const char* unit, size_t unitLength,
                                                             const mlocUnitOptions* options);

    /// Formats an amount of the formatter's unit.
    ///
    /// @param formatter  A formatter from mlocInitUnitFormatter.
    /// @param decimal    The amount.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values, the unit's name as
    ///                   mloc_numberPartUnit.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed) or rounding
    ///         passes the decimal's limits; mloc_errorInvalid for a null
    ///         pointer, or a formatter or decimal the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatUnit(const mlocUnitFormatter* formatter,
                                                      const mlocDecimal* decimal,
                                                      mlocOutput* output);

    /// Formats a range of two amounts, as mlocFormatNumberRange does, the
    /// unit written once in the category CLDR's plural ranges give the
    /// ends' ("3–5 meters"), as ICU collapses a unit.
    ///
    /// @param formatter  A formatter from mlocInitUnitFormatter.
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
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatUnitRange(const mlocUnitFormatter* formatter,
                                                           const mlocDecimal* start,
                                                           const mlocDecimal* end,
                                                           mlocOutput* output);

    /// Writes a unit's display name in a locale ("meters", "m"), as a menu
    /// of units shows it.
    ///
    /// @param data        The data with the unit names.
    /// @param locale      The locale, canonicalized.
    /// @param unit        A unit CLDR names.
    /// @param unitLength  Its length in bytes.
    /// @param width       The width.
    /// @param output      Receives the text.
    /// @return mloc_success; mloc_fellBack when no locale of the chain
    ///         names it and the identifier is written instead;
    ///         mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed);
    ///         mloc_errorNotFound for a unit the names do not know, data
    ///         without them, or a chain that cannot be walked;
    ///         mloc_errorInvalid for a null pointer, an empty unit or a
    ///         width out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitDisplayName(const mlocDataSet* data,
                                                              const mlocLocale* locale,
                                                              const char* unit, size_t unitLength,
                                                              mlocUnitWidth width,
                                                              mlocOutput* output);

#ifdef __cplusplus
}
#endif

#endif
