// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validator of the regional preferences table, for the kind list,
// and the region a locale's regional data comes from.

#ifndef MLOC_LOCALE_PREFS_H
#define MLOC_LOCALE_PREFS_H

#include "data_view.h"

#include "maul-locale/locale.h"

#include <stdint.h>

bool mlocValidPreferences(const DataView* view, DataSpan payload);

// The region of a locale's regional data by UTS #35's order, packed: the
// -u-rg key's, the locale's own, the -u-sd key's, the likely one, else
// 001; and the language (a likely one for und).
uint32_t mlocPreferenceRegion(const mlocDataSet* data, const mlocLocale* locale,
                              uint32_t* languageOut);

#endif
