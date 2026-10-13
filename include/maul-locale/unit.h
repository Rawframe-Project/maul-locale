// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units of measurement (UTS #35 Parts 2 and 6, record mloc-0023): CLDR's
// unit identifiers ("kilometer-per-hour", "square-foot", "foot-and-inch")
// and exact conversion between them by CLDR's units.xml, and the units
// a locale prefers for a usage.

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

#ifdef __cplusplus
}
#endif

#endif
