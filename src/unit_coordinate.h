// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Angles written as coordinates ("12°N", "40° 26′ 46″ N"): the amount's
// sign chooses the locale's direction pattern, the amount unsigned within
// it.

#ifndef MLOC_UNIT_COORDINATE_H
#define MLOC_UNIT_COORDINATE_H

#include "unit_patterns.h"

#include <stddef.h>

// Fills a formatter's coordinate: checks that its unit (each part of a
// mixed one) is an angle and takes the locale's direction patterns of
// the width (numeric as narrow). Returns mloc_success; mloc_errorInvalid
// for a unit that is not an angle or a usage; mloc_errorNotFound for
// data without the unit table or the patterns.
mlocResult mlocInitCoordinate(UnitFormat* u, const mlocDataSet* data, const mlocLocale* locale,
                              const char* unit, size_t length, const mlocUnitOptions* options);

// Whether a formatter's coordinate is as mlocInitCoordinate leaves it.
bool mlocValidCoordinate(const UnitFormat* u);

// Formats an amount as a coordinate: mloc_success; mloc_errorCapacity
// when the text does not fit or the amount passes the limits.
mlocResult mlocFormatCoordinate(const UnitFormat* u, const mlocDecimal* decimal,
                                mlocOutput* output);

#endif
