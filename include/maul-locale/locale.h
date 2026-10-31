// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Locale identifiers (UTS #35 Part 1): a parsed locale is a fixed-size
// value in canonical syntax.

#ifndef MAUL_LOCALE_LOCALE_H
#define MAUL_LOCALE_LOCALE_H

#include "maul-locale/base.h"
#include "maul-locale/calendar.h"
#include "maul-locale/data.h"

#ifdef __cplusplus
extern "C"
{
#endif

// The most subtags a locale id may have.
#define MLOC_MAX_LOCALE_SUBTAGS 64

// The most bytes a locale's variants and extensions take (mlocLocale's
// tail).
#define MLOC_MAX_LOCALE_TAIL 112

// The longest text, in bytes, mlocCanonicalizeLocale reads, also after it
// replaces a legacy tag or puts "und-" before one.
#define MLOC_MAX_LOCALE_TEXT 255

// The most supported or desired locales a match takes: a match reports
// their indexes as int32_t.
#define MLOC_MAX_MATCH_LOCALES 2147483647

    // A locale in canonical syntax. The library writes every byte: each
    // field is NUL-padded and uses all its bytes only when its text is
    // that long, so two locales compare equal with memcmp. Read the
    // fields; make a locale with mlocParseLocale.
    //
    // Every function that reads a locale refuses with mloc_errorInvalid
    // one whose fields are not NUL-padded text of the syntax below (the
    // tail's subtags 1 to 8 lowercase letters and digits), as it refuses
    // a null pointer. It checks syntax only: a locale filled by hand that
    // is well formed but not canonical (an alias, variants or extensions
    // out of order) is accepted, and what it gives is then undefined.
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
        char tail[MLOC_MAX_LOCALE_TAIL];
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
    ///         mloc_errorLimit when the variants and extensions pass
    ///         MLOC_MAX_LOCALE_TAIL bytes or the tag has more than
    ///         MLOC_MAX_LOCALE_SUBTAGS subtags;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocTextResult mlocParseLocale(const char* text, size_t length,
                                                           mlocLocale* localeOut);

    /// Writes a locale as its BCP 47 tag in canonical syntax, such as
    /// "sr-Latn-RS" or "de-DE-u-co-phonebk". It reports no parts.
    ///
    /// @param locale  A locale from mlocParseLocale.
    /// @param textOut Where the tag goes; left as it was on any failure but
    ///                mloc_errorCapacity.
    /// @return mloc_success; mloc_errorCapacity with the length needed;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocFormatLocale(const mlocLocale* locale,
                                                        mlocOutput* textOut);

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
        // An extension: a t extension's language, a u or t value, or a
        // subdivision.
        mloc_changedExtensions = 64,
    };

    /// Parses a BCP 47 tag or CLDR locale id and canonicalizes it as UTS
    /// #35 Annex C does: a legacy tag replaced whole, an extlang removed,
    /// "und-" put before a tag that begins with "x" or a script, then
    /// CLDR's language, script, region and variant aliases applied until
    /// none matches ("iw" gives "he", "sh" gives "sr-Latn", "SU" gives the
    /// region of the language's likely one), then the extensions: a t
    /// extension's language the same way, u and t values by the bcp47
    /// data's aliases ("ms-imperial" gives "ms-uksystem"), and the
    /// subdivisions of "sd" and "rg" by theirs ("rg-fi01" gives
    /// "rg-axzzzz"). The syntax is made canonical as mlocParseLocale does.
    ///
    /// @param data        The data with the aliases and likely subtags.
    /// @param text        The tag.
    /// @param length      Its length in bytes, at most
    ///                    MLOC_MAX_LOCALE_TEXT.
    /// @param localeOut   Receives the locale; left unchanged on failure.
    /// @param changesOut  Receives what was replaced; may be null; left
    ///                    unchanged on failure.
    /// @return status mloc_success; mloc_errorSyntax with an offset into the
    ///         text as mlocParseLocale; mloc_errorLimit for a text longer
    ///         than MLOC_MAX_LOCALE_TEXT bytes, before or after a legacy tag
    ///         is replaced, or a result past the bounds of mlocParseLocale;
    ///         mloc_errorNotFound
    ///         for data without the aliases (or, for a tag with a u or t
    ///         extension, the extension aliases); mloc_errorFormat for aliases that
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
    typedef int32_t mlocFavor;
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
    /// @param localeOut  Receives the result, a locale apart from locale;
    ///                   left unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound when the data has no likely
    ///         subtags for the locale or has no likely subtags table;
    ///         mloc_errorInvalid for a null pointer or a localeOut that is
    ///         or overlaps locale.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMaximizeLocale(const mlocDataSet* data,
                                                          const mlocLocale* locale,
                                                          mlocLocale* localeOut);

    /// A locale's direction, CLDR's character order: right to left when
    /// its script, or its likely script when it names none (or "Zzzz"),
    /// is written right to left (Arabic, Hebrew, Thaana, N'Ko, Adlam and
    /// the other scripts CLDR's script metadata marks so), else left to
    /// right. This agrees with every CLDR locale's own character order
    /// (Mongolian's, top to bottom, is left to right in a horizontal
    /// line, its letters being of bidi class L).
    ///
    /// @param data          The data to read the likely subtags from.
    /// @param locale        The locale.
    /// @param directionOut  Receives mloc_directionLeftToRight or
    ///                      mloc_directionRightToLeft; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_errorNotFound when the locale names no
    ///         script and the data has no likely script for it;
    ///         mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetLocaleDirection(const mlocDataSet* data,
                                                              const mlocLocale* locale,
                                                              mlocDirection* directionOut);

    /// Removes likely subtags (UTS #35): the shortest of the language
    /// alone, then with its region or script (in the order favor gives),
    /// that adds back to the same locale; "zh-Hant-TW" gives "zh-TW", or
    /// "zh-Hant" when the script is favored. The variants and extensions
    /// are kept.
    ///
    /// @param data       The data to read the likely subtags from.
    /// @param locale     The locale.
    /// @param favor      mloc_favorRegion or mloc_favorScript.
    /// @param localeOut  Receives the result, a locale apart from locale;
    ///                   left unchanged on failure.
    /// @return mloc_success; mloc_errorNotFound as mlocMaximizeLocale;
    ///         mloc_errorInvalid for a null pointer, a localeOut that is or
    ///         overlaps locale, or another favor.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocMinimizeLocale(const mlocDataSet* data,
                                                          const mlocLocale* locale, mlocFavor favor,
                                                          mlocLocale* localeOut);

    // The parts of CLDR's data whose locales inherit differently (UTS #35,
    // Parent Locales).
    typedef int32_t mlocComponent;
    enum
    {
        // Everything not listed below: names, numbers, dates and the rest.
        mloc_componentMain = 0,
        mloc_componentCollation = 1,
        mloc_componentPlurals = 2,
        mloc_componentSegmentation = 3,
        mloc_componentGrammar = 4,
    };

    /// Returns the next locale of a locale's fallback chain in a component: the
    /// locale is first put in key form, with the script likely subtags give it
    /// and left out when it is its language's own ("zh-TW" is zh-Hant-TW,
    /// "sr-Cyrl-BA" is sr-BA); then its next key is CLDR's explicit parent
    /// ("en-AU" gives "en-001", "es-MX" "es-419"), else the last variant, the
    /// region or the script dropped, where in the main component a script gives
    /// root ("zh-Hant" gives "und", never "zh"). The extensions are kept. Root
    /// is "und".
    ///
    /// @param data       The data with the parents and likely subtags.
    /// @param component  The component whose parents apply.
    /// @param locale     The locale.
    /// @param parentOut  Receives the parent; left unchanged on failure. May be locale itself.
    /// @return mloc_success; mloc_errorNotFound for root, which has no
    ///         parent, or data without the parents table;
    ///         mloc_errorLimit when the extensions after the parent's
    ///         variants pass MLOC_MAX_LOCALE_TAIL bytes; mloc_errorInvalid for a null pointer or
    ///         another component.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetParentLocale(const mlocDataSet* data,
                                                           mlocComponent component,
                                                           const mlocLocale* locale,
                                                           mlocLocale* parentOut);

    // A supported locale prepared for matching by mlocPrepareMatch: its
    // maximized language, script and region and its index. Opaque.
    typedef struct mlocMatchEntry
    {
        uint64_t opaque[3];
    } mlocMatchEntry;

    /// Prepares supported locales for mlocMatchLocales: maximizes each as
    /// matching does and orders them as they are tried, those equivalent to the
    /// default locale first, then CLDR's paradigm locales (en, en-GB, es,
    /// es-419, pt-BR, pt-PT), then the rest, each group in the order given.
    /// Give canonicalized locales (mlocCanonicalizeLocale): matching replaces
    /// no aliases.
    ///
    /// @param data           The data with the likely subtags and the
    ///                       matching table.
    /// @param supported      The supported locales.
    /// @param count          Their number, at most MLOC_MAX_MATCH_LOCALES.
    /// @param defaultLocale  The locale whose equivalents are tried first,
    ///                       as ICU's default locale: usually supported[0];
    ///                       null for none.
    /// @param entriesOut     Receives count entries; left unchanged on
    ///                       failure.
    /// @return mloc_success; mloc_errorNotFound for data without the likely
    ///         subtags or the matching table; mloc_errorLimit for a count
    ///         past MLOC_MAX_MATCH_LOCALES; mloc_errorInvalid for a null
    ///         pointer with a count.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocPrepareMatch(const mlocDataSet* data,
                                                        const mlocLocale* supported, size_t count,
                                                        const mlocLocale* defaultLocale,
                                                        mlocMatchEntry* entriesOut);

    // How mlocMatchLocales weighs distances, as bits.
    typedef uint32_t mlocMatchFlags;
    enum
    {
        // The language distance counts a quarter, so that a script
        // difference weighs more than a language one: "pl" then matches
        // "fr" (one script), and "sr" matches "ru" rather than "hr".
        mloc_matchFavorScript = 1,
        // A later desired locale is not demoted: each is weighed alike.
        mloc_matchNoDemotion = 2,
        // A pair matches only when it also matches the other way, so the
        // one-way rules (Afrikaans to English, Swiss German to German) do
        // not apply.
        mloc_matchBothWays = 4,
    };

    typedef struct mlocMatchOptions
    {
        mlocMatchFlags flags;
        // The distance a match stays below, 1 to 100; 0 for CLDR's default
        // script distance (50), so that different scripts do not match.
        uint32_t threshold;
    } mlocMatchOptions;

    // The outcome of mlocMatchLocales.
    typedef struct mlocMatch
    {
        // The index of the best supported locale, or -1 when none is near
        // enough, for the default locale.
        int32_t supported;
        // The index of the desired locale it matched, or -1.
        int32_t desired;
    } mlocMatch;

    /// Finds the supported locale that best matches the desired locales in
    /// their order of preference, as ICU's LocaleMatcher does. Each desired
    /// locale is maximized and each supported one gets the distance CLDR's
    /// rules give its language, script and region (regions by groups: "es-MX"
    /// is nearer "es-419" than "es", "en-IN" nearer "en-GB" than "en-US"); the
    /// nearest wins, a tie going to the one tried first unless a later one has
    /// its language's likely script or region. Each later desired locale is
    /// demoted by the distance of two English regions, so that it wins only
    /// with a closer match.
    ///
    /// @param data          The data mlocPrepareMatch was given.
    /// @param supported     The supported locales mlocPrepareMatch was
    ///                      given; private use tags are compared with them.
    /// @param entries       What mlocPrepareMatch made of them.
    /// @param count         Their number.
    /// @param desired       The desired locales, the most wanted first,
    ///                      canonicalized.
    /// @param desiredCount  Their number, at most MLOC_MAX_MATCH_LOCALES.
    /// @param options       The options; null for none.
    /// @param matchOut      Receives the outcome; left unchanged on failure.
    /// @return mloc_success, also when nothing matches; mloc_errorNotFound
    ///         as mlocPrepareMatch; mloc_errorLimit for a count past
    ///         MLOC_MAX_MATCH_LOCALES; mloc_errorInvalid for a null pointer
    ///         with a count, another flag, a threshold
    ///         above 100, or an entry the search reads that names no
    ///         locale of the count (entries prepared for another list).
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult
    mlocMatchLocales(const mlocDataSet* data, const mlocLocale* supported,
                     const mlocMatchEntry* entries, size_t count, const mlocLocale* desired,
                     size_t desiredCount, const mlocMatchOptions* options, mlocMatch* matchOut);

    /// Makes the locale to use from a match, as ICU's resolved locale: the
    /// supported locale with the desired one's region, its variants (which
    /// replace the supported ones) and its extensions (each replacing the
    /// supported one of its letter): "ar-SA-u-nu-latn" matched with "ar-EG"
    /// gives "ar-SA-u-nu-latn".
    ///
    /// @param desired    The desired locale of the match.
    /// @param supported  The supported locale of the match.
    /// @param localeOut  Receives the locale, a locale apart from both
    ///                   inputs; left unchanged on failure.
    /// @return mloc_success; mloc_errorLimit when the variants and
    ///         extensions pass MLOC_MAX_LOCALE_TAIL bytes; mloc_errorInvalid for a null
    ///         pointer or a localeOut that is or overlaps either input.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocCombineMatch(const mlocLocale* desired,
                                                        const mlocLocale* supported,
                                                        mlocLocale* localeOut);

    // An hour format of CLDR's time data: the four hour cycles (the
    // pattern letters K, h, H and k), then 12 hours with a day period (hb,
    // "noon") or a flexible one (hB, "in the evening").
    typedef int32_t mlocHourFormat;
    enum
    {
        mloc_hourFormatH11 = 0,
        mloc_hourFormatH12 = 1,
        mloc_hourFormatH23 = 2,
        mloc_hourFormatH24 = 3,
        mloc_hourFormatH12DayPeriod = 4,
        mloc_hourFormatH12FlexibleDayPeriod = 5,
    };

    typedef int32_t mlocMeasurementSystem;
    enum
    {
        mloc_measurementMetric = 0,
        // Feet and pints of 16 ounces.
        mloc_measurementUs = 1,
        // Metric with feet and pints of 20 ounces.
        mloc_measurementUk = 2,
    };

    typedef int32_t mlocPaperSize;
    enum
    {
        mloc_paperA4 = 0,
        mloc_paperUsLetter = 1,
    };

    // A locale's regional preferences.
    typedef struct mlocLocalePreferences
    {
        // The region they are from, NUL-padded: "001" when the locale
        // gives none and has no likely one.
        char region[4];
        // One of the four hour cycles.
        mlocHourFormat hourCycle;
        // The formats the region accepts, the most usual first,
        // allowedHourCount of them.
        mlocHourFormat allowedHours[6];
        mlocMeasurementSystem measurementSystem;
        mlocPaperSize paperSize;
        // The region's first preference, as CLDR orders them.
        mlocCalendar calendar;
        // Counts and days, not kinds: each fits a byte. Days are numbered
        // from Monday, 1, to Sunday, 7.
        uint8_t allowedHourCount;
        uint8_t firstDay;
        // The days of a year's first week that must fall in the year.
        uint8_t minimalDays;
        uint8_t weekendStart;
        uint8_t weekendEnd;
    } mlocLocalePreferences;

    /// Returns a locale's regional preferences (UTS #35), the region's
    /// values from CLDR's supplemental data. The region is the -u-rg
    /// key's ("en-GB-u-rg-uszzzz" takes the US's), else the locale's
    /// region, else the -u-sd key's, else the likely one ("en" takes the
    /// US's), else 001; a region without data takes 001's. Keys override:
    /// -u-hc the hour cycle (c12 and c24 resolve to the cycle the allowed
    /// formats list first), -u-fw the first day, -u-ms the measurement
    /// system, -u-ca the calendar; -u-ca-iso8601 also begins weeks on
    /// Monday with four days in the first unless -u-fw or -u-rg give the
    /// first day. The hour data is
    /// looked up for the language in the region first (ca-ES has its own).
    /// A key with a value it does not take is left out.
    ///
    /// @param data          The data with the preferences and likely
    ///                      subtags.
    /// @param locale        The locale, canonicalized.
    /// @param preferencesOut Receives the preferences; left unchanged on
    ///                      failure.
    /// @return mloc_success; mloc_errorNotFound for data without the
    ///         preferences; mloc_errorInvalid for a null pointer.
    /// @par Thread safety
    /// Safe from any thread.
    MLOC_NODISCARD MLOC_API mlocResult mlocGetLocalePreferences(
        const mlocDataSet* data, const mlocLocale* locale, mlocLocalePreferences* preferencesOut);

#ifdef __cplusplus
}
#endif

#endif
