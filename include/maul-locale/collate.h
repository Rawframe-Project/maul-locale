// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Collation (UTS #10 and UTS #35 Part 5, records mloc-0064 to
// mloc-0071): text compared in a locale's order, as ICU4C's Collator
// compares it, from CLDR's root collation and the locales' tailorings in
// the collation data file (src/generated/collation.mloc). A locale's
// -u-co picks among its collation types; its -u- keys set the options
// (ks, ka, kv, kf, kc, kn, kb, kr); the options of the call override
// them. Text is always compared in NFD, whatever kk asks. Sort keys are
// ICU4C's bytes, with a version to store beside them.

#ifndef MAUL_LOCALE_COLLATE_H
#define MAUL_LOCALE_COLLATE_H

#include "maul-locale/locale.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // The levels compared (ks): base letters, then accents, case and
    // variants, punctuation when shifted, then the code points.
    typedef enum mlocCollationStrength
    {
        mloc_strengthOfLocale = 0,
        mloc_strengthPrimary = 1,
        mloc_strengthSecondary = 2,
        mloc_strengthTertiary = 3,
        mloc_strengthQuaternary = 4,
        mloc_strengthIdentical = 5,
    } mlocCollationStrength;

    // Whether spaces, punctuation and, by the max variable, symbols are
    // ignored until the quaternary level (ka).
    typedef enum mlocCollationAlternate
    {
        mloc_alternateOfLocale = 0,
        mloc_alternateNonIgnorable = 1,
        mloc_alternateShifted = 2,
    } mlocCollationAlternate;

    // The last group that is ignorable when shifted (kv).
    typedef enum mlocCollationMaxVariable
    {
        mloc_maxVariableOfLocale = 0,
        mloc_maxVariableSpace = 1,
        mloc_maxVariablePunctuation = 2,
        mloc_maxVariableSymbol = 3,
        mloc_maxVariableCurrency = 4,
    } mlocCollationMaxVariable;

    // Which case sorts first at the tertiary level (kf).
    typedef enum mlocCollationCaseFirst
    {
        mloc_caseFirstOfLocale = 0,
        mloc_caseFirstOff = 1,
        mloc_caseFirstLower = 2,
        mloc_caseFirstUpper = 3,
    } mlocCollationCaseFirst;

    // A switch of the options: the locale's, off or on.
    typedef enum mlocCollationSwitch
    {
        mloc_switchOfLocale = 0,
        mloc_switchOff = 1,
        mloc_switchOn = 2,
    } mlocCollationSwitch;

    // A collator's options; the zero value takes each from the locale,
    // and from CLDR's root where the locale says nothing (tertiary,
    // non-ignorable, punctuation, case first off, the switches off, no
    // reordering).
    typedef struct mlocCollatorOptions
    {
        mlocCollationStrength strength;
        mlocCollationAlternate alternate;
        mlocCollationMaxVariable maxVariable;
        mlocCollationCaseFirst caseFirst;
        // A case level between the secondary and tertiary levels (kc).
        mlocCollationSwitch caseLevel;
        // Runs of digits by their value (kn).
        mlocCollationSwitch numeric;
        // Accents compared from the end (kb, French's old order).
        mlocCollationSwitch backwards;
        // The scripts and groups reordered (kr), as its value is written:
        // four-letter script codes, space, punct, symbol, currency, digit
        // and others, joined by '-' ("grek-latn", "digit-others-latn").
        // Null takes the locale's; an empty text, none.
        const char* reorder;
        size_t reorderLength;
    } mlocCollatorOptions;

    // A collator: a locale's order and options over the collation data
    // it was made from, which must outlive it. Opaque; make it with
    // mlocInitCollator. It may be copied.
    typedef struct mlocCollator
    {
        uint64_t opaque[128];
    } mlocCollator;

    /// Makes a locale's collator. Its order is the locale's tailoring,
    /// as ICU4C's loader finds it: the -u-co type along the locale's
    /// collation chain, else the chain's default type, else
    /// "standard", else the root's order. The tailoring's settings are
    /// the defaults the -u- keys and the options override. A -u- key's
    /// value that is not one of its values is passed over, as is a kr
    /// value the root cannot order.
    ///
    /// @param collatorOut  Receives the collator; left unchanged on
    ///                     failure.
    /// @param data         The data with the collation root (the
    ///                     collation data file) and, for a locale other
    ///                     than root, the parents (the main data).
    /// @param locale       The locale, canonicalized.
    /// @param options      The options, or null for the locale's.
    /// @return mloc_success; mloc_errorNotFound for data without the
    ///         collation root, or without the parents a locale's chain
    ///         needs when the data has tailorings; mloc_errorInvalid for
    ///         a null pointer, an
    ///         option out of range, or a reorder text that is not a list
    ///         of codes, names a code twice or two codes of one range;
    ///         mloc_errorCapacity for a reordering too large for the
    ///         root's lead bytes.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocInitCollator(mlocCollator* collatorOut,
                                                        const mlocDataSet* data,
                                                        const mlocLocale* locale,
                                                        const mlocCollatorOptions* options);

    /// Compares two UTF-8 texts in a collator's order. A byte that is not
    /// UTF-8 compares as U+FFFD.
    ///
    /// @param collator  A collator mlocInitCollator made.
    /// @param a         The first text; may be null when aLength is 0.
    /// @param aLength   Its length in bytes.
    /// @param b         The second text; may be null when bLength is 0.
    /// @param bLength   Its length in bytes.
    /// @param orderOut  Receives -1 when a sorts first, 1 when b does, 0
    ///                  when they are equal up to the strength.
    /// @return mloc_success; mloc_errorInvalid for a collator the library
    ///         did not make or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocCompare(const mlocCollator* collator, const char* a,
                                                   size_t aLength, const char* b, size_t bLength,
                                                   int* orderOut);

    /// Writes a UTF-8 text's sort key in a collator's order: bytes whose
    /// memcmp order, a shorter key first when it is the other's start, is
    /// the order mlocCompare gives, ICU4C's bytes for the same order and
    /// options (record mloc-0070). The key ends with a 00 byte, counted in
    /// its length. Keys from collators of other options or another
    /// version do not compare.
    ///
    /// @param collator   A collator mlocInitCollator made.
    /// @param text       The text; may be null when length is 0.
    /// @param length     Its length in bytes.
    /// @param key        Receives the key; may be null when capacity is 0.
    /// @param capacity   The room in key, in bytes.
    /// @param lengthOut  Receives the key's length, also when it does not
    ///                   fit.
    /// @return mloc_success; mloc_errorCapacity when the key does not fit
    ///         (the bytes written are then no key); mloc_errorInvalid for
    ///         a collator the library did not make or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetSortKey(const mlocCollator* collator,
                                                      const char* text, size_t length, uint8_t* key,
                                                      size_t capacity, size_t* lengthOut);

    /// Compares two UTF-16 texts in a collator's order, as mlocCompare
    /// compares their UTF-8 forms. A lone surrogate compares as U+FFFD.
    ///
    /// @param collator  A collator mlocInitCollator made.
    /// @param a         The first text; may be null when aLength is 0.
    /// @param aLength   Its length in 16-bit units.
    /// @param b         The second text; may be null when bLength is 0.
    /// @param bLength   Its length in 16-bit units.
    /// @param orderOut  Receives -1 when a sorts first, 1 when b does, 0
    ///                  when they are equal up to the strength.
    /// @return mloc_success; mloc_errorInvalid for a collator the library
    ///         did not make or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocCompareUtf16(const mlocCollator* collator,
                                                        const uint16_t* a, size_t aLength,
                                                        const uint16_t* b, size_t bLength,
                                                        int* orderOut);

    /// Writes a UTF-16 text's sort key: the bytes mlocGetSortKey writes
    /// for its UTF-8 form, a lone surrogate read as U+FFFD.
    ///
    /// @param collator   A collator mlocInitCollator made.
    /// @param text       The text; may be null when length is 0.
    /// @param length     Its length in 16-bit units.
    /// @param key        Receives the key; may be null when capacity is 0.
    /// @param capacity   The room in key, in bytes.
    /// @param lengthOut  Receives the key's length, also when it does not
    ///                   fit.
    /// @return mloc_success; mloc_errorCapacity when the key does not fit
    ///         (lengthOut has the length needed); mloc_errorInvalid for
    ///         a collator the library did not make or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetSortKeyUtf16(const mlocCollator* collator,
                                                           const uint16_t* text, size_t length,
                                                           uint8_t* key, size_t capacity,
                                                           size_t* lengthOut);

    /// Gives the version of a collator's keys, to store beside them: it
    /// changes whenever the keys of some text could change, with the key
    /// format or the collation data the collator reads, and not with the
    /// options, which the caller keeps with the keys.
    ///
    /// @param collator    A collator mlocInitCollator made.
    /// @param versionOut  Receives the version.
    /// @return mloc_success; mloc_errorInvalid for a collator the library
    ///         did not make or a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetCollationVersion(const mlocCollator* collator,
                                                               uint64_t* versionOut);

#ifdef __cplusplus
}
#endif

#endif
