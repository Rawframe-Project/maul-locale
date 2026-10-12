// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit kinds the library reads, each with its part's validator,
// sorted by kind. A part adds its kinds here as it is built.

#include "currency_region.h"
#include "data_view.h"
#include "kinds.h"
#include "locale_canonical.h"
#include "locale_distance.h"
#include "locale_fallback.h"
#include "locale_likely.h"
#include "locale_prefs.h"
#include "number_data.h"
#include "plural.h"
#include "unit_data.h"

static const DataKind s_kinds[] = {
    {kindLocaleLikely, mlocValidLikely},
    {kindLocaleAliases, mlocValidAliases},
    {kindLocaleExtensionAliases, mlocValidExtensionAliases},
    {kindLocaleParents, mlocValidParents},
    {kindLocaleMatch, mlocValidMatch},
    {kindLocalePreferences, mlocValidPreferences},
    {kindPlurals, mlocValidPlurals},
    {kindPluralRanges, mlocValidPluralRanges},
    {kindNumberSystems, mlocValidNumberSystems},
    {kindNumberSymbols, mlocValidNumberSymbols},
    {kindNumberPatterns, mlocValidNumberPatterns},
    {kindNumbers, mlocValidNumbers},
    {kindNumberCompactPatterns, mlocValidNumberCompactPatterns},
    {kindNumberCompact, mlocValidNumberCompact},
    {kindCurrencyFractions, mlocValidCurrencyFractions},
    {kindCurrencyPatterns, mlocValidCurrencyPatterns},
    {kindCurrencySymbols, mlocValidCurrencySymbols},
    {kindCurrencyNames, mlocValidCurrencyNames},
    {kindCurrencyRegions, mlocValidCurrencyRegions},
    {kindUnits, mlocValidUnits},
};

const DataKind* const mlocDataKinds = s_kinds;
const uint32_t mlocDataKindCount = sizeof(s_kinds) / sizeof(s_kinds[0]);
