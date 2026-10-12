// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The validators of the plural units (record mloc-0020), for the kind
// list.

#ifndef MLOC_PLURAL_H
#define MLOC_PLURAL_H

#include "data_view.h"

bool mlocValidPlurals(const DataView* view, DataSpan payload);
bool mlocValidPluralRanges(const DataView* view, DataSpan payload);

#endif
