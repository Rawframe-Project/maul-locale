// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Numbers in words, ordinal suffixes and the algorithmic numbering
// systems (UTS #35 Part 3, record mloc-0034): CLDR's rule-based number
// formats, as ICU4C reads and runs them, on the exact decimal. The rules
// are in a data file of their own, spellout.mloc, which a program adds to
// its data set.

#ifndef MAUL_LOCALE_SPELLOUT_H
#define MAUL_LOCALE_SPELLOUT_H

#include "maul-locale/locale.h"
#include "maul-locale/number.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A locale's formatter of one rule set: a value that points into the
    // data it was made from, which must outlive it. Opaque; make it with
    // mlocInitSpelloutFormatter.
    typedef struct mlocSpelloutFormatter
    {
        uint64_t opaque[32];
    } mlocSpelloutFormatter;

    /// Makes a locale's formatter of a rule set: words
    /// ("%spellout-cardinal", "%spellout-ordinal-feminine",
    /// "%spellout-numbering-year"), ordinal suffixes ("%digits-ordinal")
    /// or a numbering system's rules ("%roman-upper"). The rules are the
    /// first the locale's chain has of each group (spell-out, ordinal
    /// suffixes, numbering systems), a group taken whole as ICU takes it.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units, the plural
    ///                      rules and the spell-out rules (spellout.mloc).
    /// @param locale        The locale, canonicalized.
    /// @param ruleSet       A public rule set's name, with its "%" as CLDR
    ///                      writes it, or null for the locale's default
    ///                      (ICU's: "%spellout-numbering").
    /// @param length        The name's length in bytes, 0 with null.
    /// @return mloc_success; mloc_fellBack when the locale has no rules of
    ///         its own and root's, which write digits, stand instead, or
    ///         when the numbering system asked for has no digits and the
    ///         default writes the digits of patterns; mloc_errorNotFound
    ///         for a rule set the locale's rules do not have, or data
    ///         without the spell-out rules or the number units;
    ///         mloc_errorInvalid for a null pointer, a length with a null
    ///         name, or a private rule set's name ("%%...").
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocInitSpelloutFormatter(mlocSpelloutFormatter* formatterOut, const mlocDataSet* data,
                              const mlocLocale* locale, const char* ruleSet, size_t length);

    /// Formats a decimal by the formatter's rules: its value, not the
    /// digits it shows ("1.10" is written as 1.1, "-0" as 0), exactly
    /// (ICU4C's results wherever its int64_t or double holds the value;
    /// long fractions and values past int64_t are written exactly where
    /// ICU loses digits). Nothing is rounded: round the decimal first
    /// (mlocRoundDecimal).
    ///
    /// @param formatter  A formatter from mlocInitSpelloutFormatter.
    /// @param decimal    The value.
    /// @param output     Receives the text; on mloc_errorCapacity its
    ///                   length is the size needed. The text has no parts.
    /// @return mloc_success; mloc_errorCapacity for a buffer too small,
    ///         or rules that nest past 64 levels for a value (a value far
    ///         past the rules' largest base) or would divide it to more
    ///         digits than a decimal holds; mloc_errorInvalid for a null
    ///         pointer, an invalid decimal, an output asking for parts, or
    ///         a formatter the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatSpellout(const mlocSpelloutFormatter* formatter,
                                                          const mlocDecimal* decimal,
                                                          mlocOutput* output);

    /// The number of public rule sets a locale's spell-out and ordinal
    /// suffix rules have, the names mlocGetSpelloutRuleSet lists.
    ///
    /// @param data    The data with the spell-out rules.
    /// @param locale  The locale, canonicalized.
    /// @return The count; 0 for a null pointer or data without the rules.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API size_t mlocGetSpelloutRuleSetCount(const mlocDataSet* data,
                                                               const mlocLocale* locale);

    /// A public rule set's name, with its "%": the spell-out rules' in
    /// their order, then the ordinal suffix rules'.
    ///
    /// @param data       The data with the spell-out rules.
    /// @param locale     The locale, canonicalized.
    /// @param index      The rule set's index, below
    ///                   mlocGetSpelloutRuleSetCount.
    /// @param nameOut    Receives the name, in the data, not
    ///                   NUL-terminated.
    /// @param lengthOut  Receives its length in bytes.
    /// @return mloc_success; mloc_errorNotFound for an index past the last
    ///         or data without the rules; mloc_errorInvalid for a null
    ///         pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetSpelloutRuleSet(const mlocDataSet* data,
                                                              const mlocLocale* locale,
                                                              size_t index, const char** nameOut,
                                                              size_t* lengthOut);

#ifdef __cplusplus
}
#endif

#endif
