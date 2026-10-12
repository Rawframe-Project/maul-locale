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

// The number part: the numeric numbering systems and their digits, a unit
// of no locale (record mloc-0021, src/number_tables.h).
constexpr uint32_t kindNumberSystems = 0x00040001;
// The number part: the distinct symbol sets, a unit of no locale.
constexpr uint32_t kindNumberSymbols = 0x00040002;
// The number part: the distinct compiled patterns, a unit of no locale.
constexpr uint32_t kindNumberPatterns = 0x00040003;
// The number part: a locale's numbering systems, grouping and the indexes
// of its symbols, patterns and compact tables, a unit per locale.
constexpr uint32_t kindNumbers = 0x00040004;
// The number part: the distinct compact patterns, a unit of no locale.
constexpr uint32_t kindNumberCompactPatterns = 0x00040005;
// The number part: the distinct compact tables, a unit of no locale.
constexpr uint32_t kindNumberCompact = 0x00040006;
// The number part: the currencies' digits and roundings, a unit of no
// locale (record mloc-0022, src/currency_tables.h).
constexpr uint32_t kindCurrencyFractions = 0x00040007;
// The number part: the distinct currency pattern sets, a unit of no
// locale.
constexpr uint32_t kindCurrencyPatterns = 0x00040008;
// The number part: the currency symbols a locale key gives of its own, a
// unit per locale.
constexpr uint32_t kindCurrencySymbols = 0x00040009;

#endif
