// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit identifiers in their normal form: parsed into the unit table's
// single units, merged and sorted as ICU's MeasureUnit normalizes them,
// and written back.

#ifndef MLOC_UNIT_NORMAL_H
#define MLOC_UNIT_NORMAL_H

#include "unit_data.h"
#include "unit_id.h"

#include "maul-locale/data.h"

#include <stddef.h>
#include <stdint.h>

// The text of a simple unit, as identifiers write it.
void mlocSimpleUnitText(const UnitTable* t, uint16_t simple, const char** text, size_t* length);

// Parses an identifier into the data's unit table and normalizes it:
// kilogram as kilo- and gram, single units of one simple unit, prefix
// and side merged, then sorted. Returns mloc_success; the errors of
// mlocParseUnitId; mloc_errorInvalid for a mixed or currency unit, a
// constant in the numerator or a power past 15; mloc_errorNotFound for
// data without the unit table.
mlocResult mlocParseNormalUnit(const mlocDataSet* data, const char* unit, size_t length,
                               UnitTable* table, UnitId* id);

// Writes a normalized product as ICU's MeasureUnitImpl::serialize does
// ("per-" before the first denominator unit, a constant after it or at
// the end); false when it passes the room.
bool mlocWriteNormalUnit(const UnitTable* t, const UnitPart* parts, uint32_t count,
                         uint64_t constant, char* out, size_t room, size_t* lengthOut);

// Writes an identifier's normal form ("meter-per-second" for
// "per-second-meter", "tonne" for "metric-ton"). Returns mloc_success;
// mloc_errorSyntax or mloc_errorNotFound as mlocParseUnitId; mloc_errorInvalid
// for a mixed or currency unit or a constant in the numerator;
// mloc_errorCapacity when it passes the room; mloc_errorNotFound for data
// without the unit table.
mlocResult mlocNormalizeUnitName(const mlocDataSet* data, const char* unit, size_t length,
                                 char* out, size_t capacity, size_t* lengthOut);

#endif
