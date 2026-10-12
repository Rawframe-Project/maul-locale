// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale identifiers (UTS #35 Part 1): a parsed locale is a fixed-size
// value in canonical syntax (record mloc-0015).

#ifndef MAUL_LOCALE_LOCALE_H
#define MAUL_LOCALE_LOCALE_H

#include "maul-locale/base.h"
#include "maul-locale/data.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // A locale in canonical syntax. The library writes every byte: each
    // field is NUL-padded and uses all its bytes only when its text is
    // that long, so two locales compare equal with memcmp. Read the
    // fields; make a locale with mlocParseLocale.
    typedef struct mlocLocale
    {
        // Lowercase, 2, 3 or 5 to 8 letters; "und" when the tag gave
        // none ("und" or "root").
        char language[8];
        // Title case, 4 letters, or empty.
        char script[4];
        // Uppercase letters (2) or digits (3), or empty.
        char region[4];
        // The variants, then the extensions, each subtag lowercase,
        // separated by "-", without a leading one: "fonipa-u-ca-buddhist".
        char tail[112];
    } mlocLocale;

    /// Parses a locale id strictly and puts it in canonical syntax: case,
    /// variants and attributes in order, extensions by their singleton
    /// with private use last, keys in order, a "u" value "true" removed.
    /// Aliases are not replaced.
    ///
    /// @param text       The id, with "-" or "_" between subtags.
    /// @param length     Its length in bytes.
    /// @param localeOut  Receives the locale; left unchanged on failure.
    /// @return status mloc_success with offset the length;
    ///         mloc_errorSyntax with the offset of the subtag at fault for
    ///         text that is no locale id, a tag that begins with a script,
    ///         and duplicate variants, extensions, keys or attributes;
    ///         mloc_errorCapacity when the variants and extensions pass
    ///         112 bytes or the tag has more than 64 subtags;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocTextResult mlocParseLocale(const char* text, size_t length,
                                                           mlocLocale* localeOut);

    /// Writes a locale as its BCP 47 tag in canonical syntax, such as
    /// "sr-Latn-RS" or "de-DE-u-co-phonebk". It reports no parts.
    ///
    /// @param locale  A locale from mlocParseLocale.
    /// @param output  Where the tag goes.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatLocale(const mlocLocale* locale,
                                                        mlocOutput* output);

    // What mlocCanonicalizeLocale replaced, as bits.
    typedef uint32_t mlocLocaleChanges;
    enum
    {
        // A whole legacy tag, such as "i-klingon".
        mloc_changedLegacy = 1,
        // An extlang removed: "zh-yue" became "yue".
        mloc_changedExtlang = 2,
        mloc_changedLanguage = 4,
        mloc_changedScript = 8,
        mloc_changedRegion = 16,
        mloc_changedVariants = 32,
    };

    /// Parses a BCP 47 tag or CLDR locale id and canonicalizes it as UTS
    /// #35 Annex C does: a legacy tag replaced whole, an extlang removed,
    /// "und-" put before a tag that begins with "x" or a script, then
    /// CLDR's language, script, region and variant aliases applied until
    /// none matches ("iw" gives "he", "sh" gives "sr-Latn", "SU" gives the
    /// region of the language's likely one). The syntax is made canonical
    /// as mlocParseLocale does.
    ///
    /// @param data        The data with the aliases and likely subtags.
    /// @param text        The tag.
    /// @param length      Its length in bytes, below 256.
    /// @param localeOut   Receives the locale; left unchanged on failure.
    /// @param changesOut  Receives what was replaced; may be null.
    /// @return status mloc_success; mloc_errorSyntax with an offset into the
    ///         text as mlocParseLocale; mloc_errorCapacity for a text of 256
    ///         bytes or more, or a result that does not fit; mloc_errorNotFound
    ///         for data without the aliases; mloc_errorData for aliases that
    ///         never stop replacing, which only a crafted data file has;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocTextResult mlocCanonicalizeLocale(const mlocDataSet* data,
                                                                  const char* text, size_t length,
                                                                  mlocLocale* localeOut,
                                                                  mlocLocaleChanges* changesOut);

    // Which shorter form mlocMinimizeLocale keeps when a language's
    // likely script and region could each be dropped: UTS #35's default
    // keeps the region ("zh-TW"), the other the script ("zh-Hant").
    typedef uint8_t mlocFavor;
    enum
    {
        mloc_favorRegion = 0,
        mloc_favorScript = 1,
    };

    /// Adds likely subtags (UTS #35): "zh-TW" gives "zh-Hant-TW", "und"
    /// gives "en-Latn-US". A script "Zzzz" and a region "ZZ" are dropped
    /// first; the variants and extensions are kept. Aliases are not
    /// replaced, so give a canonicalized locale.
    ///
    /// @param data       The data to read the likely subtags from.
    /// @param locale     The locale.
    /// @param localeOut  Receives the result; may be locale itself; left
    ///                   unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound when the data has no likely
    ///         subtags for the locale or has no likely subtags table;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMaximizeLocale(const mlocDataSet* data,
                                                          const mlocLocale* locale,
                                                          mlocLocale* localeOut);

    /// Removes likely subtags (UTS #35): the shortest of the language
    /// alone, then with its region or script (in the order favor gives),
    /// that adds back to the same locale; "zh-Hant-TW" gives "zh-TW", or
    /// "zh-Hant" when the script is favored. The variants and extensions
    /// are kept.
    ///
    /// @param data       The data to read the likely subtags from.
    /// @param locale     The locale.
    /// @param favor      mloc_favorRegion or mloc_favorScript.
    /// @param localeOut  Receives the result; may be locale itself; left
    ///                   unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound as mlocMaximizeLocale;
    ///         mloc_errorInvalid for a null pointer or another favor.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMinimizeLocale(const mlocDataSet* data,
                                                          const mlocLocale* locale, mlocFavor favor,
                                                          mlocLocale* localeOut);

#ifdef __cplusplus
}
#endif

#endif
