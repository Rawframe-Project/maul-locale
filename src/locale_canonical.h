// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validator of the aliases table, for the kind list.

#ifndef MLOC_LOCALE_CANONICAL_H
#define MLOC_LOCALE_CANONICAL_H

#include "data_view.h"

bool mlocValidAliases(const DataView* view, DataSpan payload);

#endif
