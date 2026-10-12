// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units of measurement (UTS #35 Parts 2 and 6, record mloc-0023): CLDR's
// unit identifiers ("kilometer-per-hour", "square-foot", "foot-and-inch")
// and exact conversion between them by CLDR's units.xml.

#ifndef MAUL_LOCALE_UNIT_H
#define MAUL_LOCALE_UNIT_H

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

#ifdef __cplusplus
}
#endif

#endif
