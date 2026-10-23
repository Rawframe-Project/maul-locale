// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Unit name patterns: a unit's patterns per plural category, those of a
// unit CLDR names, or built from the compound patterns as ICU's
// LongNameHandler::forArbitraryUnit builds them.

#ifndef MLOC_UNIT_COMPOUND_H
#define MLOC_UNIT_COMPOUND_H

#include "maul-locale/locale.h"

#include <stddef.h>
#include <stdint.h>

enum
{
    // The bytes of a pattern built.
    unitPatternRoom = 256,
    unitCategoryCount = 6,
};

typedef struct UnitPattern
{
    char text[unitPatternRoom];
    uint16_t length;
    bool present;
} UnitPattern;

// A unit's patterns: one per plural category zero to other, present
// where the data or the building gives one (a category not present takes
// other's), and the per-unit pattern of a unit CLDR names.
typedef struct UnitForms
{
    UnitPattern form[unitCategoryCount];
    UnitPattern per;
} UnitForms;

// A unit's patterns in a width (kindUnitNamesLong and the others) and,
// in the long width, a grammatical case (unitCaseNominative and the
// others; one out of range is the nominative), a unit CLDR names taken
// as it is (named: mlocNormalUnitName found it named; else the unit is a
// normal identifier), others built with the case derivations:
// mloc_success; the errors of mlocNormalizeUnitName; mloc_errorNotFound
// for names the data lacks (a simple unit without names, a power past
// three); mloc_errorInvalid for a pattern with the number inside a unit
// name ICU refuses to build; mloc_errorCapacity for a pattern past
// unitPatternRoom.
mlocResult mlocBuildUnitForms(const mlocDataSet* data, const mlocLocale* locale, const char* unit,
                              size_t length, bool named, uint32_t kind, uint32_t gcase,
                              UnitForms* out);

#endif
