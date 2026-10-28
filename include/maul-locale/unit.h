// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Units of measurement (UTS #35 Parts 2 and 6): CLDR's unit identifiers
// ("kilometer-per-hour", "square-foot", "foot-and-inch") and exact
// conversion between them by CLDR's units.xml, the units a locale prefers
// for a usage, and amounts of a unit formatted with its name in a locale.
// The names are in a data file of their own, units.mloc, which a program
// adds to its data set.

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
    ///         mloc_errorLimit for an identifier past an MLOC_MAX_UNIT_*
    ///         bound, or when a unit's size or the value passes what the
    ///         exact integers (MLOC_MAX_UNIT_SCALE_BITS) or a decimal hold.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocConvertUnit(const mlocDataSet* data, const char* from,
                                                       size_t fromLength, const char* to,
                                                       size_t toLength,
                                                       const mlocPrecision* precision,
                                                       const mlocDecimal* value,
                                                       mlocDecimal* valueOut);

// A unit's fixed bounds, refused with mloc_errorLimit.
//
// The longest unit identifier read, in bytes, also in its normal form.
#define MLOC_MAX_UNIT_ID 255
// The most single units a core unit has ("kilogram-meter-per-square-second"
// has three).
#define MLOC_MAX_UNIT_FACTORS 8
// The most parts a mixed unit has ("hour-and-minute-and-second").
#define MLOC_MAX_UNIT_PARTS 4
// The largest constant in a unit identifier ("liter-per-100-kilometer"),
// and the largest product of the constants on either side of "per".
#define MLOC_MAX_UNIT_CONSTANT UINT64_MAX
// The width of the exact integers a conversion computes in: a unit's size
// as a fraction of its base unit, and a value in it, fit them.
#define MLOC_MAX_UNIT_SCALE_BITS 2048
// The longest pattern a unit formatter builds for a unit, in bytes.
#define MLOC_MAX_UNIT_PATTERN 256
// The bytes a unit formatter keeps its patterns in, each kept once.
#define MLOC_MAX_UNIT_PATTERN_POOL 2048
// The pattern sets a unit formatter keeps: one per output unit, or per
// part of a mixed one.
#define MLOC_MAX_UNIT_SLOTS 8

    // The amounts of a unit chosen by preference: one, or one per part of
    // a mixed unit, the largest first. Parts before the last are integers;
    // only the first carries the sign ("-3 hours 27 minutes").
    typedef struct mlocUnitAmounts
    {
        mlocDecimal amounts[MLOC_MAX_UNIT_PARTS];
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
    ///         not fit unitOut (its length is then the length needed);
    ///         mloc_errorLimit for an identifier past an MLOC_MAX_UNIT_*
    ///         bound or a value past the exact integers; mloc_errorSyntax
    ///         for a unit
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
    // ("3 km"), narrow ("3km") or long ("3 kilometers"); or numeric, as
    // ICU's MeasureFormat: a mixed unit of hours, minutes and seconds by
    // the locale's duration patterns ("1:05:09"), any other unit, and a
    // negative or non-finite amount, as narrow.
    typedef enum mlocUnitWidth
    {
        mloc_unitWidthShort = 0,
        mloc_unitWidthNarrow = 1,
        mloc_unitWidthLong = 2,
        mloc_unitWidthNumeric = 3,
    } mlocUnitWidth;

    // An angle written as a coordinate with the locale's direction
    // ("12°N"): a latitude's sign gives north or south, a longitude's
    // east or west, and the amount is written unsigned.
    typedef enum mlocCoordinate
    {
        mloc_coordinateNone = 0,
        mloc_coordinateLatitude = 1,
        mloc_coordinateLongitude = 2,
    } mlocCoordinate;

    // A grammatical case, CLDR's (grammaticalFeatures.xml): the
    // nominative, then the others in alphabetical order. The long width
    // writes a unit in the case's patterns where the locale has them
    // ("2 Metern", German's dative), the nominative's where it does not,
    // as ICU's unitDisplayCase; the other widths have no cases.
    typedef enum mlocGrammaticalCase
    {
        mloc_caseNominative = 0,
        mloc_caseAbessive = 1,
        mloc_caseAblative = 2,
        mloc_caseAccusative = 3,
        mloc_caseAdessive = 4,
        mloc_caseAllative = 5,
        mloc_caseCausal = 6,
        mloc_caseComitative = 7,
        mloc_caseDative = 8,
        mloc_caseDelative = 9,
        mloc_caseElative = 10,
        mloc_caseErgative = 11,
        mloc_caseEssive = 12,
        mloc_caseGenitive = 13,
        mloc_caseIllative = 14,
        mloc_caseInessive = 15,
        mloc_caseInstrumental = 16,
        mloc_caseLocative = 17,
        mloc_caseLocativecopulative = 18,
        mloc_caseOblique = 19,
        mloc_casePartitive = 20,
        mloc_casePrepositional = 21,
        mloc_caseSociative = 22,
        mloc_caseSublative = 23,
        mloc_caseSuperessive = 24,
        mloc_caseTerminative = 25,
        mloc_caseTranslative = 26,
        mloc_caseVocative = 27,
    } mlocGrammaticalCase;

    // A grammatical gender, CLDR's (grammaticalFeatures.xml): none, then
    // the others in alphabetical order.
    typedef enum mlocGender
    {
        mloc_genderNone = 0,
        mloc_genderAnimate = 1,
        mloc_genderCommon = 2,
        mloc_genderFeminine = 3,
        mloc_genderInanimate = 4,
        mloc_genderMasculine = 5,
        mloc_genderNeuter = 6,
        mloc_genderPersonal = 7,
    } mlocGender;

    // A unit formatter's options; the zero value is the default.
    typedef struct mlocUnitOptions
    {
        // The notation, grouping, sign display, minimum integer digits and
        // precision, as for numbers; the style must be decimal.
        mlocNumberOptions number;
        mlocUnitWidth width;
        // A usage ("road", "person-height"; UTS #35 Part 6): when its
        // length is not 0, the amount, of the formatter's unit, is
        // written in the unit the locale prefers for its value, as
        // mlocGetUnitPreference chooses it. Null and 0 for none.
        const char* usage;
        size_t usageLength;
        mlocCoordinate coordinate;
        mlocGrammaticalCase grammaticalCase;
    } mlocUnitOptions;

    // A locale's formatter of one unit: a value that points into the data
    // it was made from, which must outlive it. Opaque; make it with
    // mlocInitUnitFormatter.
    typedef struct mlocUnitFormatter
    {
        uint64_t opaque[448];
    } mlocUnitFormatter;

    /// Makes a locale's formatter of a unit: the number as for numbers, in
    /// the unit's pattern for the plural category of the number as
    /// rounded ("1 meter", "2 meters"), a category the locale lacks
    /// taking the other category's. A pattern may leave the number out
    /// (Arabic's dual names), as CLDR's data does. A unit CLDR does not
    /// name ("kilometer-per-second", "square-millimeter") takes a name
    /// built from the locale's compound patterns as ICU builds it, its
    /// identifier normalized first, as ICU normalizes it. A mixed unit
    /// ("foot-and-inch") takes an amount of its largest part and writes
    /// each part ("5 ft, 6 in"), joined by the locale's unit list of the
    /// width: the larger parts whole, as the locale's default number
    /// format writes them, the smallest with the options, and the first
    /// part given with the amount's sign. With a usage, the amount, of
    /// the unit given, is written in the unit the locale prefers for its
    /// value, as mlocGetUnitPreference chooses and rounds it (a precision
    /// in the options wins over the preference's), mixed outputs as
    /// above. With a coordinate, an angle is written with the locale's
    /// direction for its sign and the width ("12.5°N"; numeric takes
    /// narrow's), the amount unsigned whatever the sign display. With a
    /// grammatical case and the long width, each name is the case's
    /// where the locale's chain has it, as ICU's unitDisplayCase reads
    /// it, a built name taking it through CLDR's case derivations.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units, the plural
    ///                      rules and the unit names (units.mloc).
    /// @param locale        The locale, canonicalized.
    /// @param unit          A core unit identifier ("meter",
    ///                      "kilometer-per-hour", "square-millimeter") or a
    ///                      mixed unit ("foot-and-inch").
    /// @param unitLength    Its length in bytes.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the numbering system asked
    ///         for has no digits and the default stands instead;
    ///         mloc_errorSyntax for an identifier UTS #35 does not allow;
    ///         mloc_errorNotFound for a unit the names do not know (a
    ///         simple unit without names, a power past three) or data
    ///         without the number units, the unit table, the names, the
    ///         unit patterns, the lists or, for a case other than the
    ///         nominative, the case patterns;
    ///         mloc_errorLimit for an identifier past an MLOC_MAX_UNIT_*
    ///         bound, or a unit whose patterns pass the formatter's room
    ///         (MLOC_MAX_UNIT_PATTERN bytes each, MLOC_MAX_UNIT_PATTERN_POOL
    ///         in all, MLOC_MAX_UNIT_SLOTS sets); mloc_errorInvalid for a null pointer, an
    ///         empty unit, a currency, a constant in the numerator, a name
    ///         ICU refuses to build (the number inside a part), a mixed
    ///         unit whose parts do not convert to each other alone (other
    ///         dimensions, a temperature) or are of one size, or with a
    ///         notation other than standard, a usage with a mixed unit,
    ///         one that does not convert or a notation other than
    ///         standard, a coordinate of a unit not an angle or with a
    ///         usage, or options out of range (a case past
    ///         mloc_caseVocative) or in conflict (a percent style, a null
    ///         usage of a length).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitUnitFormatter(mlocUnitFormatter* formatterOut,
                                                             const mlocDataSet* data,
                                                             const mlocLocale* locale,
                                                             const char* unit, size_t unitLength,
                                                             const mlocUnitOptions* options);

    /// Formats an amount of the formatter's unit (of a mixed unit's largest
    /// part, split exactly: whole larger parts, the rest in the smallest,
    /// rounded, a part that rounds up to the next carrying; NaN and the
    /// infinities in the largest part, the others 0).
    ///
    /// @param formatter  A formatter from mlocInitUnitFormatter.
    /// @param decimal    The amount.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values, each unit name as
    ///                   mloc_numberPartUnit and a unit list's text as
    ///                   mloc_numberPartLiteral.
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed); mloc_errorLimit
    ///         when rounding or a part passes MLOC_MAX_DECIMAL_DIGITS or
    ///         MLOC_MAX_DECIMAL_EXPONENT; mloc_errorInvalid for a
    ///         null pointer, or a formatter or decimal the library did not
    ///         make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatUnit(const mlocUnitFormatter* formatter,
                                                      const mlocDecimal* decimal,
                                                      mlocOutput* output);

    /// Formats a range of two amounts, as mlocFormatNumberRange does, the
    /// unit written once in the category CLDR's plural ranges give the
    /// ends' ("3–5 meters"), as ICU collapses a unit. With a usage, ends
    /// whose values choose one output unit, not mixed, collapse so
    /// ("30–70 ft"); others are each written whole, joined by the spaced
    /// range separator ("30 ft – 1.2 mi").
    ///
    /// @param formatter  A formatter from mlocInitUnitFormatter.
    /// @param start      The range's start.
    /// @param end        Its end.
    /// @param output     Receives the text; its part function, when set,
    ///                   receives mlocNumberPart values with the end each
    ///                   belongs to (mloc_numberRangeStart,
    ///                   mloc_numberRangeEnd, or 0 for both).
    /// @return mloc_success; mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed); mloc_errorLimit
    ///         when rounding passes MLOC_MAX_DECIMAL_EXPONENT;
    ///         mloc_errorInvalid for a null pointer, a NaN end, a formatter of a mixed unit without
    ///         a usage (which ICU does not range either), a formatter of a coordinate, or a
    ///         formatter or decimal the library did not make.
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
    /// @param unit        A unit CLDR names, by any identifier of it
    ///                    ("metric-ton" for "tonne").
    /// @param unitLength  Its length in bytes.
    /// @param width       The width; numeric writes narrow's name.
    /// @param output      Receives the text.
    /// @return mloc_success; mloc_fellBack when no locale of the chain
    ///         names it and the identifier is written instead;
    ///         mloc_errorCapacity when the text does not fit
    ///         (output->length is then the length needed);
    ///         mloc_errorSyntax for an identifier UTS #35 does not allow;
    ///         mloc_errorLimit for one past an MLOC_MAX_UNIT_* bound;
    ///         mloc_errorNotFound for a unit the names do not know (a
    ///         compound CLDR does not name), data without them, or a
    ///         chain that cannot be walked; mloc_errorInvalid for a null
    ///         pointer, an empty unit, a mixed unit, a currency, a
    ///         constant in the numerator or a width out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitDisplayName(const mlocDataSet* data,
                                                              const mlocLocale* locale,
                                                              const char* unit, size_t unitLength,
                                                              mlocUnitWidth width,
                                                              mlocOutput* output);

    /// Gives a unit's grammatical gender in a locale, the one a message
    /// agreeing with a formatted amount needs, as ICU's
    /// FormattedNumber::getNounClass gives it: a unit CLDR names has its
    /// long name's ("Meter", masculine in German), else in a language
    /// whose units have genders what CLDR's gender derivations make of
    /// its parts ("kilometer-per-hour" takes kilometer's, a prefix and a
    /// power keeping meter's). A language without genders, and a mixed
    /// unit, give mloc_genderNone.
    ///
    /// @param data        The data with the unit table and the unit names.
    /// @param locale      The locale, canonicalized.
    /// @param unit        A core unit identifier or a mixed unit.
    /// @param unitLength  Its length in bytes.
    /// @param genderOut   Receives the gender; left unchanged on failure.
    /// @return mloc_success; mloc_errorSyntax for an identifier UTS #35
    ///         does not allow; mloc_errorNotFound for a simple unit the
    ///         table does not know, or data without the unit table or the
    ///         names; mloc_errorLimit for an identifier past an
    ///         MLOC_MAX_UNIT_* bound, also in its normal form; mloc_errorInvalid for a null
    ///         pointer, an empty unit, a currency or a constant in the
    ///         numerator.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetUnitGender(const mlocDataSet* data,
                                                         const mlocLocale* locale, const char* unit,
                                                         size_t unitLength, mlocGender* genderOut);

#ifdef __cplusplus
}
#endif

#endif
