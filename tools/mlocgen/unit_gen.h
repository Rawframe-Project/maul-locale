// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The unit table (src/unit_tables.h) from supplemental/units.xml: each
// convertible unit's size in its base unit as an exact fraction (CLDR's
// expressions over its constants reduced), the prefixes, the aliases, the
// base units' dimensions and the Beaufort table.

#ifndef MLOCGEN_UNIT_GEN_H
#define MLOCGEN_UNIT_GEN_H

#include "datafile.h"

#include <stddef.h>

int GenerateUnitTable(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
