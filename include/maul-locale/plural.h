// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Plural rules (UTS #35 Part 3): the category a number takes in a locale,
// cardinal ("1 file", "2 files") or ordinal ("1st", "2nd"), and that of a
// range of two numbers.

#ifndef MAUL_LOCALE_PLURAL_H
#define MAUL_LOCALE_PLURAL_H

#include "maul-locale/base.h"
#include "maul-locale/data.h"
#include "maul-locale/decimal.h"
#include "maul-locale/locale.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // CLDR's plural categories.
    typedef int32_t mlocPluralCategory;
    enum
    {
        mloc_pluralZero = 0,
        mloc_pluralOne = 1,
        mloc_pluralTwo = 2,
        mloc_pluralFew = 3,
        mloc_pluralMany = 4,
        mloc_pluralOther = 5,
    };

    // Which rules: counting things or ordering them.
    typedef int32_t mlocPluralType;
    enum
    {
        mloc_pluralCardinal = 0,
        mloc_pluralOrdinal = 1,
    };

    // A locale's plural rules of one type, with its range table: a value
    // that reads no data once made. Opaque; make it with
    // mlocInitPluralRules.
    typedef struct mlocPluralRules
    {
        uint64_t opaque[5];
    } mlocPluralRules;

    /// Finds a locale's plural rules along its chain in the plurals
    /// component: "pt-PT" has its own, "en-GB" takes "en"'s, and a
    /// language CLDR does not name takes root's (other for everything).
    ///
    /// @param data      The data with the plural units and the parents.
    /// @param locale    The locale, canonicalized.
    /// @param type      Cardinal or ordinal.
    /// @param rulesOut  Receives the rules; left unchanged on failure.
    /// @return mloc_success; mloc_fellBack when the rules are those of a
    ///         locale it falls back to; mloc_errorNotFound for data
    ///         without them; mloc_errorInvalid for a null pointer or
    ///         another type.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitPluralRules(const mlocDataSet* data,
                                                           const mlocLocale* locale,
                                                           mlocPluralType type,
                                                           mlocPluralRules* rulesOut);

    /// Selects the category of a decimal, by the digits it shows: in
    /// English 1 is one and 1.0 is other. Infinity and NaN are other.
    ///
    /// @param rules        Rules from mlocInitPluralRules.
    /// @param decimal      The number.
    /// @param categoryOut  Receives the category.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, or
    ///         rules or a decimal the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocPluralRules_Select(const mlocPluralRules* rules,
                                                              const mlocDecimal* decimal,
                                                              mlocPluralCategory* categoryOut);

    /// Selects the category of an integer, as that of its decimal with no
    /// fraction digits, without making the decimal.
    ///
    /// @param rules        Rules from mlocInitPluralRules.
    /// @param value        The integer.
    /// @param categoryOut  Receives the category.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or rules
    ///         the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocPluralRules_SelectInteger(
        const mlocPluralRules* rules, int64_t value, mlocPluralCategory* categoryOut);

    /// Selects the category of plural operands, for a compact number
    /// whose exponent (c) the caller sets: in French 1c6 ("1 M") is many.
    ///
    /// @param rules        Rules from mlocInitPluralRules.
    /// @param operands     The operands, as mlocGetPluralOperands gives
    ///                     them for the whole value, with c set.
    /// @param categoryOut  Receives the category.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or rules
    ///         the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocPluralRules_SelectOperands(const mlocPluralRules* rules, const mlocPluralOperands* operands,
                                   mlocPluralCategory* categoryOut);

    /// Selects the category of a range from its ends' categories, by the
    /// locale's table (English days from 1 to 2 are other); where CLDR has no
    /// entry, the end's category.
    ///
    /// @param rules        Cardinal rules from mlocInitPluralRules.
    /// @param start        The category of the range's start.
    /// @param end          The category of its end.
    /// @param categoryOut  Receives the category.
    /// @return mloc_success; mloc_errorInvalid for a null pointer, ordinal
    ///         rules, rules the library did not make, or another category.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocPluralRules_SelectRange(const mlocPluralRules* rules,
                                                                   mlocPluralCategory start,
                                                                   mlocPluralCategory end,
                                                                   mlocPluralCategory* categoryOut);

    /// Gives the categories a locale's rules use, as bits 1 << category:
    /// one and other for English cardinals, all six for Arabic's.
    ///
    /// @param rules          Rules from mlocInitPluralRules.
    /// @param categoriesOut  Receives the bits.
    /// @return mloc_success; mloc_errorInvalid for a null pointer or rules
    ///         the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocPluralRules_GetCategories(const mlocPluralRules* rules,
                                                                     uint32_t* categoriesOut);

#ifdef __cplusplus
}
#endif

#endif
