// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Fallback for the parts (record mloc-0016): the parents table's
// validator, and the lookup of a part's unit along a locale's chain.

#ifndef MLOC_LOCALE_FALLBACK_H
#define MLOC_LOCALE_FALLBACK_H

#include "data_view.h"

#include "maul-locale/locale.h"

bool mlocValidParents(const DataView* view, DataSpan payload);

// Finds the unit of a kind for a locale: its key, then each key of its
// chain in the component, each in every source of the set in order.
// *fellBackOut tells whether the unit is not the locale's own key's;
// *viewOut, when not null, receives the source that holds it, whose
// other units the unit's indexes name. False when no step has it, or
// when the chain is needed and the data has no parents table.
bool mlocFindLocaleUnit(const mlocDataSet* data, mlocComponent component, uint32_t kind,
                        const mlocLocale* locale, DataSpan* spanOut, bool* fellBackOut,
                        DataView* viewOut);

// Called with each unit of a walk along a locale's chain, the source
// that holds it and its step (0 for the locale's own key); returns true
// to end the walk.
typedef bool (*LocaleUnitVisitor)(void* context, DataSpan unit, const DataView* view, int depth);

// Visits the units of a kind along a locale's chain, as
// mlocFindLocaleUnit finds the first: each key of its chain in the
// component, each in every source of the set in order, until the visitor
// ends the walk. False when the chain is needed and the data has no
// parents table.
bool mlocVisitLocaleUnits(const mlocDataSet* data, mlocComponent component, uint32_t kind,
                          const mlocLocale* locale, LocaleUnitVisitor visit, void* context);

#endif
