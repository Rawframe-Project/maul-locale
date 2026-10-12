// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit identifiers (UTS #35 Part 2, Unit Identifiers; record mloc-0023):
// a core unit (products of single units, "per" once or more, unit
// constants) or a mixed unit ("foot-and-inch"), parsed against the unit
// table with its deprecated aliases replaced.

#ifndef MLOC_UNIT_ID_H
#define MLOC_UNIT_ID_H

#include "unit_data.h"

#include "maul-locale/base.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // The single units a core unit holds, and the parts of a mixed unit.
    unitMaxFactors = 8,
    unitMaxParts = 4,
    // The longest identifier read.
    unitMaxIdLength = 255,
};

// A single unit: a simple unit (or, for a currency unit, the currency's
// packed code), its prefix and its power, negative in the denominator.
typedef struct UnitPart
{
    uint16_t simple;
    int8_t prefix;
    int8_t power;
    uint8_t currency;
} UnitPart;

// A parsed identifier: a core unit's single units in the order given,
// with the unit constants multiplied into its numerator and its
// denominator; or a mixed unit's parts, each a single unit of positive
// power, in the order given.
typedef struct UnitId
{
    UnitPart parts[unitMaxFactors];
    uint64_t numeratorConstant;
    uint64_t denominatorConstant;
    uint8_t count;
    uint8_t mixed;
} UnitId;

// Parses an identifier. Returns mloc_success; mloc_errorSyntax for one
// that is not well-formed; mloc_errorNotFound for a simple unit the
// table lacks or a private-use unit; mloc_errorCapacity for more single
// units, parts or constant than the limits hold.
mlocResult mlocParseUnitId(const UnitTable* t, const char* text, size_t length, UnitId* out);

#endif
