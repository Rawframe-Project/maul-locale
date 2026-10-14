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

    // A spell-out formatter's options; zero is the default.
    typedef struct mlocSpelloutOptions
    {
        // The first word titlecased by the context, as ICU does: when the
        // text begins with a lowercase letter, at the beginning of a
        // sentence always, in a list or menu or alone as the locale says.
        mlocCapitalization capitalization;
    } mlocSpelloutOptions;

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
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_fellBack when the locale has no rules of
    ///         its own and root's, which write digits, stand instead, or
    ///         when the numbering system asked for has no digits and the
    ///         default writes the digits of patterns; mloc_errorNotFound
    ///         for a rule set the locale's rules do not have, or data
    ///         without the spell-out rules or the number units;
    ///         mloc_errorInvalid for a null pointer, a length with a null
    ///         name, a private rule set's name ("%%..."), or options out of
    ///         range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitSpelloutFormatter(
        mlocSpelloutFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale,
        const char* ruleSet, size_t length, const mlocSpelloutOptions* options);

    /// Makes a formatter of an algorithmic numbering system by its CLDR id
    /// ("roman", "hebr", "hans", "jpanyear"), as ICU's NumberFormat makes
    /// one: the rules of the locale the system names, with that locale's
    /// plural rules and symbols ("hans" is zh's "%spellout-cardinal"), or
    /// for a system of root's rules ("roman", "armn", "geor") the locale
    /// asked for.
    ///
    /// @param formatterOut  Receives the formatter; left unchanged on
    ///                      failure.
    /// @param data          The data with the number units, the plural
    ///                      rules and the spell-out rules (spellout.mloc).
    /// @param locale        The locale, canonicalized.
    /// @param system        The numbering system's id.
    /// @param length        Its length in bytes.
    /// @param options       The options, or null for the defaults.
    /// @return mloc_success; mloc_errorNotFound for an id not of an
    ///         algorithmic system (the numeric ones, "latn" or "arab", are
    ///         the number formatter's), or data without the spell-out
    ///         rules or the number units; mloc_errorInvalid for a null
    ///         pointer or options out of range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitNumberingSystemFormatter(
        mlocSpelloutFormatter* formatterOut, const mlocDataSet* data, const mlocLocale* locale,
        const char* system, size_t length, const mlocSpelloutOptions* options);

    /// Formats a decimal by the formatter's rules: its value, not the
    /// digits it shows ("1.10" is written as 1.1, "-0" as 0), exactly
    /// (ICU4C's results wherever its int64_t or double holds the value;
    /// long fractions and values past int64_t are written exactly where
    /// ICU loses digits). Nothing is rounded: round the decimal first
    /// (mlocRoundDecimal).
    ///
    /// @param formatter  A formatter from mlocInitSpelloutFormatter or
    ///                   mlocInitNumberingSystemFormatter.
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

    /// Reads a number written in words (or ordinal suffixes, or a
    /// numbering system's numerals) back, as ICU4C's RuleBasedNumberFormat
    /// parses it, strictly: every public rule set of the formatter's
    /// group is tried and the longest reading kept, the first of equal
    /// ones, so a cardinal formatter also reads ordinals and years. The
    /// text must be written as the rules write it (case, spaces and
    /// hyphens as they are); digits in a rule's pattern read as the
    /// strict number parser reads them. Reading starts at the first byte
    /// and stops where no rule reads more: compare the end with the
    /// length to require the whole text. The value is exact (ICU's is a
    /// double).
    ///
    /// @param formatter   A formatter from mlocInitSpelloutFormatter or
    ///                    mlocInitNumberingSystemFormatter.
    /// @param text        UTF-8 text; it need not end in a 0 byte.
    /// @param length      Its length in bytes.
    /// @param decimalOut  Receives the value read: finite, NaN or an
    ///                    infinity; zero is never negative.
    /// @param endOut      Receives the offset after the text read, 0 when
    ///                    none is.
    /// @return mloc_success; mloc_errorSyntax when no rule reads anything
    ///         (or the rules are of a kind ICU refuses to parse);
    ///         mloc_errorCapacity when a value read passes a decimal's
    ///         digits, or text crafted against the search makes it take
    ///         more than 4,194,304 steps; mloc_errorInvalid for a null
    ///         pointer or a formatter the library did not make.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocParseSpellout(const mlocSpelloutFormatter* formatter,
                                                         const char* text, size_t length,
                                                         mlocDecimal* decimalOut, size_t* endOut);

    // Formatting and parsing nest rule sets up to 64 levels deep, as ICU
    // does, and take about 100 KB of stack at the deepest: a thread for
    // them, or a WebAssembly build (Emscripten's default stack is 64 KB),
    // needs a stack of 256 KB to format or parse rules nested that far.

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
