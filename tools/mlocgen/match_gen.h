// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The distance table of language matching (record mloc-0017) from
// supplemental/languageInfo.xml, built as ICU's cldr-to-icu tool builds
// it (LocaleDistanceMapper and DistanceTable): the written_new rules in
// order, each one way or both, a rule with a "*" also put under every
// matching pair already there, the first distance for a pair kept; then
// the default script and region tables pruned. The layout is
// src/locale_tables.h's.

#ifndef MLOCGEN_MATCH_GEN_H
#define MLOCGEN_MATCH_GEN_H

#include "datafile.h"

// Adds the match unit. Returns 0 and a message on failure.
int GenerateMatchUnit(const char* commonDirectory, DataFile* file, char* errorOut,
                      size_t errorCapacity);

#endif
