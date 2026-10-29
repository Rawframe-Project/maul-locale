// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Canonicalization's parts for one another: the validator of the
// extension aliases (the alias table's is in locale_aliases.h), the
// aliases applied to a locale's language id, and the canonicalization
// of its extensions.

#ifndef MLOC_LOCALE_CANONICAL_H
#define MLOC_LOCALE_CANONICAL_H

#include "data_view.h"

#include "maul-locale/locale.h"

bool mlocValidExtensionAliases(const DataView* view, DataSpan payload);

// Applies the alias rules to a locale's language, script, region and
// variants, adding what changed to *changes; the extensions are kept.
mlocResult mlocApplyAliases(const mlocDataSet* data, mlocLocale* locale, uint32_t* changes);

// Canonicalizes a locale's extensions: a t extension's language, and the
// values of u and t keys and of subdivisions by their aliases.
mlocResult mlocCanonicalizeExtensions(const mlocDataSet* data, mlocLocale* locale,
                                      uint32_t* changes);

#endif
