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
// The locale part: a locale's context transforms, a unit per locale key
// (src/locale_context.h).
constexpr uint32_t kindLocaleContext = 0x00010007;
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
// The number part: the currency names a locale key gives of its own, a
// unit per locale.
constexpr uint32_t kindCurrencyNames = 0x0004000A;
// The number part: the currencies each region uses and used, a unit of no
// locale.
constexpr uint32_t kindCurrencyRegions = 0x0004000B;
// The number part: the unit identifiers' parts and their conversion to
// base units, a unit of no locale (record mloc-0023, src/unit_tables.h).
constexpr uint32_t kindUnits = 0x0004000C;
// The number part: the units each usage prefers by region, a unit of no
// locale (record mloc-0023).
constexpr uint32_t kindUnitPreferences = 0x0004000D;
// The unit names data file: the identifiers it names, a unit of no
// locale; the names of each width and the long width's extras, units per
// locale (record mloc-0023, src/unit_tables.h).
constexpr uint32_t kindUnitIds = 0x0004000E;
constexpr uint32_t kindUnitNamesLong = 0x0004000F;
constexpr uint32_t kindUnitNamesShort = 0x00040010;
constexpr uint32_t kindUnitNamesNarrow = 0x00040011;
constexpr uint32_t kindUnitNameExtras = 0x00040012;
constexpr uint32_t kindUnitDerivations = 0x00040013;
// The unit names data file: a locale's duration and coordinate patterns,
// a unit per locale.
constexpr uint32_t kindUnitPatterns = 0x00040014;
// The unit names data file: a locale's long patterns of the grammatical
// cases, a unit per locale.
constexpr uint32_t kindUnitCases = 0x00040015;
// The unit names data file: those patterns lowercased, a unit per locale.
constexpr uint32_t kindUnitCasesLowered = 0x00040016;
// The number part: the sets number parsing reads, a unit of no locale
// (src/parse_tables.h).
constexpr uint32_t kindParseSets = 0x00040017;

// The list part: the distinct list pattern sets, a unit of no locale
// (src/list_tables.h).
constexpr uint32_t kindListPatterns = 0x00050001;
// The list part: a locale's set of each list style, a unit per locale.
constexpr uint32_t kindLists = 0x00050002;
// The spell-out data file: a locale's spell-out rules, ordinal-suffix
// rules and numbering-system rules, a unit per locale key whose file has
// the group (record mloc-0034, src/spellout_tables.h).
constexpr uint32_t kindSpelloutRules = 0x00060001;
constexpr uint32_t kindSpelloutOrdinals = 0x00060002;
constexpr uint32_t kindSpelloutNumbering = 0x00060003;
// The spell-out data file: the algorithmic numbering systems, a unit of
// no locale.
constexpr uint32_t kindSpelloutSystems = 0x00060004;
// The spell-out data file: the titlecase of the rules' lowercase letters,
// a unit of no locale.
constexpr uint32_t kindSpelloutTitlecase = 0x00060005;
// The names data file: the codes it names, a unit of no locale; the
// names a locale key gives of languages, scripts, regions, variants,
// keys and their values, and its display patterns, units per locale key
// (record mloc-0036, src/names_tables.h).
constexpr uint32_t kindNameIds = 0x00070001;
constexpr uint32_t kindNamesLanguages = 0x00070002;
constexpr uint32_t kindNamesScripts = 0x00070003;
constexpr uint32_t kindNamesRegions = 0x00070004;
constexpr uint32_t kindNamesVariants = 0x00070005;
constexpr uint32_t kindNamesKeys = 0x00070006;
constexpr uint32_t kindNamePatterns = 0x00070007;

#endif
