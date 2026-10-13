// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit names (record mloc-0024, unit_tables.h): the validators of the
// identifiers, the names and the extras, and a unit's fields gathered
// along a locale's chain. A names unit's codes index the identifiers of
// its own source, so each unit of the walk is read with its source's.

#ifndef MLOC_UNIT_NAMES_H
#define MLOC_UNIT_NAMES_H

#include "data_view.h"
#include "unit_tables.h"

#include "maul-locale/locale.h"

#include <stddef.h>
#include <stdint.h>

bool mlocValidUnitIds(const DataView* view, DataSpan payload);
bool mlocValidUnitNames(const DataView* view, DataSpan payload);
bool mlocValidUnitNameExtras(const DataView* view, DataSpan payload);
bool mlocValidUnitDerivations(const DataView* view, DataSpan payload);

// The code of an identifier in a valid identifiers unit, and its kind
// (a unitId value); false when the unit lacks it.
bool mlocFindUnitNameId(DataSpan ids, const char* id, size_t length, uint16_t* codeOut,
                        uint8_t* kindOut);

// An identifier's kind in the first source of a set with the
// identifiers; false when none has them or that one lacks it.
bool mlocUnitNameKind(const mlocDataSet* data, const char* id, size_t length, uint8_t* kindOut);

// A unit's fields of a kind (a width's names, or the extras), each from
// the first key along the chain that gives it.
typedef struct UnitNameFields
{
    const uint8_t* text[unitNameFields];
    uint16_t length[unitNameFields];
    uint32_t found;
} UnitNameFields;

// Gathers an identifier's fields of a kind along a locale's chain; false
// when the chain cannot be walked.
bool mlocGatherUnitNames(const mlocDataSet* data, const mlocLocale* locale, uint32_t kind,
                         const char* id, size_t length, UnitNameFields* out);

// The field a plural category's pattern comes from: its own, else
// other's; unitNameFields when neither is found.
uint32_t mlocUnitPatternField(const UnitNameFields* fields, uint32_t category);

// The plural categories a structure (unitDerivePer and the others) gives
// its two components in a locale's language, root's for a language the
// derivations do not list: a category's index, unitDerivedCompound or
// unitDerivedNone. False when the data has no derivations.
bool mlocUnitDerivation(const mlocDataSet* data, const mlocLocale* locale, uint32_t structure,
                        uint8_t* value0Out, uint8_t* value1Out);

// Whether a patterns unit is a table of one record whose string indexes
// are in range, its coordinate patterns each holding "{0}" once.
bool mlocValidUnitPatterns(const DataView* view, DataSpan payload);

// A locale's duration and coordinate patterns (unit_tables.h's order).
typedef struct UnitPatternTexts
{
    const char* text[unitPatternCount];
    uint16_t length[unitPatternCount];
} UnitPatternTexts;

// Finds a locale's patterns along its main chain; false when the data has
// none or the chain cannot be walked.
bool mlocFindUnitPatterns(const mlocDataSet* data, const mlocLocale* locale, UnitPatternTexts* out);

#endif
