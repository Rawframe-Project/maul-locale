// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit kinds the library reads, each with its part's validator,
// sorted by kind. A part adds its kinds here as it is built.

#include "currency_region.h"
#include "data_view.h"
#include "kinds.h"
#include "list_data.h"
#include "locale_canonical.h"
#include "locale_context.h"
#include "locale_distance.h"
#include "locale_fallback.h"
#include "locale_likely.h"
#include "locale_prefs.h"
#include "names_data.h"
#include "number_data.h"
#include "number_parse.h"
#include "plural.h"
#include "spellout_data.h"
#include "unit_data.h"
#include "unit_names.h"

static const DataKind s_kinds[] = {
    {kindLocaleLikely, mlocValidLikely},
    {kindLocaleAliases, mlocValidAliases},
    {kindLocaleExtensionAliases, mlocValidExtensionAliases},
    {kindLocaleParents, mlocValidParents},
    {kindLocaleMatch, mlocValidMatch},
    {kindLocalePreferences, mlocValidPreferences},
    {kindLocaleContext, mlocValidLocaleContext},
    {kindPlurals, mlocValidPlurals},
    {kindPluralRanges, mlocValidPluralRanges},
    {kindNumberSystems, mlocValidNumberSystems},
    {kindNumberSymbols, mlocValidNumberSymbols},
    {kindNumberPatterns, mlocValidNumberPatterns},
    {kindNumbers, mlocValidNumbers},
    {kindNumberCompactPatterns, mlocValidNumberCompactPatterns},
    {kindNumberCompact, mlocValidNumberCompact},
    {kindParseSets, mlocValidParseSets},
    {kindCurrencyFractions, mlocValidCurrencyFractions},
    {kindCurrencyPatterns, mlocValidCurrencyPatterns},
    {kindCurrencySymbols, mlocValidCurrencySymbols},
    {kindCurrencyNames, mlocValidCurrencyNames},
    {kindCurrencyRegions, mlocValidCurrencyRegions},
    {kindUnits, mlocValidUnits},
    {kindUnitPreferences, mlocValidUnitPreferences},
    {kindUnitIds, mlocValidUnitIds},
    {kindUnitNamesLong, mlocValidUnitNames},
    {kindUnitNamesShort, mlocValidUnitNames},
    {kindUnitNamesNarrow, mlocValidUnitNames},
    {kindUnitNameExtras, mlocValidUnitNameExtras},
    {kindUnitDerivations, mlocValidUnitDerivations},
    {kindUnitPatterns, mlocValidUnitPatterns},
    {kindUnitCases, mlocValidUnitCases},
    {kindUnitCasesLowered, mlocValidUnitCases},
    {kindListPatterns, mlocValidListPatterns},
    {kindLists, mlocValidLists},
    {kindSpelloutRules, mlocValidSpelloutRules},
    {kindSpelloutOrdinals, mlocValidSpelloutRules},
    {kindSpelloutNumbering, mlocValidSpelloutRules},
    {kindSpelloutSystems, mlocValidSpelloutSystems},
    {kindSpelloutTitlecase, mlocValidSpelloutTitlecase},
    {kindNameIds, mlocValidNameIds},
    {kindNamesLanguages, mlocValidNames},
    {kindNamesScripts, mlocValidNames},
    {kindNamesRegions, mlocValidNames},
    {kindNamesVariants, mlocValidNames},
    {kindNamesKeys, mlocValidNames},
    {kindNamePatterns, mlocValidNames},
};

const DataKind* const mlocDataKinds = s_kinds;
const uint32_t mlocDataKindCount = sizeof(s_kinds) / sizeof(s_kinds[0]);
