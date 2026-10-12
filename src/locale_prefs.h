// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validator of the regional preferences table, for the kind list.

#ifndef MLOC_LOCALE_PREFS_H
#define MLOC_LOCALE_PREFS_H

#include "data_view.h"

bool mlocValidPreferences(const DataView* view, DataSpan payload);

#endif
