// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit kinds the library reads, each with its part's validator,
// sorted by kind. A part adds its kinds here as it is built.

#include "data_view.h"
#include "kinds.h"
#include "locale_canonical.h"
#include "locale_distance.h"
#include "locale_fallback.h"
#include "locale_likely.h"

static const DataKind s_kinds[] = {
    {kindLocaleLikely, mlocValidLikely},
    {kindLocaleAliases, mlocValidAliases},
    {kindLocaleExtensionAliases, mlocValidExtensionAliases},
    {kindLocaleParents, mlocValidParents},
    {kindLocaleMatch, mlocValidMatch},
};

const DataKind* const mlocDataKinds = s_kinds;
const uint32_t mlocDataKindCount = sizeof(s_kinds) / sizeof(s_kinds[0]);
