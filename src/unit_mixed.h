// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Values moved between units exactly, and a value split across a mixed
// unit's parts (UTS #35 Part 6, Mixed Units): shared by the preferences
// and the unit formatter.

#ifndef MLOC_UNIT_MIXED_H
#define MLOC_UNIT_MIXED_H

#include "unit_convert.h"
#include "unit_data.h"
#include "unit_id.h"

#include "maul-locale/unit.h"

// A single part of a mixed unit as a core unit.
UnitId mlocUnitPartOf(const UnitId* mixed, uint32_t i);

// Whether two scales have the same dimensions, or opposite ones
// (*reciprocalOut then true).
bool mlocSameDimensions(const UnitScale* a, const UnitScale* b, bool* reciprocalOut);

// A value in a target unit, exactly; false when the integers cannot hold
// it or the units do not convert.
bool mlocUnitValueIn(const UnitTable* t, const UnitScale* from, const UnitValue* value,
                     const UnitScale* to, UnitValue* out);

// Whether a mixed unit's parts convert to each other alone, and the index
// of its smallest part: mloc_success, or mloc_errorInvalid for parts of
// other dimensions or of one size, an offset (a temperature) or Beaufort.
mlocResult mlocCheckMixed(const UnitTable* t, const UnitId* mixed, uint32_t* smallestOut);

// Rounds the last part's amount in place, given whether it is exact.
typedef mlocResult (*MixedRound)(void* context, mlocDecimal* amount, bool exact);

// Splits a value across a mixed unit's parts, in the order given: whole
// larger parts, the rest in the smallest, which alone is rounded; a
// rounded part that reaches one of the part above carries. The parts
// are taken largest first whatever their order. The value is in the
// input's unit, or the largest part's when input is null; NaN and the
// infinities go to the largest part, the others 0. The first part given
// carries the sign of a finite value. Returns mloc_success; mloc_errorCapacity when the
// integers cannot hold a part; mloc_errorInvalid for parts that do not
// convert to each other alone (an offset or Beaufort); round's error.
mlocResult mlocSplitMixed(const UnitTable* t, const UnitId* mixed, const UnitScale* input,
                          const UnitValue* value, bool nan, MixedRound round, void* context,
                          mlocUnitAmounts* out);

#endif
