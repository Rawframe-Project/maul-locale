// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale identifiers (UTS #35 Part 1): a parsed locale is a fixed-size
// value in canonical syntax (record mloc-0015).

#ifndef MAUL_LOCALE_LOCALE_H
#define MAUL_LOCALE_LOCALE_H

#include "maul-locale/base.h"

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

#ifdef __cplusplus
}
#endif

#endif
