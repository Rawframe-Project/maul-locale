// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Display names (UTS #35 Part 2): a locale's name for a language, script,
// region, variant, key, key's value or measurement system, as ICU4C's
// LocaleDisplayNames and ECMA-402's Intl.DisplayNames give them, for a
// subdivision, and for a whole locale. The names are in a data file of
// their own, names.mloc, and the subdivision names in another,
// subdivisions.mloc, which a program adds to its data set.

#ifndef MAUL_LOCALE_NAMES_H
#define MAUL_LOCALE_NAMES_H

#include "maul-locale/locale.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // What a code names, and the code's syntax (any case; BCP 47's ids).
    typedef enum mlocNameType
    {
        // A language id CLDR names whole: "en", "en-GB", "zh-Hant",
        // "ca-ES-valencia".
        mloc_nameLanguage = 0,
        // A script: "Latn".
        mloc_nameScript = 1,
        // A region: "US", "419".
        mloc_nameRegion = 2,
        // A variant: "valencia", "1901".
        mloc_nameVariant = 3,
        // A -u or -t key ("ca", "m0"), or the extensions "t" and "x".
        mloc_nameKey = 4,
        // A key and its value: "ca-gregory", "nu-latn".
        mloc_nameKeyValue = 5,
        // A measurement system: "metric", "uksystem", "ussystem".
        mloc_nameMeasurementSystem = 6,
        // A subdivision, as -u-sd writes it: "gbsct", "usca" (names in
        // subdivisions.mloc; ICU4C has none).
        mloc_nameSubdivision = 7,
    } mlocNameType;

    // Which of a name's forms: CLDR's name, or one of its alternates,
    // which stands in for the name where a locale's chain has it.
    typedef enum mlocNameForm
    {
        mloc_nameFormStandard = 0,
        // "US" for the United States, "UK English".
        mloc_nameFormShort = 1,
        // "Mandarin Chinese".
        mloc_nameFormLong = 2,
        // Another usual name: "Côte d’Ivoire" for "Ivory Coast".
        mloc_nameFormVariant = 3,
        // The form a sorted menu lists.
        mloc_nameFormMenu = 4,
        // A language's core name in a menu ("Kurdish"), a value's name
        // beside its key's ("Gregorian").
        mloc_nameFormCore = 5,
        // What follows a language's core name in a menu ("Kurmanji").
        mloc_nameFormExtension = 6,
        // A script's name standing alone ("Simplified Han").
        mloc_nameFormStandAlone = 7,
    } mlocNameForm;

    // What a code without a name gives: the code itself (a value's code
    // for a key and its value), or nothing.
    typedef enum mlocNameFallback
    {
        mloc_nameFallbackCode = 0,
        mloc_nameFallbackNone = 1,
    } mlocNameFallback;

    // The options of a name; the zero value is the default (the standard
    // form, the code for a missing name, as the data writes it).
    typedef struct mlocNameOptions
    {
        mlocNameForm form;
        mlocNameFallback fallback;
        // The first letter titlecased, as ICU does: at the beginning of
        // a sentence always, in a list or menu or alone as the locale's
        // context transforms for the type say (a measurement system and
        // a subdivision have none).
        mlocCapitalization capitalization;
    } mlocNameOptions;

    /// Writes a locale's name for a code: the form's along the locale's
    /// chain, else the standard name along the chain, as ICU4C's
    /// LocaleDisplayNames looks it up.
    ///
    /// @param data     The data with the names (names.mloc, or
    ///                 subdivisions.mloc for a subdivision) and the
    ///                 parents.
    /// @param locale   The locale, canonicalized.
    /// @param type     What the code names.
    /// @param code     The code, in any case.
    /// @param length   Its length in bytes.
    /// @param options  The options, or null for the defaults.
    /// @param textOut  Where the name goes; no parts.
    /// @return mloc_success; mloc_fellBack when the code, in canonical
    ///         case, is written for a name the locale's chain lacks;
    ///         mloc_errorNotFound for a missing name with
    ///         mloc_nameFallbackNone, or data without the names;
    ///         mloc_errorSyntax for a code not of the type's syntax;
    ///         mloc_errorCapacity when the name does not fit (textOut's
    ///         length then the size needed); mloc_errorInvalid for a null
    ///         pointer, a part function, or a type or options out of
    ///         range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetName(const mlocDataSet* data,
                                                   const mlocLocale* locale, mlocNameType type,
                                                   const char* code, size_t length,
                                                   const mlocNameOptions* options,
                                                   mlocOutput* textOut);

    // How a locale's language and region are named together: by the
    // language's own name for the pair where the locale has one ("British
    // English", "Flemish"), or always the language with its region in
    // the pattern ("English (United Kingdom)").
    typedef enum mlocLanguageDisplay
    {
        mloc_languageDisplayDialect = 0,
        mloc_languageDisplayStandard = 1,
    } mlocLanguageDisplay;

    // The options of a locale's name; the zero value is the default
    // (dialect names, the standard form, as the data writes it).
    typedef struct mlocLocaleNameOptions
    {
        mlocLanguageDisplay languageDisplay;
        // The form of the language, script, region and variant names.
        mlocNameForm form;
        // As for mlocGetName, by the context transforms of language names.
        mlocCapitalization capitalization;
    } mlocLocaleNameOptions;

    /// Writes a locale's name in another locale by UTS #35's Locale
    /// Display Name Algorithm, as CLDR's own tools build it: the
    /// language's name (with dialect names the longest match of its
    /// script, region and variants), then by the locale's pattern the
    /// names of the parts it left out ("Latin", "Canada"), of the u keys
    /// ("Buddhist Calendar", "Currency: €"), of the t extension
    /// ("Transform: Russian, Cyrillic") and its fields, and the other
    /// extensions and private use as written ("x: abc"), joined by the
    /// locale's separator, brackets inside a name nested ("Myanmar
    /// [Burma]"). The sd and rg values are named as subdivisions ("Region
    /// Subdivision: Scotland"). A part without a name stands as written;
    /// time zones stand as written until their names are in the data.
    ///
    /// @param data           The data with the names (names.mloc), the
    ///                       parents and, for -u-cu, the currency symbols;
    ///                       for -u-sd and -u-rg, subdivisions.mloc too.
    /// @param displayLocale  The locale the name is in, canonicalized.
    /// @param locale         The locale named, canonicalized.
    /// @param options        The options, or null for the defaults.
    /// @param textOut        Where the name goes; no
    ///                       parts.
    /// @return mloc_success; mloc_fellBack when the language has no name
    ///         and its code stands; mloc_errorNotFound for data without the
    ///         names; mloc_errorCapacity when the name does not fit
    ///         (textOut's length then the size needed); mloc_errorInvalid
    ///         for a null pointer, a part function, or options out of
    ///         range.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetLocaleName(const mlocDataSet* data,
                                                         const mlocLocale* displayLocale,
                                                         const mlocLocale* locale,
                                                         const mlocLocaleNameOptions* options,
                                                         mlocOutput* textOut);

#ifdef __cplusplus
}
#endif

#endif
