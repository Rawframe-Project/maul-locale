// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The zone names data file (record mloc-0042, src/zone_name_tables.h):
// the metazone unit from supplemental/metaZones.xml and
// bcp47/timezone.xml, and per locale key the zone names of its
// timeZoneNames, each value CLDR resolves filed where the key's chain
// would give another, as the names are filed.

#ifndef MLOCGEN_ZONE_NAMES_GEN_H
#define MLOCGEN_ZONE_NAMES_GEN_H

#include "datafile.h"
#include "names_gen.h"

#include <stddef.h>

// Adds the units to the file. Returns 0 and a message on a file that
// cannot be read; fails the program on data the layout cannot hold (a
// metazone period of a zone or metazone CLDR's ids lack, periods out of
// order, a name of a control byte or a format without its placeholders).
int GenerateZoneNames(const NamesInput* input, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
