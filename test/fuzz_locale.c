// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// libFuzzer target for locale ids. A refusal points inside the text; a
// parsed locale writes a tag that parses back to the same locale and
// writes the same tag again. Canonicalizing on the compiled-in data
// refuses within the text too, and a canonical tag canonicalizes to
// itself, its fallback chain in every component ends at root, it
// matches itself first, its match among a few others is one of them and
// combines with it, it has regional preferences in range, and plural
// rules of both types whose category for 21 is one they name.

#include "maul-locale/locale.h"
#include "maul-locale/plural.h"

#include <stdlib.h>
#include <string.h>

static void Require(bool condition)
{
    if (!condition)
    {
        abort();
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

static void Match(const mlocDataSet* set, const mlocLocale* locale)
{
    static const char* const s_others[] = {"en", "en-GB", "zh-Hant", "und", "und-x-bork", "en-XA"};
    mlocLocale supported[7] = {*locale};
    for (size_t i = 0; i < 6; i++)
    {
        Require(mlocCanonicalizeLocale(set, s_others[i], strlen(s_others[i]), &supported[i + 1],
                                       nullptr)
                    .status == mloc_success);
    }
    mlocMatchEntry entries[7];
    mlocMatch match;
    mlocMatchOptions options = {.flags = mloc_matchBothWays | mloc_matchFavorScript};
    Require(mlocPrepareMatch(set, supported, 1, locale, entries) == mloc_success);
    Require(mlocMatchLocales(set, supported, entries, 1, locale, 1, &options, &match) ==
                mloc_success &&
            match.supported == 0 && match.desired == 0);
    Require(mlocPrepareMatch(set, supported + 1, 6, nullptr, entries) == mloc_success);
    Require(mlocMatchLocales(set, supported + 1, entries, 6, locale, 1, nullptr, &match) ==
            mloc_success);
    Require(match.supported >= -1 && match.supported < 6 &&
            match.desired == (match.supported >= 0 ? 0 : -1));
    if (match.supported >= 0)
    {
        mlocLocale combined;
        mlocResult result = mlocCombineMatch(locale, &supported[1 + match.supported], &combined);
        Require(result == mloc_success || result == mloc_errorCapacity);
    }
}

// Canonicalizes the input; a result written and canonicalized again
// stays the same.
static void Canonicalize(const uint8_t* data, size_t size)
{
    mlocData builtin;
    mlocOpenBuiltinData(&builtin);
    mlocDataSet set = {&builtin, 1};
    mlocLocale locale;
    mlocTextResult result = mlocCanonicalizeLocale(&set, (const char*)data, size, &locale, nullptr);
    if (result.status != mloc_success)
    {
        Require(result.status == mloc_errorSyntax || result.status == mloc_errorCapacity);
        Require(result.offset <= size);
        return;
    }
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    Require(mlocFormatLocale(&locale, &output) == mloc_success);
    mlocLocale again;
    mlocLocaleChanges changes = 0;
    Require(mlocCanonicalizeLocale(&set, tag, output.length, &again, &changes).status ==
            mloc_success);
    Require(memcmp(&locale, &again, sizeof(locale)) == 0 && changes == 0);
    // Every chain ends at root: one step a variant (112 bytes hold at
    // most 22), then explicit parents, the region, the script and root.
    for (mlocComponent component = 0; component <= mloc_componentGrammar; component++)
    {
        mlocLocale step = locale;
        int depth = 0;
        mlocResult next = mloc_success;
        while (depth++ < 64 && next == mloc_success)
        {
            mlocLocale parent;
            next = mlocGetParentLocale(&set, component, &step, &parent);
            step = next == mloc_success ? parent : step;
        }
        Require(next == mloc_errorNotFound || next == mloc_errorCapacity);
    }
    Match(&set, &locale);
    mlocLocalePreferences preferences;
    Require(mlocGetLocalePreferences(&set, &locale, &preferences) == mloc_success &&
            preferences.hourCycle <= mloc_hourFormatH24 && preferences.firstDay >= 1 &&
            preferences.firstDay <= 7 && preferences.allowedHourCount >= 1 &&
            preferences.allowedHourCount <= 6);
    // Every locale has plural rules of both types (root's at worst), and
    // a number takes one of the categories they name.
    for (int type = mloc_pluralCardinal; type <= mloc_pluralOrdinal; type++)
    {
        mlocPluralRules rules;
        mlocDecimal number;
        mlocPluralCategory category;
        uint32_t categories = 0;
        Require(mlocInitPluralRules(&rules, &set, &locale, (mlocPluralType)type) >= 0);
        Require(mlocMakeDecimal(21, &number) == mloc_success &&
                mlocSelectPlural(&rules, &number, &category) == mloc_success &&
                mlocGetPluralCategories(&rules, &categories) == mloc_success &&
                (categories >> category & 1u) != 0);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    Canonicalize(data, size);
    mlocLocale locale;
    mlocTextResult result = mlocParseLocale((const char*)data, size, &locale);
    if (result.status != mloc_success)
    {
        Require(result.status == mloc_errorSyntax || result.status == mloc_errorCapacity);
        Require(result.offset <= size);
        return 0;
    }
    char tag[256];
    mlocOutput output = {.buffer = tag, .capacity = sizeof(tag)};
    Require(mlocFormatLocale(&locale, &output) == mloc_success);
    mlocLocale again;
    Require(mlocParseLocale(tag, output.length, &again).status == mloc_success);
    Require(memcmp(&locale, &again, sizeof(locale)) == 0);
    char second[256];
    mlocOutput secondOutput = {.buffer = second, .capacity = sizeof(second)};
    Require(mlocFormatLocale(&again, &secondOutput) == mloc_success);
    Require(secondOutput.length == output.length && memcmp(tag, second, output.length) == 0);
    return 0;
}
