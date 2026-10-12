// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// The kinds of data units (record mloc-0014): the part in the high 16
// bits, the table in the low 16. Shared by the library and the
// generator; a kind's number never changes meaning within a data
// version.

#ifndef MLOC_KINDS_H
#define MLOC_KINDS_H

#include <stdint.h>

// The locale part: likely subtags, a unit of no locale.
constexpr uint32_t kindLocaleLikely = 0x00010001;
// The locale part: the aliases canonicalization replaces, a unit of no
// locale.
constexpr uint32_t kindLocaleAliases = 0x00010002;
// The locale part: the aliases of extension values, a unit of no locale.
constexpr uint32_t kindLocaleExtensionAliases = 0x00010003;
// The locale part: the explicit parent locales of each component, a unit
// of no locale.
constexpr uint32_t kindLocaleParents = 0x00010004;
// The locale part: the distance table of language matching, a unit of no
// locale.
constexpr uint32_t kindLocaleMatch = 0x00010005;
// The locale part: the regional preferences (hour cycle, week, measurement
// system, paper size), a unit of no locale.
constexpr uint32_t kindLocalePreferences = 0x00010006;
// The plural part: a locale's rule sets and range table, a unit per
// locale (record mloc-0020).
constexpr uint32_t kindPlurals = 0x00030001;
// The plural part: the range tables, a unit of no locale.
constexpr uint32_t kindPluralRanges = 0x00030002;

#endif
