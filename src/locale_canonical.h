// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Canonicalization's parts for one another: the validators of its two
// tables, the aliases applied to a locale's language id, and the
// canonicalization of its extensions.

#ifndef MLOC_LOCALE_CANONICAL_H
#define MLOC_LOCALE_CANONICAL_H

#include "data_view.h"

#include "maul-locale/locale.h"

bool mlocValidAliases(const DataView* view, DataSpan payload);
bool mlocValidExtensionAliases(const DataView* view, DataSpan payload);

// Applies the alias rules to a locale's language, script, region and
// variants, adding what changed to *changes; the extensions are kept.
mlocResult mlocApplyAliases(const mlocDataSet* data, mlocLocale* locale, uint32_t* changes);

// Canonicalizes a locale's extensions: a t extension's language, and the
// values of u and t keys and of subdivisions by their aliases.
mlocResult mlocCanonicalizeExtensions(const mlocDataSet* data, mlocLocale* locale,
                                      uint32_t* changes);

#endif
