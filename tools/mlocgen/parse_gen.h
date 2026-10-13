// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The parse sets (src/parse_tables.h): root's parseLenients compiled from
// their UnicodeSet patterns, and ICU's fixed sets computed with Maul
// Unicode's properties.

#ifndef MLOCGEN_PARSE_GEN_H
#define MLOCGEN_PARSE_GEN_H

#include "cldr.h"
#include "datafile.h"

// Adds the parse sets' unit to a file; dies on data it cannot read.
void GenerateParseSets(Cldr* cldr, DataFile* file);

#endif
